# Third-party notices

This project's own code is licensed under the **GNU Affero General Public License v3.0 or
later** (see [LICENSE](LICENSE)), the same license family as
[Bambuddy](https://github.com/maziggy/bambuddy). It does not contain Bambuddy code; it talks to
Bambuddy only over its HTTP API.

It uses or bundles the following third-party components.

## Bundled in this repository

| Component | Where | License |
|---|---|---|
| Montserrat font (subset, converted to LVGL bitmaps) | `src/fonts/font_hu_*.c` | SIL Open Font License 1.1, see [licenses/OFL-1.1.txt](licenses/OFL-1.1.txt). Copyright 2011 The Montserrat Project Authors |
| Font Awesome Free 5 icons (subset, converted to LVGL bitmaps) | `src/fonts/font_hu_*.c` | Fonts: SIL OFL 1.1; icons: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Font Awesome by Fonticons, Inc., https://fontawesome.com |
| ISRG Root X1 and X2 certificates (Let's Encrypt) | `src/certs.h` | Public CA certificates, published by ISRG at https://letsencrypt.org/certificates/ |

## Downloaded at build time (not in this repository)

These are fetched by PlatformIO and end up in the compiled firmware.

| Component | Version | License |
|---|---|---|
| [LVGL](https://github.com/lvgl/lvgl) | 9.2.2 | MIT |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | 1.2.7 | MIT and BSD-2-Clause (FreeBSD) |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 7.2.1 | MIT |
| [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) | 2.0.17 | LGPL-2.1 |
| [ESP-IDF](https://github.com/espressif/esp-idf) (via the Arduino core) | 4.4 | Apache-2.0, plus Espressif binary libraries (Wi-Fi/BT) under Espressif's own terms |

All of the above are compatible with distributing this project under AGPL-3.0-or-later. If you
distribute a compiled firmware image, keep these notices with it.

## Hardware documentation

Pin assignments and panel settings in `docs/HARDWARE.md` and `src/board.h` are taken from the
board vendor's public documentation (LCDWIKI, https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)
and restated as facts; no vendor code is included.
