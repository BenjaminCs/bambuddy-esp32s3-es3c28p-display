// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 BenjaminCs - https://github.com/BenjaminCs/bambuddy-esp32s3-es3c28p-display

#pragma once

// Board definition for the LCDWIKI ES3C28P (Shenzhen Hong Shuyuan Technology),
// "2.8inch ESP32-S3 Display", ESP32-S3-WROOM-1 N16R8.
// Pinout source: https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
// See docs/HARDWARE.md for the full pin table and the pitfalls.

#define LovyanGFX_DEVICE
#include <LovyanGFX.hpp>

// ILI9341V, 4-wire SPI
#define PIN_LCD_SCLK 12
#define PIN_LCD_MOSI 11
#define PIN_LCD_MISO 13
#define PIN_LCD_DC   46  // strapping pin, fine as an output after boot
#define PIN_LCD_CS   10
#define PIN_LCD_BL   45  // backlight, active high; strapping pin (VDD_SPI) – do not add pull-ups
// LCD reset is tied to the ESP32-S3 EN line, there is no GPIO for it.

// FT6336G capacitive touch, I2C address 0x38 (shared with the 4-pin I2C header)
#define PIN_TOUCH_SDA 16
#define PIN_TOUCH_SCL 15
#define PIN_TOUCH_INT 17
#define PIN_TOUCH_RST 18

#define SCREEN_W 320  // landscape
#define SCREEN_H 240

// Landscape orientation. 3 is the right way up on the ES3C28P (verified on hardware);
// 1 turns the picture by 180°.
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION 3
#endif

// Touch mapping, measured on real hardware (fitted from a 5-cross tap test, then checked on
// 12 targets).
//
// LovyanGFX's getTouch()/offset_rotation does not fit this panel: the touch layer's axes are
// turned against the LCD, and its active area is narrower than the nominal 0-239 in raw X.
// So the firmware reads raw FT6336G coordinates with getTouchRaw() and maps them itself.
static inline int touch_clamp(float v, int hi) { return v < 0 ? 0 : (v > hi ? hi : (int)(v + 0.5f)); }

static inline void touch_raw_to_screen(int raw_x, int raw_y, int &x, int &y) {
#if DISPLAY_ROTATION == 3
    x = touch_clamp(16.0f + raw_y, SCREEN_W - 1);
    y = touch_clamp(276.0f - 1.27157f * raw_x, SCREEN_H - 1);
#else  // DISPLAY_ROTATION == 1
    x = touch_clamp(303.4f - raw_y, SCREEN_W - 1);
    y = touch_clamp(1.27157f * raw_x - 37.0f, SCREEN_H - 1);
#endif
}

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Light_PWM _light;
    lgfx::Touch_FT5x06 _touch;  // the FT6336G speaks the FT5x06 protocol

public:
    LGFX() {
        {
            auto c = _bus.config();
            c.spi_host = SPI2_HOST;
            c.spi_mode = 0;
            c.freq_write = 40000000;  // 40 MHz is stable; 80 MHz is not guaranteed for ILI9341
            c.freq_read = 16000000;
            c.spi_3wire = false;
            c.use_lock = true;
            c.dma_channel = SPI_DMA_CH_AUTO;
            c.pin_sclk = PIN_LCD_SCLK;
            c.pin_mosi = PIN_LCD_MOSI;
            c.pin_miso = PIN_LCD_MISO;
            c.pin_dc = PIN_LCD_DC;
            _bus.config(c);
            _panel.setBus(&_bus);
        }
        {
            auto c = _panel.config();
            c.pin_cs = PIN_LCD_CS;
            c.pin_rst = -1;  // tied to EN
            c.pin_busy = -1;
            c.panel_width = 240;  // native portrait size
            c.panel_height = 320;
            c.offset_rotation = 0;
            c.readable = true;
            // The vendor init code sends 0x21 (display inversion ON) and sets the BGR bit
            // in MADCTL: this is an IPS panel. Without invert=true all colours are negative.
            c.invert = true;
            c.rgb_order = false;  // false = BGR, the ILI9341 default
            c.dlen_16bit = false;
            c.bus_shared = false;  // the SD card uses SDIO pins, not this SPI bus
            _panel.config(c);
        }
        {
            auto c = _light.config();
            c.pin_bl = PIN_LCD_BL;
            c.invert = false;
            c.freq = 44100;
            c.pwm_channel = 7;
            _light.config(c);
            _panel.setLight(&_light);
        }
        {
            auto c = _touch.config();
            c.x_min = 0;  // raw range is in portrait orientation
            c.x_max = 239;
            c.y_min = 0;
            c.y_max = 319;
            c.pin_int = PIN_TOUCH_INT;
            c.pin_rst = -1;  // reset is pulsed by setup() before init
            c.bus_shared = false;
            c.offset_rotation = 0;  // unused: mapping is done by touch_raw_to_screen()
            c.i2c_port = 0;
            c.i2c_addr = 0x38;
            c.pin_sda = PIN_TOUCH_SDA;
            c.pin_scl = PIN_TOUCH_SCL;
            c.freq = 400000;
            _touch.config(c);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};
