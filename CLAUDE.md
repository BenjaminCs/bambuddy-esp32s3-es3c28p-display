# Instructions for Claude

You are working on firmware for a 2.8" ESP32-S3 touch display (LCDWIKI ES3C28P) that controls
Bambu Lab printers through a self-hosted **Bambuddy** server. Read `README.md`,
`docs/HARDWARE.md` and `docs/BAMBUDDY_API.md` before changing anything non-trivial.

## Safety rules

- **Never send printer commands while testing.** `POST .../print/pause|resume|stop`,
  `/clear-plate` and `/queue/{id}/start` act on real hardware: a print can be ruined or a job
  started on a plate that is not empty. Use GET requests only, unless the user explicitly asks
  you to send a specific command.
- **Never commit or print secrets.** `include/secrets.h` (Wi-Fi password, API key, OTA
  password) is git-ignored; keep it that way. Do not paste the API key into docs, logs, commit
  messages or chat. `firmware-merged.bin` built from a real `secrets.h` contains these secrets
  too: never commit or publish it.
- **Do not create Bambuddy API keys by writing to its database.** Ask the user to create one in
  the Bambuddy UI (Settings → API keys) with read status, queue and control printer permissions.
- **Keep TLS validation.** Do not "fix" connection problems with `setInsecure()`. Find the real
  cause (DNS, certificate chain, clock) instead.

## Build

PlatformIO, `platformio.ini`, environments `usb` (serial upload) and `ota` (ArduinoOTA).
Without a local toolchain, build in Docker:

```bash
docker run --rm -v "$HOME/.cache/pio-docker":/root/.platformio -v "$PWD":/proj -w /proj \
  python:3.12-slim sh -c 'pip install -q platformio==6.1.18 && pio run -e usb'
```

- Always build after a change and fix warnings in `src/`. Output:
  `.pio/build/usb/firmware.bin` and `firmware-merged.bin` (flash at `0x0`).
- To compile without the user's secrets, copy `include/secrets.example.h` to `include/secrets.h`
  in a scratch copy of the project. Careful with `cp` aliases (`cp -i`) that silently refuse to
  overwrite.
- After any UI change, run `tools/sim/render.sh` and **look at** the PNGs in
  `docs/screenshots/` (clipping, overlaps, missing glyphs shown as boxes). Add a scenario to
  `tools/sim/sim.cpp` for new screens. The simulator uses the stub `tools/sim/stub/secrets.h`,
  never the real one.
- You usually cannot flash or see the device. Say so plainly: "builds" is not "works on the
  hardware". Ask the user to report what the screen shows.

## Architecture (keep it)

- **Two tasks.** `net.cpp` runs Wi-Fi + HTTPS in a FreeRTOS task on core 0. LVGL runs only on
  the Arduino loop (core 1). LVGL is not thread-safe: never call `lv_*` from the net task.
- **Data flow.** The net task writes `AppState` under a mutex and bumps `version`; the UI copies
  it with `net_snapshot()` when the version changed. The UI sends `Command`s through
  `net_send()` and reads `CommandResult`s with `net_poll_result()`. Add new actions as a new
  `CmdType`, not as direct HTTP calls from the UI.
- **Fixed-size structs** in `model.h` (no `String`/heap in shared state). Raise `MAX_*` with
  care: `AppState` is copied as a whole.
- **JSON** is parsed through ArduinoJson filters (`fetch_status`, `fetch_queue`). When you need a
  new field, add it to the filter, otherwise it silently reads as missing/zero.
- **UI text** lives only in `src/ui_strings.h` (English default, Hungarian with `UI_LANGUAGE_HU`).
  Add every new string to **both** language blocks.
- **Fonts**: `font_hu_14/18/28` replace LVGL's Montserrat. A character outside ASCII, Latin-1
  and Őőűű, or a symbol outside the set in `tools/gen_fonts.sh`, renders as a box. Regenerate
  with the script; do not hand-edit `src/fonts/*.c`.
- **Board specifics** live in `src/board.h` only. Rotation is `DISPLAY_ROTATION` (default 3).
- **Touch mapping is measured, not guessed.** `touch_raw_to_screen()` maps raw FT6336G
  coordinates (`getTouchRaw()`); at rotation 3: `x = 16 + raw_y`, `y = 276 - 1.27157 * raw_x`
  (rotation 1: `x = 303.4 - raw_y`, `y = 1.27157 * raw_x - 37`), verified on hardware with a
  tap test. Mixing up the two rotations' formulas mirrors touch on both axes. LovyanGFX's `getTouch()` / `offset_rotation` give wrong results on
  this panel. Do not "simplify" this back to `getTouch()`, and do not change rotation or the
  mapping without a hardware tap test.

## Library versions and API names

- **LVGL 9.2** (not 8): `lv_button_create`, `lv_buttonmatrix_*`, `lv_screen_active()`,
  `lv_display_*`, `lv_indev_create()`, `lv_msgbox_create(NULL)` + `lv_msgbox_add_*`,
  `lv_obj_remove_flag`. Many v8 tutorials online use names that no longer exist.
- **LovyanGFX 1.2.x**: `Panel_ILI9341`, `Touch_FT5x06` (for the FT6336G).
- **ArduinoJson 7**: `JsonDocument` (no `StaticJsonDocument` / `DynamicJsonDocument`),
  `doc["x"].is<T>()`.
- **Arduino-ESP32 2.0.17** via `platform = espressif32@6.9.0`. Arduino core 3.x needs the
  pioarduino platform and has API differences (WiFi events, `ledc`, TLS); do not switch casually.

## Bambuddy behaviour to remember

- `remaining_time` is in **minutes**, `print_time_seconds` in seconds, `progress` is 0–100.
- `/queue/{id}/start` does not print immediately: it clears `manual_start`; Bambuddy's
  scheduler starts the job when the printer is idle and `awaiting_plate_clear` is false.
- `/start` → `409` = filament-deficit check; retry with `skip_filament_check=true` only after
  the user confirms.
- `clear-plate` returns `400` when the printer is not waiting for it; that is not an error when
  followed by a start.
- Check `/openapi.json` on the user's instance before relying on a field: Bambuddy moves fast.

## Hardware facts that are easy to get wrong

- ILI9341V needs **inversion ON** and **BGR**; LCD reset is on EN (no GPIO).
- PSRAM is **octal** (`qio_opi`); flash is 16 MB.
- GPIO 45 (backlight) and 46 (LCD DC) are strapping pins.
- Touch I2C (15/16) is shared with the external I2C header; audio amp enable (GPIO 1) is active low.
- Wi-Fi is 2.4 GHz only.

## Style

Match the existing code: C++ with Arduino types, 4-space indent, `snake_case` functions,
`g_` prefix for net-task globals, `s_` for UI statics, short comments that explain *why*.
Keep comments in English.
