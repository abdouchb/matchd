/*
 * Zarqaa OS 0.1 - GPU 2D Hardware Acceleration API
 * Creator: Abdelhak Chabbi
 *
 * This layer abstracts hardware-accelerated 2D operations (like BitBLT)
 * so that the Compositor/Window Manager can use them without knowing 
 * the underlying GPU type.
 */

#ifndef DRIVERS_GPU_RENDERER_H
#define DRIVERS_GPU_RENDERER_H

#include "zarqaa/types.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    void* backbuffer;
    rect_t clip_rect;
} gpu_surface_t;

typedef struct {
    bool page_flip;
    bool blt;
    bool alpha_blend;
    bool cursor;
    bool textures;
    bool multiple_surfaces;
    bool hardware_clip;
    bool vsync;
} gpu_caps_t;

/* The vtable that each GPU driver provides to the renderer */
typedef struct {
    bool (*fill_rect)(gpu_surface_t *surface, int x, int y, int w, int h, uint32_t color);
    bool (*copy_rect)(gpu_surface_t *surface, int src_x, int src_y, int dst_x, int dst_y, int w, int h);
    bool (*draw_image)(gpu_surface_t *surface, int x, int y, int w, int h, uint32_t *pixels);
    bool (*draw_line)(gpu_surface_t *surface, int x1, int y1, int x2, int y2, uint32_t color);
    bool (*draw_cursor)(gpu_surface_t *surface, int x, int y);
    void (*present)(gpu_surface_t *surface);
} gpu_driver_ops_t;

/* Initialize the renderer system (called by GPU HAL) */
void gpu_renderer_init(void);

/* Register a GPU driver's operations */
void gpu_renderer_register(gpu_driver_ops_t *ops);

/* High-level Drawing API for the OS */
void gpu_renderer_fill_rect(int x, int y, int w, int h, uint32_t color);
void gpu_renderer_copy(int src_x, int src_y, int dst_x, int dst_y, int w, int h);
void gpu_renderer_draw_image(int x, int y, int w, int h, uint32_t *pixels);
void gpu_renderer_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void gpu_renderer_draw_cursor(int x, int y);
void gpu_renderer_present(void);
void gpu_renderer_putpixel(int x, int y, uint32_t color);

const gpu_caps_t* gpu_renderer_caps(void);
const gpu_surface_t* gpu_renderer_surface(void);
void gpu_renderer_bind_surface(gpu_surface_t* surface);

void gpu_renderer_set_clip(int x, int y, int w, int h);
void gpu_renderer_reset_clip(void);

gpu_surface_t* gpu_renderer_primary_surface(void);
void gpu_renderer_set_primary_surface(gpu_surface_t *surf);

gpu_surface_t* gpu_surface_create(uint32_t w, uint32_t h);
void gpu_surface_destroy(gpu_surface_t* surface);
void gpu_surface_lock(gpu_surface_t* surface);
void gpu_surface_unlock(gpu_surface_t* surface);

uint32_t gpu_renderer_getpixel(int x, int y);

#endif /* DRIVERS_GPU_RENDERER_H */
