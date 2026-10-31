#include "canvas.h"
#include "kernel/heap.h"
#include "gui/renderer.h"

canvas_t *canvas_create(surface_t *surf) {
    canvas_t *canvas = kcalloc(1, sizeof(canvas_t));
    canvas->target_surface = surf;
    return canvas;
}

void canvas_destroy(canvas_t *canvas) {
    kfree(canvas);
}

void canvas_begin(canvas_t *canvas) {
    // Lock or prepare surface if necessary
    canvas->offset_x = 0;
    canvas->offset_y = 0;
    canvas->has_clip = false;
}

void canvas_end(canvas_t *canvas) {
    // Unlock or flush surface
}

void canvas_set_clip(canvas_t *canvas, int x, int y, int w, int h) {
    canvas->clip_rect.x = x;
    canvas->clip_rect.y = y;
    canvas->clip_rect.w = w;
    canvas->clip_rect.h = h;
    canvas->has_clip = true;
}

void canvas_clear_clip(canvas_t *canvas) {
    canvas->has_clip = false;
}

void canvas_set_offset(canvas_t *canvas, int x, int y) {
    canvas->offset_x = x;
    canvas->offset_y = y;
}

// Helpers for coordinate mapping
static int to_screen_x(canvas_t *canvas, int x) { return x + canvas->offset_x; }
static int to_screen_y(canvas_t *canvas, int y) { return y + canvas->offset_y; }

void canvas_fill_rect(canvas_t *canvas, int x, int y, int w, int h, color_t color) {
    if (!canvas || !canvas->target_surface) return;
    int sx = to_screen_x(canvas, x);
    int sy = to_screen_y(canvas, y);
    renderer_execute_fill_rect(canvas->target_surface, sx, sy, w, h, color);
}

void canvas_draw_rect(canvas_t *canvas, int x, int y, int w, int h, color_t color) {
    if (!canvas || !canvas->target_surface) return;
    int sx = to_screen_x(canvas, x);
    int sy = to_screen_y(canvas, y);
    renderer_execute_draw_line(canvas->target_surface, sx, sy, sx + w, sy, color);
    renderer_execute_draw_line(canvas->target_surface, sx, sy + h, sx + w, sy + h, color);
    renderer_execute_draw_line(canvas->target_surface, sx, sy, sx, sy + h, color);
    renderer_execute_draw_line(canvas->target_surface, sx + w, sy, sx + w, sy + h, color);
}

void canvas_fill_rounded_rect(canvas_t *canvas, int x, int y, int w, int h, int radius, color_t color) {
    if (!canvas || !canvas->target_surface) return;
    int sx = to_screen_x(canvas, x);
    int sy = to_screen_y(canvas, y);
    // Draw simple rect for now since we don't have renderer_execute_fill_rounded_rect
    renderer_execute_fill_rect(canvas->target_surface, sx, sy, w, h, color);
}

void canvas_draw_line(canvas_t *canvas, int x1, int y1, int x2, int y2, color_t color) {
    if (!canvas || !canvas->target_surface) return;
    renderer_execute_draw_line(canvas->target_surface, to_screen_x(canvas, x1), to_screen_y(canvas, y1), 
                 to_screen_x(canvas, x2), to_screen_y(canvas, y2), color);
}

void canvas_draw_text(canvas_t *canvas, int x, int y, const char *text, color_t color) {
    if (!canvas || !canvas->target_surface) return;
    renderer_execute_draw_text(canvas->target_surface, to_screen_x(canvas, x), to_screen_y(canvas, y), text, color, 0, true);
}

void canvas_draw_image(canvas_t *canvas, int x, int y, void *image_data, int w, int h) {
    if (!canvas || !canvas->target_surface || !image_data) return;
    surface_t src;
    memset(&src, 0, sizeof(src));
    src.width = w;
    src.height = h;
    src.pitch = w * 4;
    src.format = PIXEL_FORMAT_ARGB32;
    src.cpu_pointer = image_data;
    src.pixels = (uint32_t *)image_data;
    
    int sx = to_screen_x(canvas, x);
    int sy = to_screen_y(canvas, y);
    renderer_execute_blit(canvas->target_surface, &src, sx, sy, 0, 0, w, h, true, 255);
}

void canvas_draw_image_rounded(canvas_t *canvas, int x, int y, void *image_data, int w, int h, int radius) {
    if (!canvas || !canvas->target_surface || !image_data) return;
    if (radius <= 0) {
        canvas_draw_image(canvas, x, y, image_data, w, h);
        return;
    }
    
    uint32_t *src_pixels = (uint32_t *)image_data;
    uint32_t *dst_pixels = (uint32_t *)canvas->target_surface->cpu_pointer;
    int dst_pitch = canvas->target_surface->pitch / 4;
    int dst_w = canvas->target_surface->width;
    int dst_h = canvas->target_surface->height;

    int sx = to_screen_x(canvas, x);
    int sy = to_screen_y(canvas, y);

    for (int iy = 0; iy < h; iy++) {
        for (int ix = 0; ix < w; ix++) {
            int dx = sx + ix;
            int dy = sy + iy;
            if (dx < 0 || dx >= dst_w || dy < 0 || dy >= dst_h) continue;
            
            int cx = -1, cy = -1;
            if (ix < radius && iy < radius) { cx = radius; cy = radius; }
            else if (ix >= w - radius && iy < radius) { cx = w - radius - 1; cy = radius; }
            else if (ix < radius && iy >= h - radius) { cx = radius; cy = h - radius - 1; }
            else if (ix >= w - radius && iy >= h - radius) { cx = w - radius - 1; cy = h - radius - 1; }
            
            if (cx != -1) {
                int dist_sq = (ix - cx) * (ix - cx) + (iy - cy) * (iy - cy);
                if (dist_sq > radius * radius) continue;
            }

            uint32_t src_col = src_pixels[iy * w + ix];
            uint8_t a = (src_col >> 24) & 0xFF;
            if (a == 0) continue;
            
            if (a == 255) {
                dst_pixels[dy * dst_pitch + dx] = src_col;
            } else {
                uint32_t bg = dst_pixels[dy * dst_pitch + dx];
                uint8_t inv_a = 255 - a;
                uint8_t r = (((src_col >> 16) & 0xFF) * a + ((bg >> 16) & 0xFF) * inv_a) >> 8;
                uint8_t g = (((src_col >> 8) & 0xFF) * a + ((bg >> 8) & 0xFF) * inv_a) >> 8;
                uint8_t b = ((src_col & 0xFF) * a + (bg & 0xFF) * inv_a) >> 8;
                dst_pixels[dy * dst_pitch + dx] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        }
    }
}

int canvas_measure_text(const char *text) {
    return renderer_measure_text(text);
}
