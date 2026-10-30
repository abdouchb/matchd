#ifndef BROWSER_PRO_GRAPHICS_UTILS_H
#define BROWSER_PRO_GRAPHICS_UTILS_H

#include <stdint.h>

extern "C" {
    struct window;
}

void draw_rect(struct window* win, int rx, int ry, int rw, int rh, uint32_t color);
void draw_rounded_rect(struct window* win, int rx, int ry, int rw, int rh, int radius, uint32_t color);
void draw_text(struct window* win, int tx, int ty, const char* text, uint32_t color);

#endif // BROWSER_PRO_GRAPHICS_UTILS_H
