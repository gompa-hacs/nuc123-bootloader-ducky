#!/bin/bash
#
# NUC123 DFU Bootloader Flash Script
# Uses OpenOCD with Raspberry Pi Zero GPIO (bcm2835gpio)
#
# Requirements:
#   - OpenOCD installed
#   - nuc123.cf config file in same directory
#   - nuc123-dfu-bootloader.bin (bootloader binary)
#   - SWD debugger connected to Raspberry Pi GPIO
#
# Usage:
#   ./flash_bootloader.sh [bootloader.bin]
#
# Default bootloader binary: nuc123-dfu-bootloader.bin

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${SCRIPT_DIR}/nuc123.cf"
BOOTLOADER_BIN="${1:-${SCRIPT_DIR}/nuc123-dfu-bootloader.bin}"
LDROM_ADDRESS="0x00100000"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    echo_info "Checking prerequisites..."
    
    # Check OpenOCD
    if ! command -v openocd &> /dev/null; then
        echo_error "OpenOCD not found. Please install OpenOCD first."
        exit 1
    fi
    echo_info "OpenOCD: $(openocd --version | head -n1)"
    
    # Check config file
    if [ ! -f "$CONFIG_FILE" ]; then
        echo_error "Config file not found: $CONFIG_FILE"
        exit 1
    fi
    echo_info "Config file: $CONFIG_FILE"
    
    # Check bootloader binary
    if [ ! -f "$BOOTLOADER_BIN" ]; then
        echo_error "Bootloader binary not found: $BOOTLOADER_BIN"
        exit 1
    fi
    echo_info "Bootloader binary: $BOOTLOADER_BIN ($(stat -c%s "$BOOTLOADER_BIN" 2>/dev/null || stat -f%z "$BOOTLOADER_BIN") bytes)"
    
    echo_info "All prerequisites met."
}

# Flash the bootloader to LDROM
flash_bootloader() {
    echo_info "Flashing bootloader to LDROM ($LDROM_ADDRESS)..."
    echo_warn "This will replace the factory ISP bootloader!"
    echo_warn "Press Ctrl+C to cancel or enter to continue..."
    read -r
    
    openocd -f "$CONFIG_FILE" \
        -c "init" \
        -c "halt" \
        -c "flash write_image erase \"$BOOTLOADER_BIN\" $LDROM_ADDRESS bin" \
        -c "verify_image \"$BOOTLOADER_BIN\" $LDROM_ADDRESS bin" \
        -c "reset run" \
        -c "shutdown"
    
    echo_info "Bootloader flashed successfully!"
}

# Verify the flashed bootloader
verify_bootloader() {
    echo_info "Verifying flashed bootloader..."
    
    openocd -f "$CONFIG_FILE" \
        -c "init" \
        -c "halt" \
        -c "mdw $LDROM_ADDRESS 8" \
        -c "shutdown"
    
    echo_info "Verification complete. Check the output above for the first 8 words at LDROM."
}

# Chip erase (emergency recovery)
chip_erase() {
    echo_warn "WARNING: This will erase ALL flash memory including APROM and LDROM!"
    echo_warn "This will remove both the bootloader AND any firmware!"
    echo_warn "Press Ctrl+C to cancel or enter to continue..."
    read -r
    
    openocd -f "$CONFIG_FILE" \
        -c "init" \
        -c "halt" \
        -c "ChipErase" \
        -c "shutdown"
    
    echo_info "Chip erased. The device is now blank and unlocked."
}

# Dump APROM (backup firmware)
dump_aprom() {
    OUTPUT_FILE="${1:-aprom_backup.bin}"
    echo_info "Dumping APROM to $OUTPUT_FILE..."
    
    openocd -f "$CONFIG_FILE" \
        -c "init" \
        -c "halt" \
        -c "DumpAPROM \"$OUTPUT_FILE\"" \
        -c "shutdown"
    
    echo_info "APROM dumped to $OUTPUT_FILE"
}

# Boot from LDROM (test bootloader)
boot_ldrom() {
    echo_info "Booting from LDROM..."
    
    openocd -f "$CONFIG_FILE" \
        -c "init" \
        -c "SysReset ldrom" \
        -c "shutdown"
    
    echo_info "Device reset, booting from LDROM."
}

# Boot from APROM (normal operation)
boot_aprom() {
    echo_info "Booting from APROM..."
    
    openocd -f "$CONFIG_FILE" \
        -c "init" \
        -c "SysReset aprom" \
        -c "shutdown"
    
    echo_info "Device reset, booting from APROM."
}

# Show usage
usage() {
    echo "NUC123 DFU Bootloader Flash Script"
    echo ""
    echo "Usage: $0 <command> [options]"
    echo ""
    echo "Commands:"
    echo "  flash              Flash bootloader to LDROM (default if no command)"
    echo "  verify             Verify flashed bootloader"
    echo "  erase              Chip erase (emergency recovery)"
    echo "  dump [file]        Dump APROM to file (default: aprom_backup.bin)"
    echo "  boot-ldrom         Boot from LDROM (test bootloader)"
    echo "  boot-aprom         Boot from APROM (normal operation)"
    echo "  help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 flash                    # Flash bootloader"
    echo "  $0 flash custom.bin         # Flash custom bootloader"
    echo "  $0 verify                   # Verify flash"
    echo "  $0 erase                    # Chip erase"
    echo "  $0 dump mybackup.bin        # Dump APROM"
    echo "  $0 boot-ldrom               # Boot from LDROM"
    echo ""
    echo "Requirements:"
    echo "  - OpenOCD installed"
    echo "  - nuc123.cf config file"
    echo "  - nuc123-dfu-bootloader.bin (or specify custom path)"
    echo "  - SWD debugger connected"
}

# Main
main() {
    case "${1:-flash}" in
        flash)
            check_prerequisites
            flash_bootloader
            ;;
        verify)
            verify_bootloader
            ;;
        erase)
            chip_erase
            ;;
        dump)
            dump_aprom "$2"
            ;;
        boot-ldrom)
            boot_ldrom
            ;;
        boot-aprom)
            boot_aprom
            ;;
        help|--help|-h)
            usage
            ;;
        *)
            echo_error "Unknown command: $1"
            usage
            exit 1
            ;;
    esac
}

main "$@"