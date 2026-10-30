#!/bin/bash
#
# Zarqaa OS 0.1 - QEMU Launch Script
# Creator: Abdelhak Chabbi
#
# Launches Zarqaa OS in QEMU with UEFI firmware.
#
# Usage:
#   chmod +x scripts/run.sh
#   ./scripts/run.sh [--bios]
#
# Options:
#   --bios    Boot in BIOS legacy mode (no OVMF needed)
#   --debug   Enable GDB debugging on port 1234
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

ISO_FILE="build/zarqaaos.iso"
OVMF_FILE="ovmf/OVMF.fd"

# Check if ISO exists
if [ ! -f "$ISO_FILE" ]; then
    echo "ISO not found. Building first..."
    ./scripts/build.sh
fi

# Parse arguments
BIOS_MODE=false
DEBUG_MODE=false

for arg in "$@"; do
    case $arg in
        --bios)  BIOS_MODE=true ;;
        --debug) DEBUG_MODE=true ;;
    esac
done

# Build QEMU command
QEMU_CMD="qemu-system-x86_64"
QEMU_ARGS=(
    -M q35
    -m 256M
    -cdrom "$ISO_FILE"
    -serial stdio
    -no-reboot
    -no-shutdown
    -usb
    -device usb-tablet
)

# Add UEFI firmware unless BIOS mode
if [ "$BIOS_MODE" = false ]; then
    if [ ! -f "$OVMF_FILE" ]; then
        echo "ERROR: OVMF firmware not found at $OVMF_FILE"
        echo "Run scripts/build.sh first to download it,"
        echo "or use --bios flag for BIOS boot."
        exit 1
    fi
    QEMU_ARGS+=(-bios "$OVMF_FILE")
    echo "=== Launching Zarqaa OS (UEFI mode) ==="
else
    echo "=== Launching Zarqaa OS (BIOS mode) ==="
fi

# Add debug options
if [ "$DEBUG_MODE" = true ]; then
    QEMU_ARGS+=(-s -S)
    echo "  GDB server listening on localhost:1234"
    echo "  Connect with: gdb -ex 'target remote :1234' build/kernel.elf"
fi

echo ""

# Launch QEMU
exec $QEMU_CMD "${QEMU_ARGS[@]}"
