#!/usr/bin/env python3

# Adds PlatformIO post-processing to merge all the ESP flash images into a single image.
# This can be installed locally by running some variation of:
# python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.usbmodem2101  --chip esp32s3 write_flash 0x0 /tmp/vest_s3.bin

import os

Import("env", "projenv")

firmware_bin = "${BUILD_DIR}/${PROGNAME}"
is_esp32 = "esp32" in env.get("BOARD_MCU", "")
if is_esp32:
    firmware_bin += ".bin"

merged_bin = os.environ.get("MERGED_BIN_PATH", "${BUILD_DIR}/${PROGNAME}-merged.bin")


def merge_bin_action(source, target, env):
    if not is_esp32:
        return
    board_config = env.BoardConfig()
    flash_images = [
        *env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])),
        "$ESP32_APP_OFFSET",
        source[0].get_abspath(),
    ]
    merge_cmd = " ".join(
        [
            '"$PYTHONEXE"',
            '"$OBJCOPY"',
            "--chip",
            board_config.get("build.mcu", "esp32"),
            "merge_bin",
            "-o",
            merged_bin,
            "--flash_mode",
            # Force DIO. Otherwise the web flasher does not work for ESP32-S3 and ESP32-C6.
            "dio",
            #  board_config.get("build.flash_mode", "dio"),
            "--flash_freq",
            "${__get_board_f_flash(__env__)}",
            "--flash_size",
            board_config.get("upload.flash_size", "4MB"),
            *flash_images,
        ]
    )
    env.Execute(merge_cmd)


env.AddCustomTarget(
    name="mergebin",
    dependencies=firmware_bin,
    actions=merge_bin_action,
    title="Merge binary",
    description="Build combined image",
    always_build=True,
)
