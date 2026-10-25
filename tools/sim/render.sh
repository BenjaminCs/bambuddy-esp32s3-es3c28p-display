#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 BenjaminCs - https://github.com/BenjaminCs/bambuddy-esp32s3-es3c28p-display
# Renders UI screenshots on the host (Linux/macOS, no hardware needed).
#
# Compiles the real src/ui.cpp + LVGL natively, feeds it mock Bambuddy data
# (tools/sim/sim.cpp), taps through the scenarios and writes PNGs (2x, 640x480)
# to docs/screenshots/<lang>_<scenario>.png.
#
# Needs: gcc/g++, Python 3 with Pillow, and the LVGL sources that PlatformIO
# downloads on the first build (run `pio run -e usb` once).
#
# Usage: tools/sim/render.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SIM="$ROOT/tools/sim"
LVGL="$(ls -d "$ROOT"/.pio/libdeps/*/lvgl 2>/dev/null | head -1)"
[ -n "$LVGL" ] || { echo "LVGL not found: run 'pio run -e usb' once first" >&2; exit 1; }
BUILD="$ROOT/.pio/sim"
OUT="$ROOT/docs/screenshots"
mkdir -p "$BUILD/obj" "$OUT"

CFLAGS=(-O1 -w -DLV_CONF_INCLUDE_SIMPLE -I"$ROOT/include" -I"$LVGL")

if [ ! -f "$BUILD/liblvgl.a" ]; then
    echo "building LVGL for the host…"
    find "$LVGL/src" -name '*.c' | while read -r f; do
        echo "$f"
    done | xargs -P"$(getconf _NPROCESSORS_ONLN)" -I{} sh -c \
        'gcc -c "$@" "{}" -o "'"$BUILD"'/obj/$(echo "{}" | md5sum | cut -c1-12).o"' _ "${CFLAGS[@]}"
    for s in 14 18 28; do
        gcc -c "${CFLAGS[@]}" "$ROOT/src/fonts/font_hu_$s.c" -o "$BUILD/obj/font_hu_$s.o"
    done
    ar rcs "$BUILD/liblvgl.a" "$BUILD"/obj/*.o
fi

# tools/sim/stub comes first so the stub secrets.h is used, never your real one.
for lang in en hu; do
    flag=()
    [ "$lang" = hu ] && flag=(-DUI_LANGUAGE_HU)
    g++ -O1 -std=gnu++17 "${flag[@]}" -DLV_CONF_INCLUDE_SIMPLE -I"$SIM/stub" -I"$ROOT/include" -I"$ROOT/src" \
        -I"$LVGL" "$SIM/sim.cpp" "$ROOT/src/ui.cpp" "$BUILD/liblvgl.a" -lm -o "$BUILD/sim_$lang"
done

SCENARIOS="status queue queue_item finished confirm_next started low_filament paused stop_confirm x2d offline connecting"
for lang in en hu; do
    for sc in $SCENARIOS; do
        "$BUILD/sim_$lang" "$sc" "$BUILD/${lang}_$sc.ppm"
    done
done

python3 - "$BUILD" "$OUT" $SCENARIOS <<'EOF'
import sys
from PIL import Image
build, out, names = sys.argv[1], sys.argv[2], sys.argv[3:]
for lang in ("en", "hu"):
    for n in names:
        im = Image.open(f"{build}/{lang}_{n}.ppm").convert("RGB")
        im.resize((640, 480), Image.NEAREST).save(f"{out}/{lang}_{n}.png", optimize=True)
print(f"wrote {2 * len(names)} screenshots to {out}")
EOF
