# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 BenjaminCs - https://github.com/BenjaminCs/bambuddy-esp32s3-es3c28p-display
# PlatformIO post-build step: writes .pio/build/<env>/firmware-merged.bin, a single
# image (bootloader + partition table + boot_app0 + app) to flash at offset 0x0.
# This lets people flash from the browser (https://espressif.github.io/esptool-js/)
# without installing anything.
Import("env")


def merge_bin(source, target, env):
    build = env.subst("$BUILD_DIR")
    out = f"{build}/firmware-merged.bin"
    images = [(addr, env.subst(path)) for addr, path in env["FLASH_EXTRA_IMAGES"]]
    images.append(("0x10000", f"{build}/firmware.bin"))
    args = " ".join(f"{addr} {path}" for addr, path in images)
    # "keep" preserves the flash mode/frequency already set in the bootloader header.
    env.Execute(
        f'"$PYTHONEXE" "$OBJCOPY" --chip esp32s3 merge_bin -o "{out}" '
        f"--flash_mode keep --flash_freq keep --flash_size 16MB {args}"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
