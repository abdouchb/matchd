/*
 * Zarqaa OS 0.1 - GUI Renderer Header
 * Creator: Abdelhak Chabbi & AI
 */

#ifndef GUI_RENDERER_H
#define GUI_RENDERER_H

#include "gui/surface.h"
#include "drivers/framebuffer.h"
#include "drivers/font.h"
#include "zarqaa/string.h"

static inline void renderer_execute_fill_rect(surface_t *surf, int x, int y, int w, int h, color_t color) {
    if (!surf) return;
    uint32_t *pixels = surf->pixels ? surf->pixels : (uint32_t *)surf->cpu_pointer;
    if (!pixels) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > surf->width) w = surf->width - x;
    if (y + h > surf->height) h = surf->height - y;
    if (w <= 0 || h <= 0) return;

    for (int i = y; i < y + h; i++) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + i * surf->pitch) + x;
        for (int j = 0; j < w; j++) {
            row[j] = color;
        }
    }
}

static inline void renderer_execute_draw_line(surface_t *surf, int x0, int y0, int x1, int y1, color_t color) {
    if (!surf) return;
    uint32_t *pixels = surf->pixels ? surf->pixels : (uint32_t *)surf->cpu_pointer;
    if (!pixels) return;
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        if (x0 >= 0 && x0 < surf->width && y0 >= 0 && y0 < surf->height) {
            uint32_t *pixel = (uint32_t *)((uint8_t *)pixels + y0 * surf->pitch) + x0;
            *pixel = color;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static inline void renderer_execute_draw_text(surface_t *surf, int x, int y, const char *text, color_t fg, color_t bg, bool shadow) {
    (void)surf; (void)bg; (void)shadow;
    if (!text) return;
    fb_draw_string_transparent(x, y, text, fg);
}

static inline void renderer_execute_draw_text_wrapped(surface_t *surf, int x, int y, int max_w, const char *text, color_t fg, color_t bg, bool shadow) {
    (void)surf; (void)max_w; (void)bg; (void)shadow;
    if (!text) return;
    fb_draw_string_transparent(x, y, text, fg);
}

static inline void renderer_execute_blit(surface_t *dst, surface_t *src, int dx, int dy, int sx, int sy, int w, int h, bool blend, uint8_t alpha) {
    (void)dst; (void)src; (void)dx; (void)dy; (void)sx; (void)sy; (void)w; (void)h; (void)blend; (void)alpha;
}

static inline int renderer_measure_text(const char *text) {
    if (!text) return 0;
    return strlen(text) * FONT_WIDTH;
}

#endif /* GUI_RENDERER_H */
