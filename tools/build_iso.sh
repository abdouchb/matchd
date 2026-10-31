#!/usr/bin/env bash
# Zarqaa OS ISO maker: smart clean -> build -> verify -> publish to dist/.
#   tools/build_iso.sh               incremental build; cleans only what is stale
#   tools/build_iso.sh clean         force a full rebuild
#   tools/build_iso.sh --no-publish  build + verify only (used by run_os.bat)
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD=build
DIST=dist
KEEP=3                         # ISOs kept in dist/
CONFIG_STAMP=$BUILD/.zarqaa-config
SOURCE_HASHES=$BUILD/.zarqaa-sources
LOG=$BUILD/build.log
ISO=$BUILD/zarqaaos.iso
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}
START=$(date +%s)

blue=$'\033[1;34m'; green=$'\033[32m'; yellow=$'\033[33m'; red=$'\033[1;31m'; off=$'\033[0m'
step() { printf '\n%s==> %s%s\n' "$blue" "$*" "$off"; }
info() { printf '    %s\n' "$*"; }
ok()   { printf '    %sOK%s %s\n' "$green" "$off" "$*"; }
warn() { printf '    %s!%s  %s\n' "$yellow" "$off" "$*"; }
die()  { printf '\n%sERROR:%s %s\n' "$red" "$off" "$*" >&2; exit 1; }

FORCE_CLEAN=0
PUBLISH=1
for arg in "$@"; do
    case "$arg" in
        clean|--clean) FORCE_CLEAN=1 ;;
        --no-publish)  PUBLISH=0 ;;
        -h|--help)     sed -n '2,5p' "$0"; exit 0 ;;
        *)             die "unknown option '$arg' (see --help)" ;;
    esac
done

# --------------------------------------------------------------------------
step "Checking toolchain"
missing=()
for tool in make clang ld.lld llvm-ar nasm xorriso tar md5sum sha256sum od; do
    command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
done
[ ${#missing[@]} -eq 0 ] || die "missing tools: ${missing[*]}"
if [ ! -x limine/limine ] && [ ! -f limine/limine.exe ]; then
    info "building the limine host tool"
    make -C limine >/dev/null 2>&1 || die "could not build limine/limine"
fi
ok "$(clang --version | head -1)"

# --------------------------------------------------------------------------
# Timestamps can't be trusted (unzipping an old archive over a newer build/
# leaves objects that look up to date), so staleness is decided by content.
step "Smart clean"
config=$( { clang --version | head -1; ld.lld --version | head -1; nasm -v
            md5sum Makefile linker.ld; } 2>&1 | md5sum | cut -c1-32 )

new_hashes=$(mktemp)
trap 'rm -f "$new_hashes"' EXIT
find . \( -path ./build -o -path ./dist -o -path ./.git \) -prune -o -type f \
       \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \
          -o -name '*.asm' -o -name '*.inc' \) -print0 \
    | xargs -0 md5sum | LC_ALL=C sort > "$new_hashes"

reason=""
if [ "$FORCE_CLEAN" = 1 ]; then
    reason="requested"
elif [ -d "$BUILD" ] && { [ ! -f "$CONFIG_STAMP" ] || [ ! -f "$SOURCE_HASHES" ]; }; then
    reason="existing build/ has no build record, its state is unknown"
elif [ -f "$CONFIG_STAMP" ] && [ "$(cat "$CONFIG_STAMP")" != "$config" ]; then
    reason="compiler, Makefile or linker script changed"
fi

if [ -n "$reason" ]; then
    warn "full clean: $reason"
    rm -rf "$BUILD"
elif [ -d "$BUILD" ]; then
    # Touch only files whose content changed; make + .d deps rebuild the rest
    changed=$(LC_ALL=C comm -13 "$SOURCE_HASHES" "$new_hashes" | cut -c35-)
    count=0
    if [ -n "$changed" ]; then
        while IFS= read -r file; do
            [ -f "$file" ] && touch -- "$file" && count=$((count + 1))
        done <<< "$changed"
    fi
    if [ "$count" -gt 0 ]; then info "$count changed source file(s) will be rebuilt"
    else ok "sources unchanged since the last build"; fi
else
    info "first build"
fi
# Always repackage: the ISO also depends on fat_root/, limine.conf and images
rm -rf "$BUILD/iso_root" "$ISO" "$BUILD/initrd.tar"

# --------------------------------------------------------------------------
step "Building (make -j$JOBS)"
mkdir -p "$BUILD"
set +e
make -j"$JOBS" all 2>&1 | tee "$LOG" | grep --line-buffered -E '^\[(CC|ASM)\]' | sed 's/^/    /'
status=${PIPESTATUS[0]}
set -e
if [ "$status" -ne 0 ]; then
    echo
    grep -E 'error:|Error [0-9]|No such file' "$LOG" | head -20 | sed 's/^/    /'
    die "build failed - full log: $LOG"
fi
ok "$(grep -cE '^\[(CC|ASM)\]' "$LOG" || true) file(s) compiled"

# --------------------------------------------------------------------------
step "Verifying ISO"
[ -f "$ISO" ] || die "make finished but $ISO was not produced"
size=$(wc -c < "$ISO")
[ "$size" -gt 1048576 ] || die "ISO is suspiciously small ($size bytes)"
[ "$(head -c 4 "$BUILD/zarqaaos.elf" | od -An -tx1 | tr -d ' \n')" = 7f454c46 ] \
    || die "kernel is not an ELF file"
[ "$(od -An -tx1 -j510 -N2 "$ISO" | tr -d ' \n')" = 55aa ] \
    || die "ISO has no MBR boot signature (not USB-bootable)"
[ "$(od -An -c -j512 -N8 "$ISO" | tr -d ' \n')" = "EFIPART" ] \
    || die "ISO has no GPT header (UEFI USB boot would fail)"
listing=$(xorriso -indev "$ISO" -find / -type f 2>/dev/null)
for f in /boot/zarqaaos.elf /boot/initrd.tar /limine.conf /limine-bios.sys \
         /limine-bios-cd.bin /limine-uefi-cd.bin /EFI/BOOT/BOOTX64.EFI; do
    grep -qF "'$f'" <<< "$listing" || die "ISO is missing $f"
done
ok "kernel, initrd and Limine BIOS + UEFI loaders present"
ok "hybrid MBR/GPT image: boots from CD, DVD or USB stick"

# --------------------------------------------------------------------------
printf '%s\n' "$config" > "$CONFIG_STAMP"
cp "$new_hashes" "$SOURCE_HASHES"

out=$ISO
if [ "$PUBLISH" = 1 ]; then
    step "Publishing"
    mkdir -p "$DIST"
    name=ZarqaaOS-$(date +%Y%m%d-%H%M%S).iso
    cp "$ISO" "$DIST/$name"
    (cd "$DIST" && sha256sum "$name" > "$name.sha256")
    ls -1t "$DIST"/ZarqaaOS-*.iso 2>/dev/null | tail -n +$((KEEP + 1)) | while IFS= read -r old; do
        rm -f -- "$old" "$old.sha256"
        info "removed old $(basename "$old")"
    done
    out=$DIST/$name
fi

sha=$(sha256sum "$out" | cut -c1-64)
printf '\n%sZarqaa OS ISO ready%s  (%ss)\n' "$green" "$off" "$(( $(date +%s) - START ))"
info "file   : $out"
command -v cygpath >/dev/null 2>&1 && info "windows: $(cygpath -w "$PWD/$out")"
info "size   : $((size / 1048576)) MB"
info "sha256 : $sha"
