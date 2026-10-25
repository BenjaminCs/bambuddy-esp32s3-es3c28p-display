#!/usr/bin/env bash
# Regenerates src/fonts/font_hu_{14,18,28}.c with lv_font_conv.
#
# Why: LVGL's built-in Montserrat fonts contain ASCII only. These fonts add
# Latin-1 (U+00A0-U+00FF: á é í ó ö ú ü ° · …), Hungarian Őő Űű, and the
# FontAwesome glyphs behind LVGL's LV_SYMBOL_* macros plus a clock and a flame icon.
#
# To support another language, extend RANGE (e.g. 0x100-0x17F for all of Latin
# Extended-A) and run the script again. Bigger ranges cost flash (~10-30 kB per size).
#
# Needs: node/npx and curl. Usage: tools/gen_fonts.sh   (from the project root)
set -euo pipefail

LVGL_VER=v9.2.2
OUT="$(cd "$(dirname "$0")/.." && pwd)/src/fonts"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

BASE="https://cdn.jsdelivr.net/gh/lvgl/lvgl@${LVGL_VER}/scripts/built_in_font"
curl -sSfL -o "$WORK/Montserrat-Medium.ttf" "$BASE/Montserrat-Medium.ttf"
curl -sSfL -o "$WORK/FontAwesome5.woff" "$BASE/FontAwesome5-Solid+Brands+Regular.woff"

RANGE="0x20-0x7E,0xA0-0xFF,0x150-0x151,0x170-0x171,0x2013,0x2026"
# The LV_SYMBOL_* set from LVGL's own font script, plus U+F06D (fire), U+F017 (clock),
# U+F2C9 (thermometer).
SYMBOLS="61441,61448,61451,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674,61683,61724,61732,61787,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,63426,63650,0xf06d,0xf017,0xf2c9"

mkdir -p "$OUT"
for size in 14 18 28; do
    npx -y lv_font_conv@1.5.3 --no-compress --no-prefilter --bpp 4 --size "$size" \
        --format lvgl --lv-include lvgl.h --lv-font-name "font_hu_${size}" \
        --font "$WORK/Montserrat-Medium.ttf" -r "$RANGE" \
        --font "$WORK/FontAwesome5.woff" -r "$SYMBOLS" \
        -o "$OUT/font_hu_${size}.c"
    echo "wrote $OUT/font_hu_${size}.c"
done
