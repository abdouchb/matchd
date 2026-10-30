#ifndef INTEL_GMA_H
#define INTEL_GMA_H

#include "zarqaa/types.h"
#include "kernel/pci.h"

/* Common Intel Graphics Registers (Gen5 - Gen8) */

/* GTT Base relative to MMIO */
#define INTEL_GTT_OFFSET        0x200000

/* Display Registers */
#define DSPACNTR                0x70180 /* Display Plane A Control */
#define DSPALINOFF              0x70184 /* Display Plane A Linear Offset */
#define DSPASTRIDE              0x70188 /* Display Plane A Stride */
#define DSPASURF                0x7019C /* Display Plane A Surface Address */

#define DSPBCNTR                0x71180 /* Display Plane B Control */
#define DSPBLINOFF              0x71184 /* Display Plane B Linear Offset */
#define DSPBSTRIDE              0x71188 /* Display Plane B Stride */
#define DSPBSURF                0x7119C /* Display Plane B Surface Address */

/* Blitter Engine Registers (Gen6+) */
#define BLT_RING_TAIL           0x22030
#define BLT_RING_HEAD           0x22034
#define BLT_RING_START          0x22038
#define BLT_RING_CTL            0x2203C

/* Ring Control Flags */
#define RING_VALID              1

/* 2D Command Opcodes (Bit 21:20 = 11b for 32-bit 8888 Color Format) */
#define MI_NOOP                 0x00000000
#define MI_FLUSH                0x04000000
#define XY_COLOR_BLT_CMD        ((2 << 29) | (0x50 << 22) | (3 << 20) | 0x04)
#define XY_SRC_COPY_BLT_CMD     ((2 << 29) | (0x53 << 22) | (3 << 20) | 0x06)

/* Ring buffer structure */
typedef struct {
    volatile uint32_t *virt_addr;
    uint32_t phys_addr;
    uint32_t size_bytes;
    uint32_t size_pages;
    uint32_t tail;
} intel_ring_t;

/* Memory management structure for the driver */
typedef struct {
    uint64_t mmio_base_phys;
    uint8_t *mmio_base_virt;
    uint64_t fb_base_phys;   /* Aperture base */
    uint32_t *gtt_base_virt;
    int gen;                 /* Generation: 6 (Sandy), 7 (Ivy/Haswell), 8+ (Broadwell/Skylake) */
    
    int width;
    int height;
    int pitch;
    
    /* Double buffering state */
    bool double_buffer_ready;
    uint32_t backbuffer_phys; /* Physical addr of our allocated backbuffer */
    uint32_t backbuffer_gtt_offset; /* Offset inside the GTT aperture */
    uint32_t frontbuffer_gtt_offset; /* Offset of Limine's original fb */
    uint32_t orig_dspasurf;
    uint32_t orig_dspbsurf;
    uint32_t orig_dspcsurf;
    int current_page; /* 0 = front, 1 = back */
    
    /* Blitter Ring */
    intel_ring_t blt_ring;
} intel_gma_state_t;

/* Initialize Intel GMA hardware */
bool intel_gma_init(const pci_device_t *dev, int width, int height, int pitch);

/* Setup double buffer and page flipping */
bool intel_gma_setup_double_buffer(void);

/* Flip the display. Returns pointer to the new backbuffer. */
uint32_t *intel_gma_flip(uint32_t *vram_base);

/* 2D Hardware Acceleration */
bool intel_gma_fill_rect(int x, int y, int w, int h, uint32_t color);
bool intel_gma_copy_rect(int src_x, int src_y, int dst_x, int dst_y, int w, int h);

#endif /* INTEL_GMA_H */
