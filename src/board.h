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

// 1 or 3: landscape, depending on which side the USB-C port should be on.
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION 1
#endif
// If touches land mirrored or rotated, try values 0-7 here.
#ifndef TOUCH_OFFSET_ROTATION
#define TOUCH_OFFSET_ROTATION 0
#endif

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
            c.offset_rotation = TOUCH_OFFSET_ROTATION;
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
