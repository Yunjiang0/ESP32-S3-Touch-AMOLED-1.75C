#!/bin/bash
# ============================================================================
# ESP-Brookesia + Wallpaper App 一键打包烧录脚本 (Linux/macOS)
#
# Usage:
#   ./build_and_flash.sh                    - auto-detect port
#   ./build_and_flash.sh /dev/ttyUSB0       - use specified port
#   ./build_and_flash.sh /dev/ttyUSB0 build - only build, skip flash
#   ./build_and_flash.sh /dev/ttyUSB0 monitor - build, flash, then monitor
#
# Prerequisites:
#   - ESP-IDF v5.x installed and sourced (. ~/esp/esp-idf/export.sh)
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"
echo "[setup] project dir: $PROJECT_DIR"

PORT="${1:-}"
MODE="${2:-flash}"

# Auto-detect port if not given
if [ -z "$PORT" ]; then
    echo "[auto-detect] scanning for serial ports ..."
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        ls -1 /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/tty.usbserial* 2>/dev/null || true
    else
        # Linux
        ls -1 /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true
    fi
    echo ""
    echo "Re-run with the port as the first argument, e.g.:"
    echo "  $0 /dev/ttyUSB0"
    echo "  $0 /dev/cu.usbserial-110"
    exit 1
fi
echo "[setup] using $PORT"

# Check if ESP-IDF is sourced
if [ -z "$IDF_PATH" ]; then
    echo "[error] ESP-IDF not sourced. Run:"
    echo "  . ~/esp/esp-idf/export.sh"
    exit 1
fi

# Set target if needed
if [ ! -f "sdkconfig" ]; then
    echo "[setup] sdkconfig not found, running set-target esp32s3 ..."
    idf.py set-target esp32s3
fi

# Build
echo ""
echo "=== BUILD ====================================================="
idf.py build
echo "[build] OK"

if [ "$MODE" = "build" ]; then
    echo ""
    echo "=== BUILD ONLY - done ========================================"
    exit 0
fi

# Flash
echo ""
echo "=== FLASH ====================================================="
idf.py -p "$PORT" flash
echo "[flash] OK"

if [ "$MODE" = "flash" ]; then
    echo ""
    echo "=== ALL DONE =================================================="
    exit 0
fi

# Monitor
echo ""
echo "=== MONITOR ==================================================="
echo "Press Ctrl+] to exit."
idf.py -p "$PORT" monitor
