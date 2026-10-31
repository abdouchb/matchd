/*
 * Zarqaa OS 0.1 - GPU 2D Hardware Acceleration API
 * Creator: Abdelhak Chabbi
 */

#include "drivers/gpu_renderer.h"
#include "drivers/gpu.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"

static gpu_driver_ops_t *current_ops = NULL;
static gpu_surface_t *bound_surface = NULL;
static gpu_surface_t *primary_surface = NULL;
static gpu_caps_t current_caps = {0};

void gpu_renderer_init(void) {
    kprintf("GPU Renderer: Initialized.\n");
    memset(&current_caps, 0, sizeof(current_caps));
}

void gpu_renderer_register(gpu_driver_ops_t *ops) {
    current_ops = ops;
    
    current_caps.page_flip = gpu_can_flip();
    current_caps.blt = (ops->fill_rect != NULL);
    current_caps.alpha_blend = false;
    current_caps.cursor = (ops->draw_cursor != NULL);
    current_caps.textures = false;
    current_caps.multiple_surfaces = true;
    current_caps.hardware_clip = false;
    current_caps.vsync = false;
}

const gpu_caps_t* gpu_renderer_caps(void) {
    return &current_caps;
}

const gpu_surface_t* gpu_renderer_surface(void) {
    return bound_surface;
}

gpu_surface_t* gpu_renderer_primary_surface(void) {
    return primary_surface;
}

void gpu_renderer_set_primary_surface(gpu_surface_t *surf) {
    primary_surface = surf;
}

void gpu_renderer_bind_surface(gpu_surface_t* surface) {
    bound_surface = surface;
}

void gpu_renderer_set_clip(int x, int y, int w, int h) {
    if (!bound_surface) return;
    bound_surface->clip_rect.x = x;
    bound_surface->clip_rect.y = y;
    bound_surface->clip_rect.width = w;
    bound_surface->clip_rect.height = h;
}

void gpu_renderer_reset_clip(void) {
    if (!bound_surface) return;
    bound_surface->clip_rect.x = 0;
    bound_surface->clip_rect.y = 0;
    bound_surface->clip_rect.width = bound_surface->width;
    bound_surface->clip_rect.height = bound_surface->height;
}

gpu_surface_t* gpu_surface_create(uint32_t w, uint32_t h) {
    gpu_surface_t* surf = (gpu_surface_t*)kmalloc(sizeof(gpu_surface_t));
    if (!surf) return NULL;
    memset(surf, 0, sizeof(gpu_surface_t));
    surf->width = w;
    surf->height = h;
    surf->bpp = 32;
    surf->pitch = w * 4;
    surf->backbuffer = kmalloc(h * surf->pitch);
    if (!surf->backbuffer) {
        kfree(surf);
        return NULL;
    }
    surf->clip_rect.x = 0;
    surf->clip_rect.y = 0;
    surf->clip_rect.width = w;
    surf->clip_rect.height = h;
    return surf;
}

void gpu_surface_destroy(gpu_surface_t* surface) {
    if (!surface) return;
    if (surface->backbuffer) kfree(surface->backbuffer);
    kfree(surface);
}

void gpu_surface_lock(gpu_surface_t* surface) {
    (void)surface;
}

void gpu_surface_unlock(gpu_surface_t* surface) {
    (void)surface;
}

void gpu_renderer_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!bound_surface || !current_ops || !current_ops->fill_rect) return;
    
    // Apply clipping
    if (bound_surface->clip_rect.width > 0) {
        int cx = bound_surface->clip_rect.x;
        int cy = bound_surface->clip_rect.y;
        int cw = bound_surface->clip_rect.width;
        int ch = bound_surface->clip_rect.height;
        
        int x1 = (x > cx) ? x : cx;
        int y1 = (y > cy) ? y : cy;
        int x2 = (x + w < cx + cw) ? x + w : cx + cw;
        int y2 = (y + h < cy + ch) ? y + h : cy + ch;
        
        if (x1 >= x2 || y1 >= y2) return; // Completely clipped outside
        
        x = x1;
        y = y1;
        w = x2 - x1;
        h = y2 - y1;
    }
    
    current_ops->fill_rect(bound_surface, x, y, w, h, color);
}

void gpu_renderer_copy(int src_x, int src_y, int dst_x, int dst_y, int w, int h) {
    if (!bound_surface || !current_ops || !current_ops->copy_rect) return;
    // We omit complex clipping for copy_rect for now, as it's rarely used outside window manager bounds
    current_ops->copy_rect(bound_surface, src_x, src_y, dst_x, dst_y, w, h);
}

void gpu_renderer_draw_image(int x, int y, int w, int h, uint32_t *pixels) {
    if (!bound_surface || !current_ops || !current_ops->draw_image) return;
    current_ops->draw_image(bound_surface, x, y, w, h, pixels);
}

void gpu_renderer_draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    if (!bound_surface || !current_ops || !current_ops->draw_line) return;
    current_ops->draw_line(bound_surface, x1, y1, x2, y2, color);
}

void gpu_renderer_draw_cursor(int x, int y) {
    if (!bound_surface || !current_ops || !current_ops->draw_cursor) return;
    current_ops->draw_cursor(bound_surface, x, y);
}

void gpu_renderer_present(void) {
    if (!bound_surface || !current_ops || !current_ops->present) return;
    current_ops->present(bound_surface);
}

void gpu_renderer_putpixel(int x, int y, uint32_t color) {
    if (!bound_surface) return;
    
    if (bound_surface->clip_rect.width > 0) {
        if (x < bound_surface->clip_rect.x || x >= bound_surface->clip_rect.x + (int)bound_surface->clip_rect.width ||
            y < bound_surface->clip_rect.y || y >= bound_surface->clip_rect.y + (int)bound_surface->clip_rect.height) {
            return;
        }
    }
    
    if (x < 0 || x >= (int)bound_surface->width || y < 0 || y >= (int)bound_surface->height) return;
    uint32_t *pixel = (uint32_t *)((uint8_t *)bound_surface->backbuffer + y * bound_surface->pitch + x * 4);
    *pixel = color | 0xFF000000;
}

uint32_t gpu_renderer_getpixel(int x, int y) {
    if (!bound_surface) return 0;
    if (x < 0 || x >= (int)bound_surface->width || y < 0 || y >= (int)bound_surface->height) return 0;
    uint32_t *pixel = (uint32_t *)((uint8_t *)bound_surface->backbuffer + y * bound_surface->pitch + x * 4);
    return (*pixel) & 0x00FFFFFF;
}
