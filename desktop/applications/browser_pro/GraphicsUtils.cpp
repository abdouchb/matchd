#include "GraphicsUtils.h"

extern "C" {
    #include "desktop/window_manager.h"
    #include "gui/graphics.h"
    #include "gui/renderer.h"
}

void draw_rect(struct window* win, int rx, int ry, int rw, int rh, uint32_t color) {
    if (!win || !win->content_buffer) return;
    int y_start = ry < 0 ? 0 : ry;
    int y_end = (ry + rh) > win->height ? win->height : (ry + rh);
    int x_start = rx < 0 ? 0 : rx;
    int x_end = (rx + rw) > win->width ? win->width : (rx + rw);
    
    for (int y = y_start; y < y_end; y++) {
        for (int x = x_start; x < x_end; x++) {
            win->content_buffer[y * win->width + x] = color;
        }
    }
}

void draw_rounded_rect(struct window* win, int rx, int ry, int rw, int rh, int radius, uint32_t color) {
    if (!win || !win->content_buffer) return;
    int y_start = ry < 0 ? 0 : ry;
    int y_end = (ry + rh) > win->height ? win->height : (ry + rh);
    int x_start = rx < 0 ? 0 : rx;
    int x_end = (rx + rw) > win->width ? win->width : (rx + rw);
    
    for (int y = y_start; y < y_end; y++) {
        for (int x = x_start; x < x_end; x++) {
            // Check corners
            bool draw = true;
            if (x < rx + radius && y < ry + radius) {
                int dx = (rx + radius) - x;
                int dy = (ry + radius) - y;
                if (dx * dx + dy * dy > radius * radius) draw = false;
            } else if (x >= rx + rw - radius && y < ry + radius) {
                int dx = x - (rx + rw - radius - 1);
                int dy = (ry + radius) - y;
                if (dx * dx + dy * dy > radius * radius) draw = false;
            } else if (x < rx + radius && y >= ry + rh - radius) {
                int dx = (rx + radius) - x;
                int dy = y - (ry + rh - radius - 1);
                if (dx * dx + dy * dy > radius * radius) draw = false;
            } else if (x >= rx + rw - radius && y >= ry + rh - radius) {
                int dx = x - (rx + rw - radius - 1);
                int dy = y - (ry + rh - radius - 1);
                if (dx * dx + dy * dy > radius * radius) draw = false;
            }
            if (draw) {
                win->content_buffer[y * win->width + x] = color;
            }
        }
    }
}

void draw_text(struct window* win, int tx, int ty, const char* text, uint32_t color) {
    if (!win || !win->content_buffer || !text || text[0] == '\0') return;
    
    surface_t surf;
    surf.width = win->width;
    surf.height = win->height;
    surf.pitch = win->width * 4;
    surf.format = PIXEL_FORMAT_ARGB32;
    surf.cpu_pointer = win->content_buffer;
    
    renderer_execute_draw_text(&surf, tx, ty, text, color, 0, true);
}
