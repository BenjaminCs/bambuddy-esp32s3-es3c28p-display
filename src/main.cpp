// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 BenjaminCs - https://github.com/BenjaminCs/bambuddy-esp32s3-es3c28p-display

// Bambuddy touch display – entry point.
//
// Core 1 (Arduino loop): LovyanGFX + LVGL rendering, touch, OTA, backlight dimming.
// Core 0 (net task):     Wi-Fi + Bambuddy API polling, see net.cpp.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <lvgl.h>

#include "secrets.h"  // first, so it may override DISPLAY_ROTATION
#include "board.h"
#include "net.h"
#include "ui.h"

static const uint32_t DIM_AFTER_MS = 60000;   // dim after 1 minute without touch
static const uint32_t OFF_AFTER_MS = 120000;  // backlight off after 2 minutes
static const uint8_t BRIGHT = 200;            // backlight PWM 0-255
static const uint8_t DIM = 12;
static const int BUF_LINES = 40;              // LVGL partial buffer height (2 x 25.6 kB)

static LGFX tft;
static uint32_t last_touch = 0;
enum Power { POWER_ON, POWER_DIM, POWER_OFF };
static Power power = POWER_ON;
static bool swallow_touch = false;  // the touch that wakes the screen must not press a button
static bool ota_started = false;

// LVGL renders RGB565 little-endian; lgfx::rgb565_t is the same layout, so LovyanGFX
// does the byte swap for the SPI panel itself.
static void flush_cb(lv_display_t *disp, const lv_area_t *a, uint8_t *px) {
    uint32_t w = lv_area_get_width(a), h = lv_area_get_height(a);
    tft.startWrite();
    tft.pushImage(a->x1, a->y1, w, h, (lgfx::rgb565_t *)px);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

static void touch_cb(lv_indev_t *, lv_indev_data_t *data) {
    // The FT6336G sometimes reports a single "released" sample while the finger is still
    // down; bridge such gaps briefly, otherwise LVGL sees the click as aborted.
    static const uint32_t RELEASE_HOLD_MS = 60;
    static uint32_t last_pressed_ms = 0;
    static int last_x = 0, last_y = 0;

    int32_t rx, ry;
    int x = 0, y = 0;
    bool pressed = tft.getTouchRaw(&rx, &ry) > 0;  // raw FT6336G coordinates
    if (pressed) {
        touch_raw_to_screen(rx, ry, x, y);
        last_pressed_ms = millis();
        last_x = x;
        last_y = y;
    } else if (last_pressed_ms && millis() - last_pressed_ms < RELEASE_HOLD_MS) {
        pressed = true;
        x = last_x;
        y = last_y;
    }
    if (pressed) {
        last_touch = millis();
        if (power != POWER_ON) {
            power = POWER_ON;
            swallow_touch = true;
            tft.setBrightness(BRIGHT);
        }
    } else {
        swallow_touch = false;
    }
    if (pressed && !swallow_touch) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static uint32_t tick_cb() { return millis(); }

// Wakes the screen without a touch (print finished, failed, paused, new printer error).
// No touch is swallowed in this case: the screen is already lit when the user reaches it.
static void wake_for_event(const char *why) {
    Serial.printf("Wake: %s\n", why);
    power = POWER_ON;
    last_touch = millis();
    tft.setBrightness(BRIGHT);
}

static bool state_busy(const char *s) {
    return !strcmp(s, "RUNNING") || !strcmp(s, "PREPARE") || !strcmp(s, "SLICING") || !strcmp(s, "PAUSE");
}

// Compares each printer's state with the previous poll and wakes the screen on events
// worth looking at. Uses its own snapshot, so it does not interfere with the UI's.
static void check_wake_events() {
    static AppState snap;
    static uint32_t snap_version = 0;
    static int prev_id[MAX_PRINTERS];
    static char prev_state[MAX_PRINTERS][16];
    static int prev_hms[MAX_PRINTERS];

    if (!net_snapshot(snap, snap_version)) return;
    snap_version = snap.version;

    for (int i = 0; i < snap.printer_count && i < MAX_PRINTERS; i++) {
        const PrinterInfo &p = snap.printers[i];
        bool known = prev_id[i] == p.id && prev_state[i][0] && p.state[0] && p.status_ok;
        if (known && strcmp(prev_state[i], p.state) != 0) {
            if (!strcmp(p.state, "FINISH")) wake_for_event("print finished");
            else if (!strcmp(p.state, "FAILED")) wake_for_event("print failed");
            else if (!strcmp(p.state, "PAUSE")) wake_for_event("print paused");
            else if (state_busy(prev_state[i]) && !state_busy(p.state)) wake_for_event("print ended");
        }
        if (known && p.hms_count > prev_hms[i]) wake_for_event("printer error");
        if (p.status_ok && p.state[0]) {
            prev_id[i] = p.id;
            strlcpy(prev_state[i], p.state, sizeof(prev_state[i]));
            prev_hms[i] = p.hms_count;
        }
    }
}

// The FT6336G reset line is a GPIO on this board; pulse it before LovyanGFX
// probes the controller, otherwise touch can come up dead after a soft reset.
static void reset_touch_controller() {
    pinMode(PIN_TOUCH_RST, OUTPUT);
    digitalWrite(PIN_TOUCH_RST, LOW);
    delay(10);
    digitalWrite(PIN_TOUCH_RST, HIGH);
    delay(300);
}

static void start_ota() {
    ArduinoOTA.setHostname(DEVICE_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([] {
        // LVGL stops being serviced during the upload; draw a plain message instead.
        tft.setBrightness(BRIGHT);
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.drawCenterString("Updating...", SCREEN_W / 2, SCREEN_H / 2 - 8, &fonts::Font2);
    });
    ArduinoOTA.begin();
    ota_started = true;
    Serial.printf("OTA ready: %s (%s)\n", DEVICE_HOSTNAME, WiFi.localIP().toString().c_str());
}

void setup() {
    Serial.begin(115200);  // native USB CDC (ARDUINO_USB_CDC_ON_BOOT=1)

    reset_touch_controller();
    tft.init();
    tft.setRotation(DISPLAY_ROTATION);
    tft.setBrightness(BRIGHT);
    tft.fillScreen(TFT_BLACK);

    lv_init();
    lv_tick_set_cb(tick_cb);

    // Two partial buffers in internal DMA-capable RAM (PSRAM is too slow for this).
    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
    size_t buf_size = SCREEN_W * BUF_LINES * sizeof(uint16_t);
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_indev_t *touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, touch_cb);
    lv_indev_set_scroll_limit(touch, 30);  // finger jitter must not turn a tap into a scroll
    lv_indev_set_long_press_time(touch, 600);

    ui_begin(disp);
    net_begin();
    last_touch = millis();
}

void loop() {
    ui_tick();
    lv_timer_handler();

    if (!ota_started && WiFi.status() == WL_CONNECTED) start_ota();
    if (ota_started) ArduinoOTA.handle();

    check_wake_events();

    uint32_t idle = millis() - last_touch;
    if (power == POWER_ON && idle > DIM_AFTER_MS) {
        power = POWER_DIM;
        tft.setBrightness(DIM);
    }
    if (power == POWER_DIM && idle > OFF_AFTER_MS) {
        power = POWER_OFF;
        tft.setBrightness(0);
    }
    delay(5);
}
