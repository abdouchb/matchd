/*
 * Zarqaa OS 0.1 - GPU Abstraction Layer (Graphics HAL)
 * Creator: Abdelhak Chabbi
 *
 * Universal GPU Hardware Abstraction Layer with Hardware 2D Acceleration,
 * Bochs BGA Zero-Copy Page Flipping, Intel GMA Blitting, and SIMD Software Fallback.
 */

#ifndef DRIVERS_GPU_H
#define DRIVERS_GPU_H

#include "zarqaa/types.h"

typedef enum {
    GPU_TYPE_NONE = 0,
    GPU_TYPE_BOCHS_BGA,
    GPU_TYPE_VIRTIO,
    GPU_TYPE_INTEL,
    GPU_TYPE_VMWARE_SVGA,
} gpu_type_t;

typedef struct {
    gpu_type_t type;
    uint16_t   vendor_id;
    uint16_t   device_id;
    uint32_t   mmio_base;
    uint32_t   fb_base;
    bool       accel_available;
    bool       page_flip_available;
    int        display_width;
    int        display_height;
} gpu_info_t;

/* ══════════════════════════════════════════════════════════
 *   Graphics HAL Driver Operations Table
 * ══════════════════════════════════════════════════════════ */
typedef struct gpu_hal_ops {
    const char *name;
    bool is_hardware_accel;
    bool has_page_flip;

    /* 2D Primitives */
    void (*fill_rect)(uint32_t *dst, int pitch_bytes, int x, int y, int w, int h, uint32_t color);
    void (*copy_rect)(uint32_t *dst, int dst_pitch_bytes, int dx, int dy,
                      const uint32_t *src, int src_pitch_bytes, int sx, int sy, int w, int h);
    void (*blend_rect)(uint32_t *dst, int dst_pitch_bytes, int dx, int dy,
                       const uint32_t *src, int src_pitch_bytes, int sx, int sy, int w, int h, uint8_t alpha);
    
    /* Buffer Presentation */
    void (*swap_buffers)(uint32_t *front, uint32_t *back, int pitch_bytes, int width, int height);
} gpu_hal_ops_t;

/* Initialize GPU detection and configure Graphics HAL */
void gpu_init(void);
void gpu_hal_init(void);

/* Get detected GPU info & active HAL ops */
const gpu_info_t *gpu_get_info(void);
const gpu_hal_ops_t *gpu_hal_get_ops(void);

/* Core Graphics HAL Functions (Called by Compositor, Renderer, and Window Manager) */
void gpu_hal_fill_rect(uint32_t *dst, int pitch_bytes, int x, int y, int w, int h, uint32_t color);
void gpu_hal_copy_rect(uint32_t *dst, int dst_pitch_bytes, int dx, int dy,
                       const uint32_t *src, int src_pitch_bytes, int sx, int sy, int w, int h);
void gpu_hal_blend_rect(uint32_t *dst, int dst_pitch_bytes, int dx, int dy,
                        const uint32_t *src, int src_pitch_bytes, int sx, int sy, int w, int h, uint8_t alpha);
void gpu_hal_set_front_format(int pitch_bytes, int bpp, int rs, int rsz, int gs, int gsz, int bs, int bsz);
void gpu_hal_swap_buffers(uint32_t *front, uint32_t *back, int pitch_bytes, int width, int height);

/* Legacy & Direct Page-Flipping API */
bool gpu_can_flip(void);
bool gpu_setup_double_buffer(int width, int height);
uint32_t *gpu_flip(uint32_t *vram_base, int pitch);
int gpu_get_back_page(void);
void gpu_fill_rect(uint32_t *fb, int pitch, int x, int y, int w, int h, uint32_t color);
void gpu_copy_rect(uint32_t *fb, int pitch_bytes, int src_x, int src_y, int dst_x, int dst_y, int w, int h);

#endif /* DRIVERS_GPU_H */
