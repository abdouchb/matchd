/*
 * Zarqaa OS 0.1 - Kernel Main Entry Point
 * Creator: Abdelhak Chabbi
 */

#include "zarqaa/types.h"
#include "limine.h"
#include "drivers/serial.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "drivers/pit.h"
#include "gui/boot_splash.h"
#include "gui/dirty_rect.h"
#include "drivers/ata.h"
#include "drivers/ahci.h"
#include "drivers/hda.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"
#include "kernel/irq.h"
#include "kernel/pci.h"
#include "kernel/vmm.h"
#include "drivers/usb/usb_hid.h"
#include "drivers/usb/usb_hid.h"
#include "drivers/gpu.h"
#include "kernel/early_boot_log.h"

int gui_cpu_usage = 2;

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* Extern initializers (will be implemented by other subsystems) */
extern void gdt_init(void);
extern void idt_init(void);
extern void pic_init(void);
extern void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset);
extern void heap_init(uint64_t start_virt, size_t size);
extern void pit_init(uint32_t frequency);
extern void fb_init(struct limine_framebuffer *fb);
extern void keyboard_init(void);
extern void mouse_init(void);
extern void vfs_init(void);
extern void ramfs_init(void);
extern void desktop_init(void);
extern void desktop_update(void);
extern void network_init(void);
extern void network_tick(void);

#include "desktop/window_manager.h"
#include "gui/zml_smart_renderer.h"

static uint32_t g_current_fps = 60;
static uint32_t g_fps_frame_count = 0;
static uint64_t g_last_fps_ticks = 0;

uint32_t gui_get_fps(void) {
    return g_current_fps;
}

static zsr_compositor_t *zarqaa_ui;
static zsr_surface_t g_zarqaa_surface;

void gui_start(void)
{
    zarqaa_ui = zsr_get_desktop_compositor();

    g_zarqaa_surface.pixels = (uint32_t *)fb_get_backbuffer();
    g_zarqaa_surface.width = fb_get_width();
    g_zarqaa_surface.height = fb_get_height();
    g_zarqaa_surface.pitch = fb_get_pitch();

    zsr_init(zarqaa_ui, &g_zarqaa_surface);
    zsr_request_full_redraw(zarqaa_ui);
    zsr_render(zarqaa_ui);
}

static bool g_prev_left_down = false;

void gui_mouse_event(int mouse_x, int mouse_y, bool left_down, bool moved)
{
    if (!zarqaa_ui) return;

    if (left_down && !g_prev_left_down) {
        zsr_handle_mouse_down(zarqaa_ui, mouse_x, mouse_y);
    }

    if (moved) {
        zsr_handle_mouse_move(zarqaa_ui, mouse_x, mouse_y);
    }

    if (!left_down && g_prev_left_down) {
        zsr_handle_mouse_up(zarqaa_ui);
    }

    g_prev_left_down = left_down;
}
extern void ac97_init(void);
extern void driver_manager_init(void);

#include "kernel/scheduler.h"

/* No debug tasks */

/* ================================================================== */
/* FPU / SSE Initialization                                           */
/* ================================================================== */
static void cpu_init_fpu(void) {
    uint64_t cr0, cr4;
    
    /* Read CR0 */
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); /* Clear EM (Emulation) */
    cr0 |=  (1 << 1); /* Set MP (Monitor co-processor) */
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
    
    /* Read CR4 */
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  /* Set OSFXSR (SSE instruction support) */
    cr4 |= (1 << 10); /* Set OSXMMEXCPT (Unmasked SSE exceptions) */
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
    
    /* Initialize FPU */
    __asm__ volatile("fninit");
}

/* ================================================================== */
/* Limine Requests                                                    */
/* ================================================================== */

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kaddr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_boot_time_request boot_time_request = {
    .id = LIMINE_BOOT_TIME_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

/* ================================================================== */
/* Global State                                                       */
/* ================================================================== */

uint64_t hhdm_offset = 0;
int64_t boot_time = 0;

/* ================================================================== */
/* Kernel Entry Point                                                 */
/* ================================================================== */

void _start(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hang();
    }

    serial_init();
    cpu_init_fpu();
    kprintf("Zarqaa OS 0.1 booting...\n");

    if (hhdm_request.response != NULL) {
        hhdm_offset = hhdm_request.response->offset;
    } else {
        panic("HHDM request failed");
    }

    if (boot_time_request.response != NULL) {
        boot_time = boot_time_request.response->boot_time;
    }

    /* Hardware Init (no screen yet - these are silent) */
    gdt_init();
    idt_init();
    // pic_init() is removed. apic_init() will be called after ACPI is ready.
    pci_init();
    gpu_init();

    extern void pit_init(uint32_t frequency);
    pit_init(1000);
    register_irq_handler(0, pit_handler);
    // PS/2 IRQs removed for Native USB HID Support

    /* Memory Init */
    if (memmap_request.response != NULL) {
        pmm_init(memmap_request.response, hhdm_offset);
        vmm_init();
    } else {
        panic("Memory map request failed");
    }
    
    static uint8_t initial_heap[128 * 1024 * 1024] __attribute__((aligned(16)));
    heap_init((uint64_t)initial_heap, sizeof(initial_heap));
    
    /* Initialize SMP (Symmetric Multiprocessing) */
    extern void smp_init(void);
    smp_init();
    
    /* ACPI Init */
    extern void acpi_init(void);
    acpi_init();

    /* APIC Init (Replaces PIC) */
    extern void apic_init(void);
    apic_init();

    /* LAPIC Timer Init (Replaces PIT) */
    extern void lapic_timer_init(void);
    lapic_timer_init();

    /* Framebuffer Init */
    if (framebuffer_request.response != NULL && framebuffer_request.response->framebuffer_count > 0) {
        fb_init(framebuffer_request.response->framebuffers[0]);
    } else {
        panic("Framebuffer request failed");
    }

    /* ============================================================= */
    /* Early Boot Logger - NOW the screen is ready!                   */
    /* ============================================================= */
    early_blog_init();

    early_blog(BLOG_STEP, "CORE", "--- CPU & Core Hardware ---");
    early_blog(BLOG_OK, "CORE", "GDT initialized");
    early_blog(BLOG_OK, "CORE", "IDT initialized");
    early_blog(BLOG_OK, "CORE", "APIC initialized (Local APIC + IOAPIC)");
    extern void sensors_init(void);
    sensors_init();
    early_blog(BLOG_OK, "CORE", "Hardware Thermal & MSR Sensors initialized");
    early_blog(BLOG_OK, "PCI", "PCI bus enumerated");
    early_blog(BLOG_OK, "VIDEO", "GPU probed");
    early_blog(BLOG_OK, "CORE", "IRQ handlers registered (PIT/KB/Mouse)");

    early_blog(BLOG_STEP, "SYSTEM", "--- Memory Subsystem ---");
    early_blog(BLOG_OK, "MEMORY", "PMM (Physical Memory Manager) initialized");
    early_blog(BLOG_OK, "MEMORY", "Heap initialized (128 MB)");

    early_blog(BLOG_STEP, "TIMER", "--- Timer & Display ---");
    early_blog(BLOG_OK, "TIMER", "LAPIC Timer initialized");
    early_blog(BLOG_OK, "VIDEO", "Framebuffer active (boot logger running)");

    /* Input Init */
    early_blog(BLOG_INFO, "INPUT", "Initializing Native Input subsystem...");
    extern void input_init(void);
    input_init();
    
    early_blog(BLOG_INFO, "USB", "Initializing USB Core & HID...");
    extern void usb_core_init(void);
    usb_core_init();
    extern void usb_hid_init(void);
    usb_hid_init();

    sti(); /* Enable Interrupts - BEFORE driver_manager so IRQ timeouts fire */
    early_blog(BLOG_OK, "CORE", "CPU Interrupts ENABLED (STI)");
    kprintf("All subsystems initialized.\n");

    /* Now that memory+interrupts are ready, initialize hardware */
    early_blog(BLOG_STEP, "AUDIO", "--- Audio Drivers ---");
    early_blog(BLOG_INFO, "AUDIO", "Registering AC97 driver...");
    ac97_init();
    early_blog(BLOG_OK, "AUDIO", "AC97 driver registered");
    early_blog(BLOG_INFO, "AUDIO", "Registering HDA driver...");
    hda_init();
    extern void xhci_init(void);
    xhci_init();
    early_blog(BLOG_OK, "AUDIO", "HDA driver registered");
    /* Apply default volume to hardware (sync SW state -> HW) */
    extern void audio_init(void);
    audio_init();
    early_blog(BLOG_OK, "AUDIO", "Audio volume initialized (50%)");
    
    early_blog(BLOG_INFO, "FS/DSK", "Registering AHCI SATA driver...");
    ahci_init();
    early_blog(BLOG_OK, "FS/DSK", "AHCI SATA storage driver registered");
    
    early_blog(BLOG_STEP, "PCI", "--- PCI Device Probing (driver_manager) ---");
    early_blog(BLOG_INFO, "PCI", "Probing PCI devices and matching drivers...");
    extern bool usb_handoff_init(void);
    usb_handoff_init();
    extern void intel_gma_register(void);
    intel_gma_register();
    driver_manager_init();
    early_blog(BLOG_OK, "PCI", "PCI driver probing complete");

    /* Software Init */
    early_blog(BLOG_STEP, "SYSTEM", "--- File System ---");
    vfs_init();
    early_blog(BLOG_OK, "FS/DSK", "VFS initialized");
    ramfs_init();
    early_blog(BLOG_OK, "FS/DSK", "RamFS mounted");
    
    /* Storage Init */
    early_blog(BLOG_INFO, "FS/DSK", "Initializing ATA/IDE...");
    ata_init();
    early_blog(BLOG_OK, "FS/DSK", "ATA/IDE storage driver initialized");
    early_blog(BLOG_INFO, "FS/DSK", "Mounting FAT filesystem...");
    bool fat_ok = fat_init();
    if (fat_ok) {
        early_blog(BLOG_OK, "FS/DSK", "FAT filesystem mounted on /hd0");
    } else {
        early_blog(BLOG_WARN, "FS/DSK", "FAT mount failed - no disk or wrong format");
    }
    
    /* Test: List contents of hard disk (/hd0) */
    struct vfs_node *hd0 = vfs_resolve_path("/hd0");
    if (hd0) {
        kprintf("\n[FS TEST] Contents of Hard Disk (/hd0):\n");
        struct vfs_node *child = hd0->children;
        if (!child) {
            kprintf("    (Empty or not formatted)\n");
        }
        while (child) {
            kprintf("    -> %s (Size: %u bytes, Type: %s)\n", 
                    child->name, (uint32_t)child->size,
                    (child->type == VFS_DIRECTORY) ? "DIR" : "FILE");
            child = child->next;
        }
        kprintf("\n");
    } else {
        kprintf("[FS TEST] No hard disk mounted.\n");
    }
    
    if (module_request.response != NULL && module_request.response->module_count > 0) {
        struct limine_file *module = module_request.response->modules[0];
        extern void tar_init(void *archive_addr, size_t archive_size);
        tar_init(module->address, module->size);
        early_blog(BLOG_OK, "SYSTEM", "initrd (TAR) module loaded");
    } else {
        early_blog(BLOG_WARN, "FS/DSK", "No initrd module - using ATA/FAT only");
        kprintf("[WARN] No initrd module loaded. Using ATA/FAT only.\n");
    }

    early_blog(BLOG_STEP, "NET", "--- Network ---");
    early_blog(BLOG_INFO, "SYSTEM", "Initializing network subsystem...");
    network_init();
    early_blog(BLOG_OK, "NET", "Network subsystem initialized");

    scheduler_init();
    early_blog(BLOG_OK, "TASK", "Task Scheduler initialized");

    early_blog_sep();
    early_blog(BLOG_OK, "SYSTEM", "=== Boot complete! Launching Desktop... ===");
    early_blog_finish();

    /* Launch ZML Desktop Environment */
    fb_reset_render_target();
    gui_start();

    /* Main Event Loop Profiling */
    uint64_t work_cycles = 0;
    uint64_t total_cycles = 0;
    uint64_t last_tsc = rdtsc();
    uint64_t last_report_tsc = last_tsc;
    
    // Tracking state for Zero-Latency async input
    int last_mx = -1;
    int last_my = -1;
    bool last_left = false;
    uint64_t last_frame_ticks = pit_get_ticks();

    while (1) {
        uint64_t start_work = rdtsc();
        
        network_tick();
        
        // Zero-Latency check: Check if mouse moved, clicked or key pressed
        int mx = mouse_get_x();
        int my = mouse_get_y();
        bool left = mouse_left_pressed();
        
        bool mouse_moved = (mx != last_mx || my != last_my);
        bool click_changed = (left != last_left);
        bool key_pressed = keyboard_has_event();
        
        // 1. Process mouse and keyboard events immediately with zero input lag
        if (mouse_moved || click_changed) {
            gui_mouse_event(mx, my, left, mouse_moved);
            last_mx = mx;
            last_my = my;
            last_left = left;
        }

        if (key_pressed) {
            struct key_event ev;
            while (keyboard_poll_event(&ev)) {
                if (!ev.is_release) {
                    /* F8 Key toggles Hardware VSync on/off */
                    if (ev.scancode == 0x42) {
                        fb_set_vsync(!fb_get_vsync());
                    }
                    struct window *win = wm_get_focused();
                    if (win && win->on_key) {
                        win->on_key(win, ev.c, ev.scancode);
                        win->needs_redraw = true;
                    }
                }
            }
        }

        // 2. Uncapped FPS (بدون سقف) & VSync Engine
        // When VSync is OFF: renders immediately on input with NO ceiling / limit!
        // When VSync is ON: hardware port 0x3DA syncs frames to display refresh in fb_swap.
        uint64_t now_ticks = pit_get_ticks();
        bool should_render = mouse_moved || click_changed || key_pressed || (now_ticks - last_frame_ticks >= 16);
        if (should_render) {
            zsr_render(zarqaa_ui);
            last_frame_ticks = now_ticks;
            g_fps_frame_count++;
        }

        /* Calculate exact live FPS every second */
        if (now_ticks - g_last_fps_ticks >= 1000) {
            g_current_fps = (uint32_t)((g_fps_frame_count * 1000) / (now_ticks - g_last_fps_ticks));
            g_fps_frame_count = 0;
            g_last_fps_ticks = now_ticks;
        }
        
        uint64_t end_work = rdtsc();
        work_cycles += (end_work - start_work);
        
        // Wait for the next hardware interrupt (Timer, Mouse, Keyboard)
        __asm__ volatile("sti; hlt");
        
        uint64_t now_tsc = rdtsc();
        total_cycles = now_tsc - last_report_tsc;
        
        /* Update stats approx every ~500ms depending on CPU speed, or just by cycle count */
        /* Assuming a 2GHz CPU, 1 billion cycles is 500ms */
        if (total_cycles > 1000000000ULL) {
            gui_cpu_usage = (int)((work_cycles * 100) / total_cycles);
            if (gui_cpu_usage > 100) gui_cpu_usage = 100;
            if (gui_cpu_usage < 1) gui_cpu_usage = 1;
            
            work_cycles = 0;
            last_report_tsc = now_tsc;
        }
    }
}
