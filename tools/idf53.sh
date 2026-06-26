#!/usr/bin/env bash
set -euo pipefail

idf_path="${IDF53_PATH:-$HOME/esp-idf}"
export IDF_PATH="$idf_path"

if [[ ! -f "$IDF_PATH/export.sh" ]]; then
    echo "ESP-IDF 5.3 export script not found: $IDF_PATH/export.sh" >&2
    echo "Set IDF53_PATH to the ESP-IDF 5.3 installation directory." >&2
    exit 1
fi

source "$IDF_PATH/export.sh" >/dev/null

idf_version="$(idf.py --version)"
if [[ "$idf_version" != "ESP-IDF v5.3"* ]]; then
    echo "Expected ESP-IDF 5.3.x, got: $idf_version" >&2
    echo "IDF_PATH resolved to: $IDF_PATH" >&2
    exit 1
fi

export IDF_TARGET=esp32c3
exec idf.py "$@"
