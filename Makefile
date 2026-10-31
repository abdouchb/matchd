# Zarqaa OS Makefile
.DEFAULT_GOAL := all

ifeq ($(OS),Windows_NT)
    $(warning Build might fail on Windows without a Unix-like environment (WSL, MSYS2, Git Bash) due to Unix commands like mkdir, rm, cp.)
endif

ifeq ($(OS),Windows_NT)
    LLVM_PATH ?= C:/Program Files/LLVM/bin/
    CC = "$(LLVM_PATH)clang.exe"
    LD = "$(LLVM_PATH)ld.lld.exe"
    AR = "$(LLVM_PATH)llvm-ar.exe"
else
    CC = clang
    LD = ld.lld
    AR = llvm-ar
endif

# Compiler Flags
# -nostdinc keeps host libc headers (/usr/include) out of the freestanding build
CLANG_INC := $(shell $(CC) -print-resource-dir)/include

CFLAGS = -target x86_64-pc-none-elf -nostdinc -isystem "$(CLANG_INC)" \
         -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-pic -fno-pie \
         -m64 -march=x86-64 -mno-80387 -mno-mmx -msse -msse2 -mno-red-zone \
         -Wall -Wextra -isystem include/compat -Iinclude -Ilibs/lexbor_src/source \
         -Idesktop/applications/browser/include -Ilibs/lwip/src/include -Iinclude/net \
         -Ilibs/mbedtls/include -Ilibs/uACPI/include -Ilibs/sqlite \
         -DSQLITE_OS_OTHER=1 -DSQLITE_TEMP_STORE=3 -DSQLITE_NO_SYNC=1 -DSQLITE_OMIT_FLOATING_POINT=1 -DSQLITE_OMIT_DATETIME_FUNCS=1 \
         -include string.h -include stdlib.h \
         -mcmodel=kernel

# Linker Flags
LDFLAGS = -nostdlib -static -no-pie -z text -z max-page-size=0x1000 -T linker.ld

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Source Files
C_SOURCES = kernel/main.c \
            kernel/string.c \
            kernel/printf.c \
            kernel/gdt.c \
            kernel/idt.c \
            kernel/irq.c \
            kernel/hardware_sensors.c \
            kernel/pci.c \
            kernel/pic.c \
            kernel/pmm.c \
            kernel/vmm.c \
            kernel/heap.c \
            kernel/libgcc_stub.c \
            kernel/scheduler.c \
            kernel/syscall.c \
            kernel/syslog.c \
            kernel/driver_manager.c \
            kernel/early_boot_log.c \
            kernel/compat/posix_syscalls.c \
            kernel/smp.c \
            kernel/apic.c \
            kernel/cpu_local.c \
            kernel/acpi.c \
            kernel/acpi_os.c \
            kernel/elf.c

ASM_SOURCES = kernel/isr.asm \
              kernel/setjmp.asm

# uACPI
UACPI_SOURCES = $(wildcard libs/uACPI/source/*.c)
C_SOURCES += $(UACPI_SOURCES)

# Drivers
C_SOURCES += drivers/serial/serial.c \
             drivers/timer/pit.c \
             drivers/framebuffer/framebuffer.c \
             drivers/framebuffer/font.c \
             drivers/gpu/gpu_hal.c \
             drivers/gpu/gpu_renderer.c \
             drivers/gpu/intel_gma.c \
             drivers/gpu/virtio_gpu.c \
             drivers/virtio/virtqueue.c \
             drivers/network/network.c \
             drivers/network/rtl8139.c \
             drivers/network/rtl8168.c \
             drivers/network/ethernetif.c \
             drivers/storage/ata.c \
             drivers/storage/ahci.c \
             drivers/audio/pc_speaker.c \
             drivers/audio/ac97.c \
             drivers/audio/hda.c \
             drivers/usb/xhci.c \
             drivers/usb/usb_core.c \
             drivers/usb/usb_hid.c \
             drivers/usb_handoff.c

# lwIP
LWIP_SOURCES = $(wildcard libs/lwip/src/core/*.c) \
               $(wildcard libs/lwip/src/core/ipv4/*.c) \
               $(wildcard libs/lwip/src/netif/ethernet.c)
C_SOURCES += $(LWIP_SOURCES)

# mbedTLS
MBEDTLS_SOURCES = $(wildcard libs/mbedtls/library/*.c)
C_SOURCES += $(MBEDTLS_SOURCES)

# SQLite
C_SOURCES += libs/sqlite/sqlite3.c \
             libs/sqlite/os_zarqaa.c

# GUI engine
C_SOURCES += kernel/gui/graphics.c \
             kernel/gui/widget.c \
             kernel/gui/layer.c \
             kernel/gui/dirty_rect.c \
             gui/image.c \
             gui/zml_smart_renderer.c \
             gui/bitmap_icons.c \
             gui/wallpaper_thumbnails.c \
             gui/wallpaper.c
# ZUI Library (Zarqaa UI API)
C_SOURCES += libzui/zui.c \
             libzui/zui_button.c \
             libzui/zui_label.c \
             libzui/zui_panel.c \
             libzui/zui_window.c \
             libzui/zui_html_parser.c

# HTML Engine
C_SOURCES += libhtml/dom.c \
             libhtml/parser.c \
             libhtml/css.c \
             libhtml/layout.c \
             libhtml/render.c \
             libhtml/svg_parser.c \
             libhtml/scene_builder.c

# Javascript Engine (Duktape) - use shim header to satisfy freestanding stdlib requirements
DUK_SOURCES = libs/duktape/duktape.c
# CRITICAL: Do NOT include -isystem include/compat here - the shim replaces all of those
DUK_FLAGS   = -target x86_64-pc-none-elf -ffreestanding -fno-stack-protector \
              -fno-lto -fno-pic -fno-pie -m64 -march=x86-64 -mno-80387 -mno-mmx \
              -msse -msse2 -mno-red-zone -mcmodel=kernel \
              -Ilibs/duktape -Iinclude \
              -include libs/duktape/zarqaa_duk_shim.h \
              -Wno-unused-parameter -Wno-sign-compare \
              -Wno-missing-field-initializers -Wno-redundant-decls \
              -Wno-implicit-function-declaration -Wno-error

# Compile duktape.c with its own special flags
DUK_OBJ = $(OBJ_DIR)/libs/duktape/duktape.o

# js_engine.c - compiled with special DUK_FLAGS rule (not via C_SOURCES)
# C_SOURCES += kernel/js/js_engine.c   <- intentionally excluded
CFLAGS += -Ilibs/duktape

# Graphics API (Client-side & Unified)
C_SOURCES += libzargraphics/zargraphics.c \
             libzargraphics/graphics_api.c \
             libzargraphics/render_queue.c \
             kernel/graphics/software_renderer.c \
             kernel/graphics/hardware_renderer.c

# Vector Engine (Zarqaa OS Renderer 3.0)
C_SOURCES += graphics/libvector/vector.c \
             graphics/libvector/bezier.c \
             graphics/libvector/path.c \
             graphics/libvector/shape.c \
             graphics/libvector/rasterizer.c \
             graphics/libvector/scene_graph.c

# Desktop environment
DESKTOP_SRCS = \
	desktop/desktop.c \
	desktop/taskbar.c \
	desktop/window_manager.c \
	desktop/icons.c \
	desktop/cursor.c \
	desktop/applications/terminal.c \
	desktop/applications/file_manager.c \
	desktop/applications/text_editor.c \
	desktop/applications/task_manager.c \
	desktop/applications/settings.c \
	desktop/applications/calculator.c \
	desktop/applications/power_menu.c \
	desktop/applications/wifi_manager.c \
	desktop/applications/paint.c \
	desktop/applications/python_ide.c \
	desktop/applications/debug_console.c \
	desktop/applications/audio_player.c \
	desktop/applications/zar_test_app.c \
	desktop/applications/boot_logger.c \
	desktop/applications/device_manager.c \
	desktop/applications/image_viewer.c \
	desktop/applications/benchmark.c \
	desktop/applications/clock_app.c \
	desktop/applications/calendar_app.c \
	desktop/applications/sysinfo_app.c \
	desktop/applications/game2048.c \
	desktop/applications/minesweeper.c \
	gui/new_app_icons.c

C_SOURCES += $(DESKTOP_SRCS)

# Advanced Modular Browser
BROWSER_SOURCES = $(wildcard desktop/applications/browser/core/*.c) \
                  $(wildcard desktop/applications/browser/renderer/*.c) \
                  $(wildcard desktop/applications/browser/platform/*.c) \
                  $(wildcard desktop/applications/browser/ui/*.c)
C_SOURCES += $(BROWSER_SOURCES)

# Network User Libraries
C_SOURCES += libs/net/url.c \
             libs/net/http.c \
             libs/net/dns_manager.c \
             libs/resource/resource_manager.c

# Lexbor HTML Engine
LEXBOR_SOURCES = $(wildcard libs/lexbor_src/source/lexbor/*/*.c)
LEXBOR_SOURCES += $(wildcard libs/lexbor_src/source/lexbor/*/*/*.c)
LEXBOR_SOURCES += $(wildcard libs/lexbor_src/source/lexbor/*/*/*/*.c)
LEXBOR_SOURCES += $(wildcard libs/lexbor_src/source/lexbor/*/*/*/*/*.c)
LEXBOR_SOURCES += libs/lexbor_src/source/lexbor_port.c
LEXBOR_SOURCES := $(filter-out libs/lexbor_src/source/lexbor/utils/warc.c, $(LEXBOR_SOURCES))
LEXBOR_SOURCES := $(filter-out libs/lexbor_src/source/lexbor/ports/%, $(LEXBOR_SOURCES))
C_SOURCES += $(LEXBOR_SOURCES)

# Filesystem
C_SOURCES += filesystem/vfs.c \
             filesystem/ramfs.c \
             filesystem/fat.c \
             filesystem/tar.c

# Object Files
C_OBJECTS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst %.asm, $(OBJ_DIR)/%.o, $(ASM_SOURCES))
JS_ENGINE_OBJ = $(OBJ_DIR)/kernel/js/js_engine.o
OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS) $(DUK_OBJ) $(JS_ENGINE_OBJ)

# User Space Applications
USR_DIR = usr
USR_LIBC_SRC = $(USR_DIR)/src/malloc.c $(USR_DIR)/src/printf.c $(USR_DIR)/src/string.c $(USR_DIR)/src/stdlib.c
USR_LIBC_OBJ = $(BUILD_DIR)/obj/malloc.o $(BUILD_DIR)/obj/printf.o $(BUILD_DIR)/obj/string.o $(BUILD_DIR)/obj/stdlib.o
USR_LIBC = $(BUILD_DIR)/libc.a

USR_BINS = fat_root/hello.elf fat_root/sh.elf fat_root/calc.elf fat_root/snake.elf

USR_CFLAGS = -target x86_64-pc-none-elf -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-pic -fno-pie -m64 -march=x86-64 -mno-80387 -mno-mmx -msse -msse2 -mno-red-zone -Wall -Wextra -I$(USR_DIR)/include

$(BUILD_DIR)/obj/%.o: $(USR_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(USR_CFLAGS) -c $< -o $@

$(USR_LIBC): $(USR_LIBC_OBJ)
	$(AR) rcs $@ $^

fat_root/%.elf: $(BUILD_DIR)/obj/%.o $(USR_LIBC)
	$(LD) -nostdlib -static -z max-page-size=0x1000 $< $(USR_LIBC) -o $@

# Targets
all: $(BUILD_DIR)/zarqaaos.iso $(USR_BINS)
iso: $(BUILD_DIR)/zarqaaos.iso

# Build ISO
$(BUILD_DIR)/zarqaaos.iso: $(BUILD_DIR)/zarqaaos.elf $(USR_BINS)
	@echo "=== Building ISO ==="
	@mkdir -p $(BUILD_DIR)/iso_root/boot
	@mkdir -p $(BUILD_DIR)/iso_root/EFI/BOOT
	@cp $(BUILD_DIR)/zarqaaos.elf $(BUILD_DIR)/iso_root/boot/
	@cp "desktop/boot image/Zarqaa_boot.png" $(BUILD_DIR)/iso_root/boot/boot_bg.png
	@echo "=== Creating Initrd (Ramdisk) ==="
	@cd fat_root && tar -cf ../$(BUILD_DIR)/initrd.tar *
	@cp $(BUILD_DIR)/initrd.tar $(BUILD_DIR)/iso_root/boot/
	@cp limine.conf $(BUILD_DIR)/iso_root/
	@cp limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin $(BUILD_DIR)/iso_root/
	@cp limine/BOOTX64.EFI $(BUILD_DIR)/iso_root/EFI/BOOT/
	@cp limine/BOOTIA32.EFI $(BUILD_DIR)/iso_root/EFI/BOOT/
	@xorriso -as mkisofs -b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(BUILD_DIR)/iso_root -o $(BUILD_DIR)/zarqaaos.iso
	@limine/limine bios-install $(BUILD_DIR)/zarqaaos.iso
	@echo "=== ISO created: $(BUILD_DIR)/zarqaaos.iso ==="

# Link Kernel
$(BUILD_DIR)/zarqaaos.elf: $(OBJECTS)
	@echo "=== Linking Kernel ==="
	@mkdir -p $(BUILD_DIR)
	@$(LD) $(LDFLAGS) $(OBJECTS) -o $(BUILD_DIR)/zarqaaos.elf

# Compile C
$(OBJ_DIR)/%.o: %.c
	@echo "[CC]  $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# Special rule: Compile Duktape with its own isolated flags (no -Wall conflicts)
$(DUK_OBJ): libs/duktape/duktape.c libs/duktape/zarqaa_duk_shim.h
	@echo "[CC]  libs/duktape/duktape.c (freestanding mode)"
	@mkdir -p $(dir $(DUK_OBJ))
	@$(CC) $(DUK_FLAGS) -c libs/duktape/duktape.c -o $(DUK_OBJ)

# Special rule: Compile js_engine.c with same isolated flags (no isystem compat)
JS_ENGINE_OBJ = $(OBJ_DIR)/kernel/js/js_engine.o
$(JS_ENGINE_OBJ): kernel/js/js_engine.c libs/duktape/zarqaa_duk_shim.h
	@echo "[CC]  kernel/js/js_engine.c (duktape mode)"
	@mkdir -p $(dir $(JS_ENGINE_OBJ))
	@$(CC) $(DUK_FLAGS) -c kernel/js/js_engine.c -o $(JS_ENGINE_OBJ)

# Compile ASM
$(OBJ_DIR)/%.o: %.asm
	@echo "[ASM] $<"
	@mkdir -p $(dir $@)
	@nasm -f elf64 $< -o $@

clean:
	@echo "=== Cleaning build artifacts ==="
	@rm -rf $(BUILD_DIR)
	@echo "=== Clean complete ==="

run: $(BUILD_DIR)/zarqaaos.iso
	@mkdir -p fat_root/www
	@echo "This is a test file to prove the FAT driver works!" > fat_root/test.txt
	@cp -r www/* fat_root/www/
	"/c/Program Files/qemu/qemu-system-x86_64.exe" -accel tcg,thread=multi -smp 2 -cpu max -m 2G -M q35 -bios ovmf/OVMF.fd -device virtio-vga -cdrom $(BUILD_DIR)/zarqaaos.iso -drive file=fat:rw:fat_root,format=raw,media=disk,if=ide,index=0 -no-reboot -no-shutdown -serial stdio -serial tcp:127.0.0.1:4444,server,nowait -netdev user,id=u1 -device rtl8139,netdev=u1 -audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0 -device AC97,audiodev=snd0
