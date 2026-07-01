"""Sync PlatformIO partition table and flash size with esp32.h (pre-build)."""

Import("env")

import os
import re

FLASH_OPTIONS = (
    ("ESP32_FLASH_32MB", "partitions_32MB_ota.csv", "32MB"),
    ("ESP32_FLASH_8MB", "partitions_8MB_ota.csv", "8MB"),
    ("ESP32_FLASH_4MB", "partitions_4MB_ota.csv", "4MB"),
)

esp32_h = os.path.join(env["PROJECT_DIR"], "esp32.h")
with open(esp32_h, encoding="utf-8") as f:
    content = f.read()

partitions_file = FLASH_OPTIONS[-1][1]
flash_size = FLASH_OPTIONS[-1][2]

for define, csv_name, size in FLASH_OPTIONS:
    if re.search(r"^\s*#define\s+" + define + r"\s*$", content, re.MULTILINE):
        partitions_file = csv_name
        flash_size = size
        break

board = env.BoardConfig()
board.update("build.partitions", partitions_file)
board.update("upload.flash_size", flash_size)

env.Replace(
    BOARD_PARTITIONS_FILE=partitions_file,
    UPLOAD_FLASH_SIZE=flash_size,
)

print(
    "esp32_partition_setup: using %s (flash %s) from esp32.h"
    % (partitions_file, flash_size)
)
