/*
 * Zarqaa OS 0.1 - Graphics API (libzargraphics)
 * Creator: Abdelhak Chabbi & AI
 */

#include "libzargraphics/zargraphics.h"
#include "drivers/framebuffer.h"
#include "kernel/heap.h"

void zar_graphics_init(void) {
}

zar_surface_t zar_create_surface(int width, int height) {
    return (zar_surface_t)kcalloc(width * height, 4);
}

void zar_destroy_surface(zar_surface_t surf) {
    if (surf) kfree(surf);
}

void zar_draw_rect(zar_surface_t surf, int x, int y, int w, int h, color_t color) {
    (void)surf; (void)x; (void)y; (void)w; (void)h; (void)color;
}

void zar_draw_line(zar_surface_t surf, int x0, int y0, int x1, int y1, color_t color) {
    (void)surf; (void)x0; (void)y0; (void)x1; (void)y1; (void)color;
}

void zar_draw_text(zar_surface_t surf, int x, int y, const char *text, color_t fg, color_t bg) {
    (void)surf; (void)x; (void)y; (void)text; (void)fg; (void)bg;
}

void zar_blit_surface(zar_surface_t dst, zar_surface_t src, int dx, int dy, int sx, int sy, int w, int h, bool blend, uint8_t alpha) {
    (void)dst; (void)src; (void)dx; (void)dy; (void)sx; (void)sy; (void)w; (void)h; (void)blend; (void)alpha;
}

void zar_present(zar_surface_t surf) {
    (void)surf;
}
