#include "intel_gma.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/pci.h"
#include "zarqaa/string.h"

static intel_gma_state_t gma_state = {0};

/* Read a 32-bit MMIO register */
static inline uint32_t intel_read32(uint32_t reg) {
    if (!gma_state.mmio_base_virt) return 0;
    volatile uint32_t *ptr = (volatile uint32_t *)(gma_state.mmio_base_virt + reg);
    return *ptr;
}

/* Write a 32-bit MMIO register */
static inline void intel_write32(uint32_t reg, uint32_t val) {
    if (!gma_state.mmio_base_virt) return;
    volatile uint32_t *ptr = (volatile uint32_t *)(gma_state.mmio_base_virt + reg);
    *ptr = val;
}

/* Write a single GTT PTE entry based on GPU generation */
static void intel_gma_write_gtt_pte(uint32_t page_index, uint64_t phys_addr) {
    if (!gma_state.gtt_base_virt) return;
    
    if (gma_state.gen >= 8) {
        /* Gen8+ (Broadwell, Skylake, Kaby Lake, etc.): 64-bit PTE (8 bytes per entry) */
        volatile uint64_t *gtt64 = (volatile uint64_t *)gma_state.gtt_base_virt;
        gtt64[page_index] = (phys_addr & ~0xFFFULL) | 0x01; // Bit 0 = Valid
    } else {
        /* Gen6/Gen7 (Sandy Bridge, Ivy Bridge, Haswell): 32-bit PTE (4 bytes per entry) */
        volatile uint32_t *gtt32 = (volatile uint32_t *)gma_state.gtt_base_virt;
        gtt32[page_index] = (uint32_t)(phys_addr & 0xFFFFF000) | 0x01; // Bit 0 = Valid
    }
}

bool intel_gma_init(const pci_device_t *dev, int width, int height, int pitch) {
    if (!dev || dev->vendor_id != 0x8086) return false;
    
    kprintf("Intel GMA: Initializing for device 0x%x...\n", dev->device_id);
    memset(&gma_state, 0, sizeof(gma_state));
    
    /* 1. Enable PCI Bus Master and Memory Space Decoding */
    uint16_t pci_cmd = pci_read_config_word(dev->bus, dev->device, dev->function, 0x04);
    pci_cmd |= (1 << 1) | (1 << 2); // Memory Space (0x02) | Bus Master (0x04)
    pci_write_config_word(dev->bus, dev->device, dev->function, 0x04, pci_cmd);
    
    /* 2. Detect Intel GPU Generation. Only known Gen6-9 parts are driven; anything else
     * (pre-Gen6, Gen11+) has a different GTT/ring layout and must stay on the GOP framebuffer. */
    uint16_t id = dev->device_id;
    int gen = 0;
    switch (id & 0xFF00) {
        case 0x0100: gen = ((id & 0x00F0) == 0x0050 || (id & 0x00F0) == 0x0060) ? 7 : 6; break;
        case 0x0400: case 0x0A00: case 0x0D00: gen = 7; break;
        case 0x1600: gen = 8; break;
        case 0x1900: case 0x5900: case 0x3E00: case 0x9B00: gen = 9; break;
        default: break;
    }
    if (!gen) {
        kprintf("Intel GMA: Device 0x%x not supported, using framebuffer fallback.\n", id);
        return false;
    }
    gma_state.gen = gen;
    kprintf("Intel GMA: Detected Gen%d graphics architecture.\n", gen);

    /* 3. Extract MMIO & VRAM Aperture BARs */
    if ((dev->bar[0] & 0x6) == 0x4) {
        gma_state.mmio_base_phys = (uint64_t)(dev->bar[0] & 0xFFFFFFF0) | ((uint64_t)dev->bar[1] << 32);
        if ((dev->bar[2] & 0x6) == 0x4) {
            gma_state.fb_base_phys = (uint64_t)(dev->bar[2] & 0xFFFFFFF0) | ((uint64_t)dev->bar[3] << 32);
        } else {
            gma_state.fb_base_phys = dev->bar[2] & 0xFFFFFFF0;
        }
    } else {
        gma_state.mmio_base_phys = dev->bar[0] & 0xFFFFFFF0;
        gma_state.fb_base_phys   = dev->bar[2] & 0xFFFFFFF0;
    }

    if (!gma_state.mmio_base_phys) {
        kprintf("Intel GMA: Error - BAR0 MMIO address is 0!\n");
        return false;
    }

    gma_state.width = width;
    gma_state.height = height;
    gma_state.pitch = pitch;
    
    extern uint64_t hhdm_offset;
    extern uint64_t *kernel_pml4;
    extern void vmm_map_mmio(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
    
    /* 4. Map 4MB of MMIO space (registers + GTT); Gen6/7 BAR0 is only 4MB */
    for (uint64_t offset = 0; offset < 4 * 1024 * 1024; offset += PAGE_SIZE) {
        vmm_map_mmio(kernel_pml4, gma_state.mmio_base_phys + hhdm_offset + offset, gma_state.mmio_base_phys + offset, 0x1B);
    }
    
    gma_state.mmio_base_virt = (uint8_t *)(gma_state.mmio_base_phys + hhdm_offset);
    
    /* GTT is located at offset 2MB (0x200000) inside MMIO on Gen 6+ */
    gma_state.gtt_base_virt = (uint32_t *)(gma_state.mmio_base_virt + INTEL_GTT_OFFSET);
    
    /* 5. Save original DSPxSURF values and determine active frontbuffer offset */
    gma_state.orig_dspasurf = intel_read32(DSPASURF);
    gma_state.orig_dspbsurf = intel_read32(DSPBSURF);
    gma_state.orig_dspcsurf = intel_read32(0x7219C);
    
    gma_state.frontbuffer_gtt_offset = 0;
    if (intel_read32(DSPACNTR) & (1 << 31)) {
        gma_state.frontbuffer_gtt_offset = gma_state.orig_dspasurf;
    } else if (intel_read32(DSPBCNTR) & (1 << 31)) {
        gma_state.frontbuffer_gtt_offset = gma_state.orig_dspbsurf;
    } else if (intel_read32(0x72180) & (1 << 31)) {
        gma_state.frontbuffer_gtt_offset = gma_state.orig_dspcsurf;
    }
    kprintf("Intel GMA: Frontbuffer GTT offset: 0x%x\n", gma_state.frontbuffer_gtt_offset);
    if (!(intel_read32(DSPACNTR) & (1u << 31)) && !(intel_read32(DSPBCNTR) & (1u << 31)) &&
        !(intel_read32(0x72180) & (1u << 31))) {
        kprintf("Intel GMA: No active display plane, using framebuffer fallback.\n");
        gma_state.mmio_base_virt = NULL;
        return false;
    }
    
    /* 6. Initialize Blitter Ring Buffer */
    intel_ring_t *ring = &gma_state.blt_ring;
    ring->size_pages = 4; // 16 KB ring buffer
    ring->size_bytes = ring->size_pages * PAGE_SIZE;
    
    ring->phys_addr = (uint32_t)pmm_alloc_blocks_low32(ring->size_pages);
    if (!ring->phys_addr) {
        kprintf("Intel GMA: Failed to allocate physical memory for Blitter Ring\n");
        return false;
    }
    
    ring->virt_addr = (volatile uint32_t *)((uint64_t)ring->phys_addr + hhdm_offset);
    memset((void *)ring->virt_addr, 0, ring->size_bytes);
    
    // Map into GTT at 32MB offset to avoid any VRAM frontbuffer collisions
    uint32_t ring_gtt_offset = 32 * 1024 * 1024;
    uint32_t gtt_start_index = ring_gtt_offset / PAGE_SIZE;
    
    uint32_t phys = ring->phys_addr;
    for (uint32_t i = 0; i < ring->size_pages; i++) {
        intel_gma_write_gtt_pte(gtt_start_index + i, phys);
        phys += PAGE_SIZE;
    }
    
    intel_write32(BLT_RING_TAIL, 0);
    intel_write32(BLT_RING_HEAD, 0);
    intel_write32(BLT_RING_START, ring_gtt_offset);
    
    uint32_t ctl = ((ring->size_pages - 1) << 12) | RING_VALID;
    intel_write32(BLT_RING_CTL, ctl);
    
    ring->tail = 0;
    kprintf("Intel GMA: BLT Ring Buffer initialized (16KB) at GTT offset 0x%x\n", ring_gtt_offset);
    
    return true;
}

bool intel_gma_setup_double_buffer(void) {
    if (!gma_state.mmio_base_virt) return false;
    
    /* 1. Calculate pages for backbuffer */
    uint32_t size_bytes = gma_state.pitch * gma_state.height;
    uint32_t num_pages = (size_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    
    /* 2. Allocate Physical Memory below 4GB */
    gma_state.backbuffer_phys = (uint32_t)pmm_alloc_blocks_low32(num_pages);
    if (!gma_state.backbuffer_phys) {
        kprintf("Intel GMA: Failed to allocate %d pages for backbuffer\n", num_pages);
        return false;
    }
    
    /* 3. Place backbuffer at 16MB offset in GTT */
    gma_state.backbuffer_gtt_offset = 16 * 1024 * 1024; 
    uint32_t gtt_start_index = gma_state.backbuffer_gtt_offset / PAGE_SIZE;
    
    /* 4. Map physical pages into GTT */
    uint32_t phys_addr = gma_state.backbuffer_phys;
    for (uint32_t i = 0; i < num_pages; i++) {
        intel_gma_write_gtt_pte(gtt_start_index + i, phys_addr);
        phys_addr += PAGE_SIZE;
    }
    
    gma_state.current_page = 0;
    gma_state.double_buffer_ready = true;
    
    kprintf("Intel GMA: Double Buffer ready at GTT offset 0x%x\n", gma_state.backbuffer_gtt_offset);
    return true;
}

uint32_t *intel_gma_flip(uint32_t *vram_base) {
    if (!gma_state.double_buffer_ready) return NULL;
    
    /* Switch page */
    gma_state.current_page = (gma_state.current_page == 0) ? 1 : 0;
    
    /* Write new surface address to Plane registers */
    uint32_t new_surf = (gma_state.current_page == 1) ? gma_state.backbuffer_gtt_offset : gma_state.frontbuffer_gtt_offset;
    intel_write32(DSPASURF, new_surf);
    intel_write32(DSPBSURF, new_surf);
    intel_write32(0x7219C, new_surf); /* Plane C */
    
    extern uint64_t hhdm_offset;
    if (gma_state.current_page == 0) {
        return (uint32_t *)((uint64_t)gma_state.backbuffer_phys + hhdm_offset);
    } else {
        return vram_base;
    }
}

/* --- 2D Hardware Acceleration (BLT Engine) --- */

static bool intel_gma_ring_wait(int n) {
    intel_ring_t *ring = &gma_state.blt_ring;
    if (!ring->virt_addr) return false;

    int timeout = 200000;
    while (timeout > 0) {
        uint32_t head = intel_read32(BLT_RING_HEAD) & 0x1FFFFC;
        int space = head - ring->tail;
        if (space <= 0) space += ring->size_bytes;
        if (space > n * 4 + 16) return true;
        
        timeout--;
        for (volatile int i = 0; i < 50; i++) __asm__ volatile("pause");
    }
    
    static bool hung = false;
    if (!hung) {
        kprintf("[GMA] Warning: BLT Ring buffer wait timeout. Disabling HW Accel.\n");
        hung = true;
    }
    gma_state.mmio_base_virt = NULL; // Disable GMA operations, gracefully fallback to SIMD software rasterizer
    return false;
}

static void intel_gma_ring_write(uint32_t data) {
    intel_ring_t *ring = &gma_state.blt_ring;
    ring->virt_addr[ring->tail / 4] = data;
    ring->tail += 4;
    if (ring->tail >= ring->size_bytes) {
        ring->tail = 0;
    }
}

static void intel_gma_ring_submit(void) {
    intel_ring_t *ring = &gma_state.blt_ring;
    
    // QWord align ring tail
    if ((ring->tail / 4) % 2 != 0) {
        intel_gma_ring_write(MI_NOOP);
    }
    
    /* Ensure CPU stores are visible before informing GPU */
    __asm__ volatile("sfence" ::: "memory");
    for (uint32_t i = 0; i < ring->size_bytes; i += 64) {
        __asm__ volatile("clflush (%0)" :: "r"((uint8_t*)ring->virt_addr + i) : "memory");
    }
    __asm__ volatile("sfence" ::: "memory");
    
    intel_write32(BLT_RING_TAIL, ring->tail);
}

bool intel_gma_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!gma_state.mmio_base_virt) return false;
    if (w <= 0 || h <= 0) return true;
    
    if (!intel_gma_ring_wait(6)) return false;
    intel_gma_ring_write(XY_COLOR_BLT_CMD);
    
    // ROP = 0xF0 (Solid Color), 32bpp, Pitch
    uint32_t dw1 = (3 << 24) | (0xF0 << 16) | (uint32_t)gma_state.pitch;
    intel_gma_ring_write(dw1);
    
    intel_gma_ring_write(((uint32_t)y << 16) | (uint32_t)x);
    intel_gma_ring_write(((uint32_t)(y + h) << 16) | (uint32_t)(x + w));
    
    uint32_t dest_gtt_offset = gma_state.frontbuffer_gtt_offset;
    if (gma_state.double_buffer_ready && gma_state.current_page == 0) {
        dest_gtt_offset = gma_state.backbuffer_gtt_offset;
    }
    
    intel_gma_ring_write(dest_gtt_offset);
    intel_gma_ring_write(color);
    
    intel_gma_ring_submit();
    return true;
}

bool intel_gma_copy_rect(int src_x, int src_y, int dst_x, int dst_y, int w, int h) {
    if (!gma_state.mmio_base_virt) return false;
    if (w <= 0 || h <= 0) return true;
    
    if (!intel_gma_ring_wait(8)) return false;
    intel_gma_ring_write(XY_SRC_COPY_BLT_CMD);
    
    // ROP = 0xCC (Src Copy), 32bpp, Dest Pitch
    uint32_t dw1 = (3 << 24) | (0xCC << 16) | (uint32_t)gma_state.pitch;
    intel_gma_ring_write(dw1);
    
    intel_gma_ring_write(((uint32_t)dst_y << 16) | (uint32_t)dst_x);
    intel_gma_ring_write(((uint32_t)(dst_y + h) << 16) | (uint32_t)(dst_x + w));
    
    uint32_t gtt_offset = gma_state.frontbuffer_gtt_offset;
    if (gma_state.double_buffer_ready && gma_state.current_page == 0) {
        gtt_offset = gma_state.backbuffer_gtt_offset;
    }
    
    intel_gma_ring_write(gtt_offset);
    intel_gma_ring_write(((uint32_t)src_y << 16) | (uint32_t)src_x);
    intel_gma_ring_write((uint32_t)gma_state.pitch);
    intel_gma_ring_write(gtt_offset);
    
    intel_gma_ring_submit();
    return true;
}

#include "kernel/driver.h"
#include "drivers/framebuffer.h"

bool g_intel_gma_active = false;

static bool intel_gma_pci_init(const pci_device_t *dev) {
    if (!dev || dev->vendor_id != 0x8086) return false;
    int w = fb_get_width();
    int h = fb_get_height();
    int p = fb_get_pitch();
    
    if (intel_gma_init(dev, w, h, p)) {
        kprintf("Intel GMA: Hardware detected and initialized successfully.\n");
        g_intel_gma_active = true;
        return true;
    }
    return false;
}

static pci_driver_t intel_gma_driver = {
    .name = "Intel GMA (Hardware Acceleration)",
    .match_type = DRIVER_MATCH_CLASS,
    .vendor_id = 0x8086,
    .class_code = 0x03,
    .subclass = 0x00,
    .init = intel_gma_pci_init
};

void intel_gma_register(void) {
    driver_register(&intel_gma_driver);
}

