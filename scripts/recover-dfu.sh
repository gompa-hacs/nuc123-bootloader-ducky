#!/bin/bash
# NUC123 Ducky One 2 SF — SWD recover + LDROM flash
set -euo pipefail

CFG="${NUC123_CFG:-$HOME/nuc123.cfg}"
OPENOCD="${OPENOCD:-sudo openocd}"

usage() {
    cat <<'EOF'
Usage:
  recover-dfu.sh check
  recover-dfu.sh recover [bin]       # default: nuc123-dfu-bootloader-recovery.bin (3576 B)
  recover-dfu.sh recover-good [bin]    # commit 466fc3d image (SP=0x20000400) — USB litmus test
  recover-dfu.sh live
  recover-dfu.sh usb

PC:
  make recovery && make known-good
  scp nuc123.cfg nuc123-dfu-bootloader-*.bin recover-dfu.sh pi@host:~/

Config must be 0xFFFFFF3E (LDROM boot). Old 0xFFFFFF7E boots APROM = no DFU on USB.

USB test: quit OpenOCD, unplug SWD, wait 5s, plug USB, lsusb -d 0416:bdf0
EOF
    exit 1
}

[[ -f "$CFG" ]] || { echo "Missing $CFG"; exit 1; }

case "${1:-recover}" in
    check)
        $OPENOCD -f "$CFG" -c "init" -c "halt" \
            -c "ReadConfigRegs" -c "CheckLDROM" -c "shutdown" 2>&1
        ;;
    recover)
        BIN="${2:-$HOME/nuc123-dfu-bootloader-recovery.bin}"
        [[ -f "$BIN" ]] || { echo "Missing $BIN — run: make recovery"; exit 1; }
        echo "=== Flash recovery LDROM: $BIN ==="
        md5sum "$BIN"
        [[ "$(wc -c < "$BIN")" -eq 3576 ]] || echo "WARNING: size is not 3576 bytes"
        $OPENOCD -f "$CFG" -c "init" -c "halt" \
            -c "WriteConfigRegs 0xFFFFFF3E 0xFFFFFFFF" \
            -c "ProgramLDROMBin $BIN strict" \
            -c "shutdown" 2>&1
        echo "IMPORTANT: Unplug SWD from keyboard NOW, then USB power-cycle."
        ;;
    recover-good)
        BIN="${2:-$HOME/nuc123-dfu-bootloader-known-good.bin}"
        [[ -f "$BIN" ]] || { echo "Missing $BIN — run: make known-good"; exit 1; }
        echo "=== Flash KNOWN-GOOD (466fc3d) LDROM — hold Esc if USB does not appear ==="
        md5sum "$BIN"
        $OPENOCD -f "$CFG" -c "init" -c "halt" \
            -c "WriteConfigRegs 0xFFFFFF3E 0xFFFFFFFF" \
            -c "ProgramLDROMBin $BIN known-good" \
            -c "shutdown"
        ;;
    live)
        $OPENOCD -f "$CFG" -c "init" -c "halt" \
            -c "CheckLDROM" \
            -c "SysReset ldrom run" -c "sleep 200" -c "halt" \
            -c "reg pc msp lr" -c "shutdown" 2>&1
        echo "pc must be 0x00100xxx; msp 0x20000800 (recovery) or 0x20000400 (known-good)"
        ;;
    usb)
        $OPENOCD -f "$CFG" -c "init" -c "halt" \
            -c "CheckLDROM" \
            -c "SysReset ldrom run" -c "sleep 300" -c "halt" \
            -c "reg pc msp" \
            -c "mdw 0x40060010 1" -c "mdw 0x40060014 1" \
            -c "shutdown" 2>&1
        ;;
    *)
        usage
        ;;
esac
