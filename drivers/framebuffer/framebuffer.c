/*
 * Zarqaa OS 0.1 - Framebuffer Driver
 * Creator: Abdelhak Chabbi
 *
 * Integrated with GPU page flipping (Bochs BGA / Redox-style).
 * When BGA is available, fb_swap() uses a single register write
 * instead of copying millions of pixels.
 */

#include "drivers/framebuffer.h"
#include "drivers/font.h"
#include "drivers/gpu.h"
#include "drivers/gpu_renderer.h"
#include "drivers/font_atlas.h"
#include "graphics/graphics_api.h"
#include "graphics/renderer.h"
#include "kernel/heap.h"
#include "kernel/vmm.h"
#include "zarqaa/string.h"
#include "zarqaa/types.h"
#include "gui/dirty_rect.h"
#include "../gpu/intel_gma.h"
#include "drivers/pit.h"

extern bool g_intel_gma_active;

static struct limine_framebuffer *framebuffer;
static uint32_t *fb_ptr;         /* GPU VRAM base (always page 0) */
static uint32_t *backbuffer;     /* Where we draw (page 1 VRAM or heap) */
static int width;
static int height;
static int pitch;

static bool vsync_enabled = false; /* Uncapped FPS by default, togglable with F8 or Settings */

static uint32_t *current_rt_buffer;
static int current_rt_width;
static int current_rt_height;
static int current_rt_pitch;

#include "kernel/vmm.h"

void fb_init(struct limine_framebuffer *fb) {
    framebuffer = fb;
    fb_ptr = (uint32_t *)fb->address;
    width = fb->width;
    height = fb->height;
    /* Backbuffer is always tightly packed XRGB; the HAL converts to the real scanout format. */
    pitch = width * 4;
    int front_pitch = fb->pitch;
    if (fb->bpp != 32 || fb->red_mask_shift != 16 || fb->green_mask_shift != 8 || fb->blue_mask_shift != 0 ||
        front_pitch != pitch) {
        kprintf("FB: Non-default mode %dx%d bpp=%d pitch=%d (rgb shifts %d/%d/%d) - using converting blit.\n",
                width, height, (int)fb->bpp, front_pitch,
                fb->red_mask_shift, fb->green_mask_shift, fb->blue_mask_shift);
    }
    gpu_hal_set_front_format(front_pitch, fb->bpp, fb->red_mask_shift, fb->red_mask_size,
                             fb->green_mask_shift, fb->green_mask_size,
                             fb->blue_mask_shift, fb->blue_mask_size);

    /* 
     * Allocate unified backbuffer in System RAM for 100% artifact-free, tear-free rendering.
     * Hardware page-flipping with partial dirty rects causes double-buffer tearing / striping.
     */
    uint64_t fb_size = height * pitch;
    memset(fb_ptr, 0, (uint64_t)height * front_pitch); /* Clean physical VRAM immediately on boot to eliminate bootloader residue */
    backbuffer = (uint32_t *)kmalloc(fb_size);
    if (!backbuffer) {
        backbuffer = fb_ptr;
        pitch = front_pitch;
        kprintf("FB: [WARNING] Heap exhausted. Drawing directly to VRAM.\n");
    } else {
        memset(backbuffer, 0, fb_size);
        kprintf("FB: Unified RAM Backbuffer allocated successfully (Artifact-Free Mode).\n");
    }
    
    /* Initialize Hardware GPU Renderer */
    gpu_renderer_init();
    
    if (gpu_hal_get_ops()->is_hardware_accel) {
        kprintf("FB: Using Hardware GPU Backend.\n");
        renderer_set_backend(&g_hardware_renderer);
        
        gpu_surface_t *primary = gpu_surface_create(width, height);
        if (primary) {
            if (primary->backbuffer) kfree(primary->backbuffer);
            primary->backbuffer = backbuffer; // Alias to our managed backbuffer
            primary->pitch = pitch;
            gpu_renderer_set_primary_surface(primary);
            gpu_renderer_bind_surface(primary);
        }
    } else {
        renderer_set_backend(&g_software_renderer);
    }

    fb_reset_render_target();
}

void fb_set_render_target(uint32_t *buffer, int w, int h, int p) {
    current_rt_buffer = buffer;
    current_rt_width = w;
    current_rt_height = h;
    current_rt_pitch = p;
}

/* Optional redirect: while set, "reset" returns to this buffer instead of the screen */
static uint32_t *rt_base_buffer;
static int rt_base_w, rt_base_h, rt_base_p;

void fb_set_base_target(uint32_t *buffer, int w, int h, int p) {
    rt_base_buffer = buffer;
    rt_base_w = w; rt_base_h = h; rt_base_p = p;
    fb_reset_render_target();
}

void fb_reset_render_target(void) {
    if (rt_base_buffer) {
        current_rt_buffer = rt_base_buffer;
        current_rt_width = rt_base_w;
        current_rt_height = rt_base_h;
        current_rt_pitch = rt_base_p;
        return;
    }
    current_rt_buffer = backbuffer;
    current_rt_width = width;
    current_rt_height = height;
    current_rt_pitch = pitch;
}

/* Glyph accessor for the baked TTF atlas; bitmap rows are FONT_ATLAS_WIDTH bytes apart */
const uint8_t *fb_ttf_glyph(char c, int *w, int *h, int *xoff, int *yoff, int *adv) {
    if (c < 32 || c >= 127) c = '?';
    const baked_char_t *b = &font_cdata[c - 32];
    *w = b->x1 - b->x0; *h = b->y1 - b->y0;
    *xoff = b->xoff; *yoff = b->yoff; *adv = b->xadvance;
    return &font_bitmap[b->y0 * FONT_ATLAS_WIDTH + b->x0];
}

color_t fb_getpixel(int x, int y) {
    if (x < 0 || x >= current_rt_width || y < 0 || y >= current_rt_height) return 0;
    uint32_t *pixel = (uint32_t *)((uint8_t *)current_rt_buffer + y * current_rt_pitch + x * 4);
    return (*pixel) & 0x00FFFFFF;
}

void fb_putpixel(int x, int y, color_t color) {
    if (x < 0 || x >= current_rt_width || y < 0 || y >= current_rt_height) return;

    if (current_rt_buffer == backbuffer) {
        if (fb_clip_rect.width > 0) {
            if (x < fb_clip_rect.x || x >= fb_clip_rect.x + fb_clip_rect.width ||
                y < fb_clip_rect.y || y >= fb_clip_rect.y + fb_clip_rect.height) {
                return;
            }
        }
    }

    uint32_t *pixel = (uint32_t *)((uint8_t *)current_rt_buffer + y * current_rt_pitch + x * 4);
    *pixel = color | 0xFF000000;
}

static inline uint32_t fast_alpha_blend_u32(uint32_t fg, uint32_t bg, uint8_t alpha) {
    if (alpha >= 255) return fg | 0xFF000000;
    if (alpha == 0) return bg | 0xFF000000;
    uint32_t inv_alpha = 255 - alpha;
    uint32_t rb = (((fg & 0x00FF00FF) * alpha + (bg & 0x00FF00FF) * inv_alpha + 0x00800080) >> 8) & 0x00FF00FF;
    uint32_t g  = (((fg & 0x0000FF00) * alpha + (bg & 0x0000FF00) * inv_alpha + 0x00008000) >> 8) & 0x0000FF00;
    return 0xFF000000 | rb | g;
}

void fb_blend_pixel(int x, int y, color_t color, uint8_t alpha) {
    if (x < 0 || y < 0 || x >= current_rt_width || y >= current_rt_height || alpha == 0) return;

    uint8_t src_a = (color >> 24) & 0xFF;
    if (src_a == 0 && (color & 0x00FFFFFF) != 0) src_a = 255;
    uint8_t effective_a = (src_a == 255) ? alpha : (uint8_t)((src_a * alpha) / 255);
    if (effective_a == 0) return;

    uint32_t *dst = (uint32_t *)((uint8_t *)current_rt_buffer + y * current_rt_pitch) + x;

    /* Fast Path: Direct write when fully opaque */
    if (effective_a == 255) {
        *dst = color | 0xFF000000;
        return;
    }

    *dst = fast_alpha_blend_u32(color, *dst, effective_a);
}

void fb_blend_pixel_clipped(int x, int y, color_t color, uint8_t alpha, const rect_t *clip) {
    if (clip) {
        if (x < clip->x || y < clip->y || x >= clip->x + clip->width || y >= clip->y + clip->height)
            return;
    }
    fb_blend_pixel(x, y, color, alpha);
}

void fb_fill_rect_clipped(int x, int y, int w, int h, color_t color, const rect_t *clip) {
    int x1 = x, y1 = y, x2 = x + w, y2 = y + h;

    if (clip) {
        int cx2 = clip->x + clip->width;
        int cy2 = clip->y + clip->height;
        if (x1 < clip->x) x1 = clip->x;
        if (y1 < clip->y) y1 = clip->y;
        if (x2 > cx2)     x2 = cx2;
        if (y2 > cy2)     y2 = cy2;
    }

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > current_rt_width)  x2 = current_rt_width;
    if (y2 > current_rt_height) y2 = current_rt_height;

    if (x1 >= x2 || y1 >= y2) return;

    color |= 0xFF000000;
    int len = x2 - x1;

    for (int i = y1; i < y2; i++) {
        uint32_t *row = (uint32_t *)((uint8_t *)current_rt_buffer + i * current_rt_pitch) + x1;
        for (int j = 0; j < len; j++) {
            row[j] = color;
        }
    }
}

void fb_fill_rect_alpha_clipped(int x, int y, int w, int h, color_t color, uint8_t alpha, const rect_t *clip) {
    if (alpha == 0) return;
    if (alpha == 255) {
        fb_fill_rect_clipped(x, y, w, h, color, clip);
        return;
    }

    int x1 = x, y1 = y, x2 = x + w, y2 = y + h;

    if (clip) {
        int cx2 = clip->x + clip->width;
        int cy2 = clip->y + clip->height;
        if (x1 < clip->x) x1 = clip->x;
        if (y1 < clip->y) y1 = clip->y;
        if (x2 > cx2)     x2 = cx2;
        if (y2 > cy2)     y2 = cy2;
    }

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > current_rt_width)  x2 = current_rt_width;
    if (y2 > current_rt_height) y2 = current_rt_height;

    if (x1 >= x2 || y1 >= y2) return;

    int len = x2 - x1;
    for (int i = y1; i < y2; i++) {
        uint32_t *row = (uint32_t *)((uint8_t *)current_rt_buffer + i * current_rt_pitch) + x1;
        for (int j = 0; j < len; j++) {
            row[j] = fast_alpha_blend_u32(color, row[j], alpha);
        }
    }
}

void fb_fill_rect_alpha(int x, int y, int w, int h, color_t color, uint8_t alpha) {
    fb_fill_rect_alpha_clipped(x, y, w, h, color, alpha, (current_rt_buffer == backbuffer && fb_clip_rect.width > 0) ? &fb_clip_rect : NULL);
}

void fb_fill_rect_internal(int x, int y, int w, int h, color_t color) {
    int x1 = x, y1 = y, x2 = x + w, y2 = y + h;

    uint8_t alpha = (color >> 24) & 0xFF;
    if (alpha < 255 && alpha > 0) {
        fb_fill_rect_alpha(x, y, w, h, color, alpha);
        return;
    } else if (alpha == 0) {
        // If it's completely transparent and has an explicit alpha channel, do nothing
        // Wait, some old code might pass 0x00RRGGBB expecting it to be opaque.
        // We will only route to alpha if the alpha was explicitly set in the function that calls it.
        // Actually, we'll let alpha=0 fall through to be opaque just to avoid breaking old code that didn't set alpha.
    }

    if (current_rt_buffer == backbuffer) {
        int cx1 = fb_clip_rect.x, cy1 = fb_clip_rect.y;
        int cx2 = cx1 + fb_clip_rect.width, cy2 = cy1 + fb_clip_rect.height;
        if (x1 < cx1) x1 = cx1;
        if (y1 < cy1) y1 = cy1;
        if (x2 > cx2) x2 = cx2;
        if (y2 > cy2) y2 = cy2;
    } else {
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > current_rt_width) x2 = current_rt_width;
        if (y2 > current_rt_height) y2 = current_rt_height;
    }

    if (x1 >= x2 || y1 >= y2) return;

    color |= 0xFF000000;
    int fill_w = x2 - x1;

    for (int i = y1; i < y2; i++) {
        uint32_t *row = (uint32_t *)((uint8_t *)current_rt_buffer + i * current_rt_pitch) + x1;
        for (int j = 0; j < fill_w; j++) {
            row[j] = color;
        }
    }
}

void fb_fill_rect(int x, int y, int w, int h, color_t color) {
    gapi_fill_rect(NULL, x, y, w, h, color);
}

void fb_draw_rect_internal(int x, int y, int w, int h, color_t color) {
    fb_fill_rect_internal(x, y, w, 1, color);
    fb_fill_rect_internal(x, y + h - 1, w, 1, color);
    fb_fill_rect_internal(x, y, 1, h, color);
    fb_fill_rect_internal(x + w - 1, y, 1, h, color);
}

void fb_draw_rect(int x, int y, int w, int h, color_t color) {
    gapi_draw_rect(NULL, x, y, w, h, color);
}

void fb_draw_line_internal(int x0, int y0, int x1, int y1, color_t color) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        fb_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fb_draw_line(int x0, int y0, int x1, int y1, color_t color) {
    gapi_draw_line(NULL, x0, y0, x1, y1, color);
}

void fb_draw_char(int x, int y, char c, color_t fg, color_t bg) {
    const uint8_t *glyph = font_get_glyph(c);
    for (int cy = 0; cy < FONT_HEIGHT; cy++) {
        uint8_t row = glyph[cy];
        for (int cx = 0; cx < FONT_WIDTH; cx++) {
            if (row & (1 << (FONT_WIDTH - 1 - cx))) {
                fb_putpixel(x + cx, y + cy, fg);
            } else {
                fb_putpixel(x + cx, y + cy, bg);
            }
        }
    }
}

void fb_draw_string_internal(int x, int y, const char *s, color_t fg, color_t bg) {
    int start_x = x;
    while (*s) {
        if (*s == '\n') {
            y += FONT_HEIGHT;
            x = start_x;
        } else {
            fb_draw_char(x, y, *s, fg, bg);
            x += FONT_WIDTH;
        }
        s++;
    }
}

void fb_draw_string(int x, int y, const char *s, color_t fg, color_t bg) {
    gapi_draw_string(NULL, x, y, s, fg, bg);
}

void fb_draw_string_transparent_internal(int x, int y, const char *s, color_t fg) {
    int cx = x;
    int cy = y + FONT_SIZE; /* Approximate TTF baseline */

    while (*s) {
        if (*s == '\n') {
            cy += FONT_SIZE;
            cx = x;
        } else {
            char c = *s;
            if (c >= 32 && c < 127) {
                const baked_char_t *bchar = &font_cdata[c - 32];
                int w = bchar->x1 - bchar->x0;
                int h = bchar->y1 - bchar->y0;
                int px = cx + bchar->xoff;
                int py = cy + bchar->yoff;
                
                for (int dy = 0; dy < h; dy++) {
                    for (int dx = 0; dx < w; dx++) {
                        int bmp_x = bchar->x0 + dx;
                        int bmp_y = bchar->y0 + dy;
                        uint8_t alpha = font_bitmap[bmp_y * FONT_ATLAS_WIDTH + bmp_x];
                        
                        if (alpha > 0) {
                            if (alpha == 255) {
                                fb_putpixel(px + dx, py + dy, fg);
                            } else {
                                color_t bg_col = fb_getpixel(px + dx, py + dy);
                                fb_putpixel(px + dx, py + dy, fast_alpha_blend_u32(fg, bg_col, alpha));
                            }
                        }
                    }
                }
                cx += bchar->xadvance;
            }
        }
        s++;
    }
}

void fb_draw_string_transparent(int x, int y, const char *s, color_t fg) {
    gapi_draw_string_transparent(NULL, x, y, s, fg);
}

void fb_set_vsync(bool enable) {
    vsync_enabled = enable;
}

bool fb_get_vsync(void) {
    return vsync_enabled;
}

void fb_wait_vsync(void) {
    if (!vsync_enabled) return;

    /*
     * Hardware VGA / SVGA Vertical Retrace Synchronization (Port 0x3DA bit 3)
     * Bit 3 of Input Status #1 (0x3DA) is 1 during vertical retrace (VBlank)
     * and 0 during active display scanning.
     */
    uint32_t timeout = 50000;

    // 1. Wait until current vertical retrace ends (if in one)
    while ((inb(0x3DA) & 0x08) && --timeout) {
        __asm__ volatile("pause");
    }

    // 2. Wait until vertical retrace begins (start of VBlank)
    timeout = 50000;
    while (!(inb(0x3DA) & 0x08) && --timeout) {
        __asm__ volatile("pause");
    }
}

void fb_swap(void) {
    if (!fb_ptr || !backbuffer) return;
    
    int count = dirty_rect_get_count();
    if (count == 0) return;

    if (vsync_enabled) {
        fb_wait_vsync();
    }

    if (g_current_renderer && g_current_renderer->execute_queue) {
        g_current_renderer->execute_queue(&g_default_render_queue);
    }
    render_queue_clear(&g_default_render_queue);

    if (g_current_renderer && g_current_renderer->present) {
        g_current_renderer->present();
    } else {
        /* Direct dirty rect blit from unified RAM backbuffer to VRAM frontbuffer */
        gpu_hal_swap_buffers(fb_ptr, backbuffer, pitch, width, height);
    }
    dirty_rect_clear();
}

void fb_clear(color_t color) {
    fb_fill_rect(0, 0, width, height, color);
}

int fb_get_width(void) { return width; }
int fb_get_height(void) { return height; }
int fb_get_pitch(void) { return pitch; }
uint32_t *fb_get_backbuffer(void) { return backbuffer; }
