# Hardware: LCDWIKI ES3C28P (2.8" ESP32-S3 display)

## Exact model

| | |
|---|---|
| Model | **ES3C28P** (capacitive touch). ES3N28P is the same board without touch: **do not buy that one** for this project |
| Manufacturer | Shenzhen Hong Shuyuan Technology Co., Ltd. (brand: **LCDWIKI**) |
| Vendor page | https://www.lcdwiki.com/2.8inch_ESP32-S3_Display |
| Module | ESP32-S3 **N16R8**: 16 MB quad SPI flash + 8 MB **octal (OPI)** PSRAM |
| Display | 2.8" IPS, 240×320, **ILI9341V** over 4-wire SPI, 262K colours (used as RGB565) |
| Touch | **FT6336G** capacitive, I2C address 0x38 |
| Audio | ES8311 codec, FM8002E amplifier, speaker connector, on-board MEMS microphone |
| Other | WS2812 RGB LED, microSD (SDIO), battery connector + charger, BOOT and RESET buttons, USB-C |
| Power | 5 V over USB-C (or a 3.7 V Li-ion cell on the BAT connector) |

### How to recognise it in shops

It is sold under generic titles, often on Temu / AliExpress, for example:
*"Multifunctional ESP32-S3 Touch Display Screen with Speaker and Microphone, IPS LCD, WiFi BT,
LVGL Support for IoT HMI Smart Home Projects"* (variant "7.11 cm black", seller reference
**TE066**, comes with a small speaker, a USB cable and a 4-pin cable).

Check the **back of the PCB**: the silkscreen reads **"2.8" LCD Display ESP32-S3 240x320
Capacitive Touch"**, with labelled connectors SPEAKER, I2C, BAT, UART, an RGB LED, a microSD
slot, BOOT and RESET buttons, and "MIC" printed next to the display on the front.

Similar-looking boards with a different pinout exist (e.g. the "Cheap Yellow Display" family,
ESP32-2432S028, which uses a classic ESP32 and resistive touch). This firmware will **not**
work on them without changing `src/board.h`.

## Pinout

| Function | GPIO | Notes |
|---|---|---|
| LCD SCLK | 12 | SPI2 (FSPI) |
| LCD MOSI | 11 | |
| LCD MISO | 13 | |
| LCD CS | 10 | active low |
| LCD DC | 46 | high = data. **Strapping pin**, fine as an output after boot |
| LCD RESET | — | tied to the ESP32-S3 **EN** line, no GPIO |
| LCD backlight | 45 | active high, PWM-dimmable. **Strapping pin** (VDD_SPI voltage), so add no external pull-up |
| Touch SDA | 16 | shared with the I2C header |
| Touch SCL | 15 | shared with the I2C header |
| Touch RST | 18 | active low |
| Touch INT | 17 | low while touched |
| RGB LED (WS2812) | 42 | |
| microSD CLK / CMD | 38 / 40 | SDIO |
| microSD D0–D3 | 39, 41, 48, 47 | SDIO (vendor lists them in this order) |
| Audio amp enable | 1 | **active low** |
| I2S MCLK / BCLK / LRCK | 4 / 5 / 7 | |
| I2S DOUT (to codec) / DIN (mic) | 8 / 6 | |
| Battery voltage ADC | 9 | |
| BOOT button | 0 | |
| UART0 RX / TX | 44 / 43 | on the UART header |
| Free GPIOs | 2, 3, 14, 21 | on the expansion header |

The SPI expansion header shares its lines with the microSD slot, and the I2C header shares
its lines with touch.

## Settings that matter

### Build (PlatformIO / Arduino)

- Board: `esp32-s3-devkitc-1` with overrides. There is no dedicated board definition.
- `board_build.arduino.memory_type = qio_opi`. **Octal PSRAM.** The default (`qio_qspi`) makes
  the board boot-loop or report no PSRAM.
- Flash 16 MB, partition table `default_16MB.csv` (two 6.5 MB OTA slots).
- `-DARDUINO_USB_CDC_ON_BOOT=1`: the USB-C port is the ESP32-S3's native USB, so `Serial`
  must be routed to USB CDC or you will see no log output.
- In the Arduino IDE instead: board "ESP32S3 Dev Module", Flash Size 16MB, PSRAM "OPI PSRAM",
  USB CDC On Boot "Enabled", Partition Scheme "16M Flash (3MB APP/9.9MB FATFS)" or similar.

### Display (LovyanGFX `Panel_ILI9341`)

- `invert = true`. The vendor init sequence sends `0x21` (display inversion ON) because it is
  an IPS panel. Without it every colour is negative.
- `rgb_order = false` (BGR). The vendor sets the BGR bit in MADCTL (`0x36 = 0x08`).
- `panel_width = 240`, `panel_height = 320` (native portrait). **Rotation 3** is landscape the
  right way up; rotation 1 shows the picture upside down.
- `pin_rst = -1`: reset is wired to EN.
- SPI at 40 MHz write / 16 MHz read is reliable.
- Vendor init code for reference: https://www.lcdwiki.com/res/ES3C28P/ILI9341V_Init.txt
  (LovyanGFX's built-in ILI9341 init works; only inversion and BGR had to be set).

### Touch (LovyanGFX `Touch_FT5x06`)

- The FT6336G is FT5x06-compatible. I2C port 0, 400 kHz, address 0x38.
- **Do not rely on LovyanGFX's touch rotation for this panel.** The touch layer is rotated
  against the LCD and its active area is smaller than the LCD (raw X ≈ 29–218, raw Y ≈ 0–303).
  Measured with a tap test on hardware, using `getTouchRaw()`:
  - rotation 3 (the right way up): `screen x = 16 + raw_y`, `screen y = 276 − 1.27157 × raw_x`
  - rotation 1 (upside down): `screen x = 303.4 − raw_y`, `screen y = 1.27157 × raw_x − 37`
  See `touch_raw_to_screen()` in `src/board.h`.
- Pulse the reset pin (GPIO 18) low for ~10 ms, then wait ~300 ms before init. Otherwise
  touch can stay dead after a soft reset.

### Flashing

- Native USB: usually flashes directly. If the port does not appear or the upload fails:
  hold **BOOT**, tap **RESET**, release **BOOT**, then flash, then press **RESET**.
- After flashing in download mode, the board does not reboot by itself: press RESET.
- Some charge-only USB-C cables have no data lines; try another cable if no port appears.

## Pitfalls we hit (so you don't have to)

1. **Accented characters render as boxes.** LVGL's built-in fonts are ASCII-only. Generate your
   own with `lv_font_conv` (see `tools/gen_fonts.sh`).
2. **LVGL warns "Possible failure to include lv_conf.h"** on every file even though the file
   is found. Use a classic `#ifndef LV_CONF_H / #define LV_CONF_H` guard, not `#pragma once`.
3. **`IP_ADDR4(&ip, MY_IP_MACRO)` fails to compile** ("requires 5 arguments"): lwIP's macro
   counts its arguments before your macro expands. Go through an array.
4. **Split-horizon DNS.** If your server's host name resolves to a LAN IP only on a local DNS
   (Pi-hole), the router's fallback DNS may return the public IP, which often fails from inside
   the LAN (no NAT hairpin). Force the local DNS on the ESP32 (`dns_setserver` for both slots
   after getting an IP).
5. **Let's Encrypt chains changed in 2025/26** (new YE/YR intermediates, cross-signed to ISRG
   Root X2 → X1). Embedding **both ISRG Root X1 and X2** keeps validation working; the
   ESP32 Arduino default bundle may lag behind.
6. **TLS handshakes are slow** (~1 s). Reuse the connection (`HTTPClient::setReuse(true)`)
   when polling every few seconds.
7. **Large JSON responses.** Bambuddy's printer status is ~2.5 kB and grows with AMS units;
   the queue list is ~1.6 kB per item. Parse with an ArduinoJson filter. PSRAM absorbs the rest.

## Unused hardware (ideas)

Speaker + ES8311 (a chime when a print finishes), RGB LED (status colour), microSD,
battery operation, and the free GPIOs on the expansion header.
