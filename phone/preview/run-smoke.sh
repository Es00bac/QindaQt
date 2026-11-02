#!/bin/bash
# Builds the preview host and loads every scenario x screen headless; QML warnings are fatal.
# Optional: QML_IMPORT_PATH=<dir containing QindaTK> to test against a newer QindaTK than installed.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
b="$root/build/phone/preview"
cmake -S "$root/phone/preview" -B "$b" -G Ninja -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$b"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=${QT_QUICK_BACKEND:-software} QT_FATAL_WARNINGS=1 \
    "$b/qindalink-preview" --smoke "$@"
