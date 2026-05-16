#!/bin/bash
# Recover NUC123 keyboard: force LDROM bootloader, optional APROM erase.
# Run on the Pi wired to SWD (192.168.1.105) or locally with nuc123.cfg in PATH.
set -euo pipefail

CFG="${NUC123_CFG:-$HOME/nuc123.cfg}"
OPENOCD="${OPENOCD:-sudo openocd}"

usage() {
    echo "Usage: $0 [ldrom|halt|erase-aprom|flash-ldrom PATH.bin]"
    exit 1
}

[[ -f "$CFG" ]] || { echo "Missing $CFG"; exit 1; }

case "${1:-ldrom}" in
    ldrom)
        echo "=== Force boot from LDROM (only works if LDROM has a bootloader) ==="
        $OPENOCD -f "$CFG" -c "init" -c "halt" -c "mdw 0x00100000 1" -c "SysReset ldrom run" -c "shutdown"
        echo "If LDROM was erased, use: $0 recover-full [bootloader.bin]"
        ;;
    halt)
        echo "=== Halt and show state ==="
        $OPENOCD -f "$CFG" -c "init" -c "reset halt" -c "mdw 0x00000000 4" -c "mdw 0x00000180 4" -c "ReadConfigRegs" -c "shutdown" 2>&1 | \
            grep -E "pc:|xPSR|0x00000000|Config"
        ;;
    erase-aprom)
        echo "WARNING: ChipErase wipes ALL flash (APROM + LDROM + config)!"
        echo "Use recover-full instead."
        exit 1
        ;;
    recover-full)
        BIN="${2:-$HOME/nuc123-dfu-bootloader.bin}"
        echo "=== Restore config, flash LDROM, boot DFU ==="
        $OPENOCD -f "$CFG" -c "init" -c "halt" \
            -c "WriteConfigRegs 0xFFFFFF7E 0xFFFFFFFF" \
            -c "program $BIN 0x00100000" \
            -c "SysReset ldrom run" -c "shutdown"
        echo "Unplug/replug USB; lsusb -d 0416:bdf0"
        ;;
    flash-ldrom)
        [[ -n "${2:-}" ]] || usage
        BIN="$(realpath "$2")"
        echo "=== Flash LDROM: $BIN ==="
        $OPENOCD -f "$CFG" -c "init" -c "halt" \
            -c "program $BIN 0x00100000" -c "SysReset ldrom run" -c "shutdown"
        ;;
    *)
        usage
        ;;
esac
