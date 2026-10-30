#!/bin/bash
#
# Zarqaa OS 0.1 - Build Script
# Creator: Abdelhak Chabbi
#
# This script handles the complete build process:
#   1. Downloads Limine bootloader binaries if not present
#   2. Downloads OVMF UEFI firmware if not present
#   3. Builds the kernel
#   4. Creates the bootable ISO image
#
# Usage:
#   chmod +x scripts/build.sh
#   ./scripts/build.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

echo "========================================"
echo "  Zarqaa OS 0.1 - Build System"
echo "  Creator: Abdelhak Chabbi"
echo "========================================"
echo ""

# ===== Step 1: Check Dependencies =====

echo "[1/5] Checking build dependencies..."

check_cmd() {
    if ! command -v "$1" &> /dev/null; then
        echo "ERROR: '$1' is not installed. Please install it first."
        echo "  On Ubuntu/Debian: sudo apt install $2"
        echo "  On Arch: sudo pacman -S $2"
        return 1
    fi
    echo "  ✓ $1 found"
}

# Check for cross-compiler or system gcc
if command -v clang &> /dev/null; then
    echo "  ✓ clang found (cross-compiler)"
    export CC=clang
    export LD=ld.lld
elif command -v gcc &> /dev/null; then
    echo "  ⚠ Using system gcc (cross-compiler recommended)"
    echo "    To install: sudo apt install gcc-x86-64-linux-gnu"
    export CC=gcc
    export LD=ld
else
    echo "ERROR: No C compiler found!"
    echo "  Install gcc: sudo apt install gcc"
    echo "  Or cross-compiler: sudo apt install gcc-x86-64-linux-gnu"
    exit 1
fi

check_cmd nasm nasm
check_cmd xorriso xorriso
echo ""

# ===== Step 2: Download Limine =====

LIMINE_VERSION="8.6.0"
LIMINE_DIR="$PROJECT_DIR/limine"

if [ ! -d "$LIMINE_DIR" ] || [ ! -f "$LIMINE_DIR/limine-bios.sys" ]; then
    echo "[2/5] Downloading Limine bootloader v${LIMINE_VERSION}..."
    
    # Clone the Limine repository (binary branch)
    if [ -d "$LIMINE_DIR" ]; then
        rm -rf "$LIMINE_DIR"
    fi
    
    git clone --depth=1 --branch=v${LIMINE_VERSION}-binary \
        https://github.com/limine-bootloader/limine.git "$LIMINE_DIR" 2>/dev/null || {
        echo "  Trying alternative download method..."
        git clone --depth=1 \
            https://github.com/limine-bootloader/limine.git "$LIMINE_DIR" 2>/dev/null
    }
    
    # Build the limine utility
    if [ -f "$LIMINE_DIR/GNUmakefile" ]; then
        make -C "$LIMINE_DIR" 2>/dev/null || true
    fi
    
    echo "  ✓ Limine downloaded"
else
    echo "[2/5] Limine already present, skipping download."
fi
echo ""

# ===== Step 3: Download OVMF =====

OVMF_DIR="$PROJECT_DIR/ovmf"
OVMF_FILE="$OVMF_DIR/OVMF.fd"

if [ ! -f "$OVMF_FILE" ]; then
    echo "[3/5] Downloading OVMF UEFI firmware..."
    mkdir -p "$OVMF_DIR"
    
    # Try to copy system OVMF first
    if [ -f "/usr/share/ovmf/OVMF.fd" ]; then
        cp /usr/share/ovmf/OVMF.fd "$OVMF_FILE"
    elif [ -f "/usr/share/OVMF/OVMF.fd" ]; then
        cp /usr/share/OVMF/OVMF.fd "$OVMF_FILE"
    elif [ -f "/usr/share/edk2/ovmf/OVMF.fd" ]; then
        cp /usr/share/edk2/ovmf/OVMF.fd "$OVMF_FILE"
    else
        # Download from the retrage/edk2-nightly releases
        echo "  Downloading from GitHub..."
        OVMF_URL="https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF.fd"
        curl -L -o "$OVMF_FILE" "$OVMF_URL" 2>/dev/null || \
        wget -O "$OVMF_FILE" "$OVMF_URL" 2>/dev/null || {
            echo "  ⚠ Could not download OVMF. Install it manually:"
            echo "    sudo apt install ovmf"
            echo "    Then copy OVMF.fd to $OVMF_DIR/"
        }
    fi
    
    if [ -f "$OVMF_FILE" ]; then
        echo "  ✓ OVMF downloaded"
    fi
else
    echo "[3/5] OVMF already present, skipping download."
fi
echo ""

# ===== Step 4: Build Kernel =====

echo "[4/5] Building Zarqaa OS kernel..."
make clean 2>/dev/null || true
make all
echo ""

# ===== Step 5: Create ISO =====

echo "[5/5] Creating bootable ISO image..."
make iso
echo ""

# ===== Done =====

echo "========================================"
echo "  Build Complete!"
echo "========================================"
echo ""
echo "  Kernel:  build/kernel.elf"
echo "  ISO:     build/zarqaaos.iso"
echo ""
echo "  To run in QEMU:"
echo "    make run        (UEFI mode)"
echo "    make run-bios   (BIOS legacy mode)"
echo ""
echo "  To write to USB drive:"
echo "    sudo dd if=build/zarqaaos.iso of=/dev/sdX bs=4M status=progress"
echo ""
echo "========================================"
