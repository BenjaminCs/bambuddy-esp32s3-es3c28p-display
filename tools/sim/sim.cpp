// Host-side screenshot generator: runs the real src/ui.cpp against mock Bambuddy
// state, drives it with scripted touches and dumps the framebuffer as PPM.
//   ./sim <scenario> <out.ppm>

#include <lvgl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "model.h"
#include "net.h"
#include "ui.h"

// ---------------------------------------------------------------- mock net

static AppState g;
static CommandResult pending_result;
static bool has_result = false;

void net_begin() {}
bool net_snapshot(AppState &out, uint32_t v) {
    if (g.version == v) return false;
    out = g;
    return true;
}
void net_send(const Command &) {}
bool net_poll_result(CommandResult &out) {
    if (!has_result) return false;
    out = pending_result;
    has_result = false;
    return true;
}

static void add_queue(int p, int id, const char *name, int min, float g_, const char *fil, bool manual = true,
                      bool shortf = false) {
    QueueItem &q = g.queue[p][g.queue_count[p]++];
    memset(&q, 0, sizeof(q));
    q.id = id;
    q.position = g.queue_count[p];
    strlcpy(q.name, name, sizeof(q.name));
    q.print_s = min * 60;
    q.grams = g_;
    strlcpy(q.filament, fil, sizeof(q.filament));
    q.manual_start = manual;
    q.filament_short = shortf;
}

static void base_state() {
    memset(&g, 0, sizeof(g));
    g.wifi = true;
    g.api_ok = true;
    g.printer_count = 2;
    PrinterInfo &a = g.printers[0];
    a.id = 1;
    strlcpy(a.name, "A1 Mini", sizeof(a.name));
    strlcpy(a.model, "A1 Mini", sizeof(a.model));
    a.status_ok = a.connected = true;
    strlcpy(a.state, "RUNNING", sizeof(a.state));
    strlcpy(a.job, "Benchy_plate_1", sizeof(a.job));
    a.progress = 72;
    a.remaining_min = 18;
    a.layer = 41;
    a.total_layers = 67;
    a.nozzle = 220; a.nozzle_target = 220; a.bed = 55; a.bed_target = 55;

    PrinterInfo &x = g.printers[1];
    x.id = 2;
    strlcpy(x.name, "X2D", sizeof(x.name));
    strlcpy(x.model, "X2D", sizeof(x.model));
    x.status_ok = x.connected = true;
    strlcpy(x.state, "IDLE", sizeof(x.state));
    x.nozzle = 31; x.bed = 28;

    add_queue(0, 11, "Cable_clips_x8", 47, 12.4f, "PLA");
    add_queue(0, 12, "Phone_stand", 136, 58.2f, "PETG");
    add_queue(0, 13, "Gridfinity_bin_2x3", 94, 41.0f, "PLA");
    add_queue(0, 14, "Spool_holder_arm", 182, 76.5f, "PETG", true, true);
    add_queue(0, 15, "Keychain_tag", 12, 3.1f, "PLA", false);
    add_queue(1, 21, "Enclosure_hinge_L", 75, 22.0f, "ABS");
    add_queue(1, 22, "Enclosure_hinge_R", 75, 22.0f, "ABS");
    g.version = 1;
}

// ---------------------------------------------------------------- display + input

static uint16_t fb[320 * 240];
static uint32_t now_ms = 0;
static bool touch_down = false;
static int touch_x = 0, touch_y = 0;

static uint32_t tick() { return now_ms; }
static void flush_cb(lv_display_t *d, const lv_area_t *, uint8_t *) { lv_display_flush_ready(d); }
static void read_cb(lv_indev_t *, lv_indev_data_t *data) {
    data->point.x = touch_x;
    data->point.y = touch_y;
    data->state = touch_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void run(int ms) {
    for (int t = 0; t < ms; t += 5) {
        now_ms += 5;
        ui_tick();
        lv_timer_handler();
    }
}

static void tap(int x, int y) {
    touch_x = x;
    touch_y = y;
    touch_down = true;
    run(120);
    touch_down = false;
    run(400);
}

// Finds the clickable object whose label text starts with/contains `text`.
static lv_obj_t *find_text(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class)) {
        const char *t = lv_label_get_text(root);
        if (t && strstr(t, text)) return root;
    }
    uint32_t n = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *r = find_text(lv_obj_get_child(root, i), text);
        if (r) return r;
    }
    return nullptr;
}

static void tap_text(const char *text) {
    lv_obj_t *o = find_text(lv_layer_top(), text);
    if (!o) o = find_text(lv_screen_active(), text);
    if (!o) {
        fprintf(stderr, "text not found: %s\n", text);
        exit(2);
    }
    lv_obj_t *target = lv_obj_get_parent(o);  // the button around the label
    lv_area_t a;
    lv_obj_get_coords(target, &a);
    tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
}

static void tap_tab(int idx) { tap(idx == 0 ? 80 : 240, 51); }
static void tap_printer(int idx) { tap(idx == 0 ? 71 : 213, 18); }

static void save(const char *path) {
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n320 240\n255\n");
    for (int i = 0; i < 320 * 240; i++) {
        uint16_t c = fb[i];
        unsigned char rgb[3] = {(unsigned char)((c >> 11 & 0x1F) * 255 / 31), (unsigned char)((c >> 5 & 0x3F) * 255 / 63),
                                (unsigned char)((c & 0x1F) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

// ---------------------------------------------------------------- scenarios

int main(int argc, char **argv) {
    if (argc < 3) return 1;
    std::string sc = argv[1];

    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *disp = lv_display_create(320, 240);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, fb, nullptr, sizeof(fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, read_cb);

    base_state();
    PrinterInfo &a = g.printers[0];

    if (sc == "connecting") {
        memset(&g, 0, sizeof(g));
        g.version = 1;
    } else if (sc == "finished" || sc == "confirm_next" || sc == "started" || sc == "low_filament") {
        strlcpy(a.state, "FINISH", sizeof(a.state));
        a.progress = 100;
        a.remaining_min = 0;
        a.awaiting_plate_clear = true;
        a.nozzle = 64; a.nozzle_target = 0; a.bed = 41; a.bed_target = 0;
    } else if (sc == "paused") {
        strlcpy(a.state, "PAUSE", sizeof(a.state));
    } else if (sc == "offline") {
        g.printers[1].connected = false;
    }

    ui_begin(disp);
    run(800);

    if (sc == "queue") {
        tap_tab(1);
    } else if (sc == "queue_item") {
        tap_tab(1);
        tap_text("Phone_stand");
    } else if (sc == "confirm_next") {
        tap(160, 209);
    } else if (sc == "started" || sc == "low_filament") {
        Command c = {CMD_CLEAR_AND_START, 1, 11, false};
        pending_result.cmd = c;
        pending_result.http_code = sc == "started" ? 200 : 409;
        pending_result.message[0] = 0;
        has_result = true;
        if (sc == "started") {
            // After a successful start Bambuddy flips the printer to PREPARE.
            strlcpy(a.state, "PREPARE", sizeof(a.state));
            strlcpy(a.job, "Cable_clips_x8", sizeof(a.job));
            a.progress = 0;
            a.awaiting_plate_clear = false;
            a.remaining_min = 47;
            a.layer = 0;
            a.total_layers = 0;
            a.nozzle = 152; a.nozzle_target = 220; a.bed = 48; a.bed_target = 55;
            g.queue_count[0]--;
            memmove(&g.queue[0][0], &g.queue[0][1], sizeof(QueueItem) * g.queue_count[0]);
            g.version++;
        }
        run(300);
    } else if (sc == "stop_confirm") {
        tap(254, 209);
    } else if (sc == "x2d" || sc == "offline") {
        tap_printer(1);
    }
    run(600);
    save(argv[2]);
    return 0;
}
