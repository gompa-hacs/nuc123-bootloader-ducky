#!/bin/bash
# Recover NUC123 keyboard via SWD (Pi or local OpenOCD + nuc123.cfg).
set -euo pipefail

CFG="${NUC123_CFG:-$HOME/nuc123.cfg}"
OPENOCD="${OPENOCD:-sudo openocd}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
    echo "Usage: $0 [recover|recover-hex|ldrom|halt|erase-aprom|check]"
    echo "  recover      ProgramLDROMBin .bin + config (try first)"
    echo "  recover-hex  ProgramLDROM .hex + config"
    echo "  recover-isp  WriteLDROM .bin via ISP (fallback)"
    echo "  check        mdw LDROM/APROM vectors"
    exit 1
}

[[ -f "$CFG" ]] || { echo "Missing $CFG — copy from $REPO_DIR/nuc123.cfg"; exit 1; }

HEX="${REPO_DIR}/nuc123-dfu-bootloader.hex"
BIN="${REPO_DIR}/nuc123-dfu-bootloader.bin"

build_dfu_always() {
    echo "=== Building make dfu-always (USB always on, no Esc needed) ==="
    make -C "$REPO_DIR" clean dfu-always
}

case "${1:-recover-hex}" in
    check)
        $OPENOCD -f "$CFG" -c "init" -c "halt" \
            -c "CheckLDROM" -c "mdw 0x00000000 2" -c "ReadConfigRegs" -c "shutdown"
        ;;
    ldrom)
        $OPENOCD -f "$CFG" -c "init" -c "halt" -c "CheckLDROM" \
            -c "SysReset ldrom halt" -c "reg pc" -c "reg msp" -c "shutdown"
        ;;
    halt)
        $OPENOCD -f "$CFG" -c "init" -c "halt" -c "CheckLDROM" \
            -c "mdw 0x00000000 2" -c "ReadConfigRegs" -c "reg pc" -c "shutdown"
        ;;
    erase-aprom)
        $OPENOCD -f "$CFG" -c "EraseAPROM" -c "shutdown"
        ;;
    recover-hex)
        HEX="${2:-$HEX}"
        build_dfu_always
        [[ -f "$HEX" ]] || { echo "Missing $HEX"; exit 1; }
        echo "=== Config + ProgramLDROM (hex, no address offset) ==="
        $OPENOCD -f "$CFG" \
            -c "WriteConfigRegs 0xFFFFFF7E 0xFFFFFFFF" \
            -c "ProgramLDROM $HEX" \
            -c "shutdown"
        echo "Unplug SWD. Plug USB only. Run: lsusb -d 0416:bdf0"
        ;;
    recover|recover-bin)
        BIN="${2:-$BIN}"
        build_dfu_always
        [[ -f "$BIN" ]] || { echo "Missing $BIN"; exit 1; }
        echo "=== Config + ProgramLDROMBin ==="
        $OPENOCD -f "$CFG" \
            -c "WriteConfigRegs 0xFFFFFF7E 0xFFFFFFFF" \
            -c "ProgramLDROMBin $BIN" \
            -c "shutdown"
        echo "Unplug SWD. Plug USB only. Run: lsusb -d 0416:bdf0"
        ;;
    recover-isp)
        BIN="${2:-$BIN}"
        build_dfu_always
        [[ -f "$BIN" ]] || { echo "Missing $BIN"; exit 1; }
        echo "=== Config + WriteLDROM (ISP) ==="
        $OPENOCD -f "$CFG" \
            -c "WriteConfigRegs 0xFFFFFF7E 0xFFFFFFFF" \
            -c "WriteLDROM $BIN" \
            -c "shutdown"
        echo "If this fails, try: $0 recover-bin $BIN"
        ;;
    *)
        usage
        ;;
esac
