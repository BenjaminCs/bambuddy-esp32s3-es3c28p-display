// Wi-Fi + Bambuddy REST client, running in its own FreeRTOS task on core 0.
//
// Polling schedule (all GET, all with the X-API-Key header):
//   every 60 s  GET /api/v1/printers/                         -> printer list
//   every  3 s  GET /api/v1/printers/{id}/status              -> live state per printer
//   every  6 s  GET /api/v1/queue/?status=pending&printer_id= -> pending jobs per printer
// Commands from the UI are executed immediately and trigger a forced refresh.
//
// The full status response is ~2.5 kB and the queue list ~1.6 kB per item, so every
// response is parsed through an ArduinoJson filter that keeps only the fields we show.

#include "net.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <vector>

#include "certs.h"
#include "lwip/dns.h"
#include "secrets.h"
#include "ui_strings.h"

static const uint32_t PRINTERS_EVERY_MS = 60000;
static const uint32_t STATUS_EVERY_MS = 3000;
static const uint32_t QUEUE_EVERY_MS = 6000;

static AppState g_state;          // written by the net task, guarded by g_lock
static SemaphoreHandle_t g_lock;
static QueueHandle_t g_cmds;      // UI -> net task
static QueueHandle_t g_results;   // net task -> UI

// One TLS client + one HTTPClient, reused with keep-alive. A fresh TLS handshake
// costs ~1 s on the ESP32-S3, so reuse matters at a 3 s poll interval.
static WiFiClientSecure g_tls;
static HTTPClient g_http;

static void set_error(const char *msg) {
    xSemaphoreTake(g_lock, portMAX_DELAY);
    if (strcmp(g_state.error, msg) != 0) {
        strlcpy(g_state.error, msg, sizeof(g_state.error));
        g_state.version++;
    }
    xSemaphoreGive(g_lock);
}

// Optional DNS override (DNS_SERVER_IP in secrets.h). Needed when the Bambuddy
// hostname only resolves to the LAN address on a local DNS (e.g. Pi-hole) and the
// router's DNS would return the public IP, which is unreachable from inside the
// LAN without NAT hairpinning. Both DNS slots are overwritten so lwIP never falls
// back to the DHCP-provided router DNS.
static void apply_dns_override() {
#ifdef DNS_SERVER_IP
    static const uint8_t ip[] = {DNS_SERVER_IP};
    ip_addr_t dns;
    // IP_ADDR4 is a macro that counts its arguments before DNS_SERVER_IP expands,
    // so the octets must go through an array.
    IP_ADDR4(&dns, ip[0], ip[1], ip[2], ip[3]);
    dns_setserver(0, &dns);
    dns_setserver(1, &dns);
#endif
}

// One API request. Returns the HTTP status, 0 on network/TLS failure, or -100 when a
// 2xx body could not be parsed. If `out` is given, the body is parsed into it
// (through `filter` if given); `raw_body` receives the unparsed body (used for
// error details).
static int api(const char *method, const String &path, JsonDocument *out, const JsonDocument *filter,
               String *raw_body = nullptr) {
    if (!g_http.begin(g_tls, BAMBUDDY_HOST, 443, path, true)) return 0;
    g_http.addHeader("X-API-Key", BAMBUDDY_API_KEY);
    g_http.addHeader("Accept", "application/json");
    // POSTs here have no body; Bambuddy's endpoints take parameters in the path/query.
    int code = g_http.sendRequest(method, (uint8_t *)nullptr, 0);
    if (code <= 0) {
        g_http.end();  // do not reuse a broken connection
        return 0;
    }
    // getString() handles chunked transfer encoding, which stream parsing would not.
    String body = g_http.getString();
    if (out) {
        DeserializationError err = filter ? deserializeJson(*out, body, DeserializationOption::Filter(*filter))
                                          : deserializeJson(*out, body);
        if (err && code < 300) code = -100;
    }
    if (raw_body) *raw_body = body;
    g_http.end();
    return code;
}

static void copy_str(char *dst, size_t n, JsonVariantConst v, const char *fallback = "") {
    strlcpy(dst, v.is<const char *>() ? v.as<const char *>() : fallback, n);
}

static void fetch_printers() {
    JsonDocument filter;
    filter[0]["id"] = true;
    filter[0]["name"] = true;
    filter[0]["model"] = true;
    filter[0]["is_active"] = true;
    JsonDocument doc;
    int code = api("GET", "/api/v1/printers/", &doc, &filter);
    if (code != 200) {
        set_error(code == 401 || code == 403 ? STR_ERR_API_KEY : STR_ERR_UNREACHABLE);
        xSemaphoreTake(g_lock, portMAX_DELAY);
        g_state.api_ok = false;
        g_state.version++;
        xSemaphoreGive(g_lock);
        return;
    }
    xSemaphoreTake(g_lock, portMAX_DELAY);
    int n = 0;
    for (JsonObjectConst p : doc.as<JsonArrayConst>()) {
        if (n >= MAX_PRINTERS) break;
        if (p["is_active"].is<bool>() && !p["is_active"].as<bool>()) continue;  // disabled in Bambuddy
        PrinterInfo &pi = g_state.printers[n];
        int id = p["id"] | 0;
        if (pi.id != id) {  // slot now holds a different printer: drop stale data
            memset(&pi, 0, sizeof(pi));
            g_state.queue_count[n] = 0;
        }
        pi.id = id;
        copy_str(pi.name, sizeof(pi.name), p["name"], STR_PRINTER);
        copy_str(pi.model, sizeof(pi.model), p["model"]);
        n++;
    }
    g_state.printer_count = n;
    g_state.api_ok = true;
    g_state.error[0] = 0;
    g_state.version++;
    xSemaphoreGive(g_lock);
}

static void fetch_status(int idx, int printer_id) {
    JsonDocument filter;
    for (const char *k : {"connected", "state", "subtask_name", "gcode_file", "progress", "remaining_time", "layer_num",
                          "total_layers", "temperatures", "awaiting_plate_clear"})
        filter[k] = true;
    filter["hms_errors"][0]["code"] = true;  // only the count is shown
    JsonDocument doc;
    int code = api("GET", "/api/v1/printers/" + String(printer_id) + "/status", &doc, &filter);

    xSemaphoreTake(g_lock, portMAX_DELAY);
    PrinterInfo &p = g_state.printers[idx];
    if (p.id == printer_id) {
        p.status_ok = code == 200;
        if (code == 200) {
            p.connected = doc["connected"] | false;
            copy_str(p.state, sizeof(p.state), doc["state"], "?");
            const char *job = doc["subtask_name"].is<const char *>() ? doc["subtask_name"].as<const char *>()
                                                                     : (doc["gcode_file"] | "");
            strlcpy(p.job, job, sizeof(p.job));
            p.progress = doc["progress"] | 0.0f;            // 0-100
            p.remaining_min = doc["remaining_time"] | 0;    // minutes (Bambu mc_remaining_time)
            p.layer = doc["layer_num"] | 0;
            p.total_layers = doc["total_layers"] | 0;
            JsonObjectConst t = doc["temperatures"];
            p.nozzle = t["nozzle"] | 0.0f;
            p.nozzle_target = t["nozzle_target"] | 0.0f;
            p.bed = t["bed"] | 0.0f;
            p.bed_target = t["bed_target"] | 0.0f;
            // True after a finished/failed print until someone confirms the plate is
            // empty; Bambuddy's scheduler will not start the next job before that.
            p.awaiting_plate_clear = doc["awaiting_plate_clear"] | false;
            p.hms_count = doc["hms_errors"].size();
        }
        g_state.version++;
    }
    xSemaphoreGive(g_lock);
}

static void fetch_queue(int idx, int printer_id) {
    JsonDocument filter;
    for (const char *k : {"id", "position", "manual_start", "filament_short", "waiting_reason", "archive_name",
                          "library_file_name", "print_time_seconds", "filament_used_grams", "filament_type", "status"})
        filter[0][k] = true;
    JsonDocument doc;
    int code = api("GET", "/api/v1/queue/?status=pending&printer_id=" + String(printer_id), &doc, &filter);
    if (code != 200) return;

    std::vector<QueueItem> items;
    for (JsonObjectConst q : doc.as<JsonArrayConst>()) {
        if (strcmp(q["status"] | "", "pending") != 0) continue;
        QueueItem it = {};
        it.id = q["id"] | 0;
        it.position = q["position"] | 0;
        // Items come either from the archive or from the file library.
        const char *name = q["archive_name"].is<const char *>() ? q["archive_name"].as<const char *>()
                                                                : (q["library_file_name"] | STR_UNNAMED);
        strlcpy(it.name, name, sizeof(it.name));
        it.print_s = q["print_time_seconds"] | 0;
        it.grams = q["filament_used_grams"] | 0.0f;
        copy_str(it.filament, sizeof(it.filament), q["filament_type"]);
        it.manual_start = q["manual_start"] | false;
        it.filament_short = q["filament_short"] | false;
        copy_str(it.waiting, sizeof(it.waiting), q["waiting_reason"]);
        items.push_back(it);
    }
    // The API does not promise an order; show the queue in Bambuddy's position order.
    std::sort(items.begin(), items.end(),
              [](const QueueItem &a, const QueueItem &b) { return a.position < b.position; });
    int n = min((int)items.size(), MAX_QUEUE);

    xSemaphoreTake(g_lock, portMAX_DELAY);
    if (g_state.printers[idx].id == printer_id) {
        memcpy(g_state.queue[idx], items.data(), sizeof(QueueItem) * n);
        g_state.queue_count[idx] = n;
        g_state.version++;
    }
    xSemaphoreGive(g_lock);
}

// FastAPI errors look like {"detail": "..."}; 409 filament errors carry an object instead.
static void error_detail(const String &body, char *out, size_t n) {
    JsonDocument doc;
    if (!deserializeJson(doc, body) && doc["detail"].is<const char *>())
        strlcpy(out, doc["detail"].as<const char *>(), n);
}

static void run_command(const Command &cmd) {
    CommandResult res = {};
    res.cmd = cmd;
    String pid = String(cmd.printer_id);
    String body;
    int code = 0;

    switch (cmd.type) {
        case CMD_PAUSE:  // needs the "control printer" key permission
            code = api("POST", "/api/v1/printers/" + pid + "/print/pause", nullptr, nullptr, &body);
            break;
        case CMD_RESUME:
            code = api("POST", "/api/v1/printers/" + pid + "/print/resume", nullptr, nullptr, &body);
            break;
        case CMD_STOP:
            code = api("POST", "/api/v1/printers/" + pid + "/print/stop", nullptr, nullptr, &body);
            break;
        case CMD_CLEAR_PLATE:  // Bambuddy-side flag only, nothing is sent to the printer
            code = api("POST", "/api/v1/printers/" + pid + "/clear-plate", nullptr, nullptr, &body);
            break;
        case CMD_CLEAR_AND_START:
            code = api("POST", "/api/v1/printers/" + pid + "/clear-plate", nullptr, nullptr, &body);
            // 400 = the printer was no longer waiting for the acknowledgement; the job can still start.
            if (code != 200 && code != 400) break;
            // fallthrough
        case CMD_START_ITEM:
            // Clears the item's manual_start flag; Bambuddy's scheduler then sends it to
            // the printer as soon as the printer is idle (needs the "queue" permission).
            // Returns 409 when the assigned spool looks too short, unless skipped.
            code = api("POST",
                       "/api/v1/queue/" + String(cmd.item_id) +
                           "/start?skip_filament_check=" + (cmd.skip_filament_check ? "true" : "false"),
                       nullptr, nullptr, &body);
            break;
    }

    res.http_code = code;
    if (code >= 300) error_detail(body, res.message, sizeof(res.message));
    xQueueSend(g_results, &res, 0);
}

static void net_task(void *) {
    uint32_t last_printers = 0, last_status = 0, last_queue = 0;
    bool force = true;

    for (;;) {
        bool wifi = WiFi.status() == WL_CONNECTED;
        if (wifi != g_state.wifi) {
            xSemaphoreTake(g_lock, portMAX_DELAY);
            g_state.wifi = wifi;
            g_state.version++;
            xSemaphoreGive(g_lock);
            if (wifi) {
                apply_dns_override();
                force = true;
            }
        }
        if (!wifi) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Waiting on the command queue doubles as the loop's idle delay.
        Command cmd;
        if (xQueueReceive(g_cmds, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            run_command(cmd);
            force = true;
            continue;
        }

        uint32_t now = millis();
        if (force || !g_state.api_ok || now - last_printers > PRINTERS_EVERY_MS) {
            fetch_printers();
            last_printers = now;
            if (!g_state.api_ok) {
                vTaskDelay(pdMS_TO_TICKS(3000));  // back off while Bambuddy is unreachable
                continue;
            }
        }

        // Only this task writes the printer list, so reading it without the lock is safe here.
        int count = g_state.printer_count;
        if (force || now - last_status > STATUS_EVERY_MS) {
            for (int i = 0; i < count; i++) fetch_status(i, g_state.printers[i].id);
            last_status = millis();
        }
        if (force || now - last_queue > QUEUE_EVERY_MS) {
            for (int i = 0; i < count; i++) fetch_queue(i, g_state.printers[i].id);
            last_queue = millis();
        }
        force = false;
    }
}

void net_begin() {
    g_lock = xSemaphoreCreateMutex();
    g_cmds = xQueueCreate(4, sizeof(Command));
    g_results = xQueueCreate(4, sizeof(CommandResult));

    // Certificate validation against the bundled roots (src/certs.h). Do not replace
    // this with setInsecure(): the API key would then go to anyone who can spoof DNS.
    g_tls.setCACert(ROOT_CA_PEM);
    g_http.setReuse(true);
    g_http.setTimeout(10000);
    g_http.setConnectTimeout(5000);

    WiFi.mode(WIFI_STA);  // the ESP32-S3 only supports 2.4 GHz networks
    WiFi.setHostname(DEVICE_HOSTNAME);
    WiFi.setAutoReconnect(true);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) { apply_dns_override(); }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // 16 kB stack: TLS + JSON parsing need more than the default 8 kB.
    xTaskCreatePinnedToCore(net_task, "net", 16384, nullptr, 1, nullptr, 0);
}

bool net_snapshot(AppState &out, uint32_t last_version) {
    xSemaphoreTake(g_lock, portMAX_DELAY);
    bool changed = g_state.version != last_version;
    if (changed) out = g_state;
    xSemaphoreGive(g_lock);
    return changed;
}

void net_send(const Command &cmd) { xQueueSend(g_cmds, &cmd, 0); }

bool net_poll_result(CommandResult &out) { return xQueueReceive(g_results, &out, 0) == pdTRUE; }
