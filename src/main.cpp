// Bambuddy touch display – entry point.
//
// Core 1 (Arduino loop): LovyanGFX + LVGL rendering, touch, OTA, backlight dimming.
// Core 0 (net task):     Wi-Fi + Bambuddy API polling, see net.cpp.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <lvgl.h>

#include "board.h"
#include "net.h"
#include "secrets.h"
#include "ui.h"

static const uint32_t DIM_AFTER_MS = 120000;  // dim after 2 minutes without touch
static const uint8_t BRIGHT = 200;            // backlight PWM 0-255
static const uint8_t DIM = 12;
static const int BUF_LINES = 40;              // LVGL partial buffer height (2 x 25.6 kB)

static LGFX tft;
static uint32_t last_touch = 0;
static bool dimmed = false;
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
    uint16_t x, y;
    bool pressed = tft.getTouch(&x, &y);  // already rotated to screen coordinates
    if (pressed) {
        last_touch = millis();
        if (dimmed) {
            dimmed = false;
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

    ui_begin(disp);
    net_begin();
    last_touch = millis();
}

void loop() {
    ui_tick();
    lv_timer_handler();

    if (!ota_started && WiFi.status() == WL_CONNECTED) start_ota();
    if (ota_started) ArduinoOTA.handle();

    if (!dimmed && millis() - last_touch > DIM_AFTER_MS) {
        dimmed = true;
        tft.setBrightness(DIM);
    }
    delay(5);
}
