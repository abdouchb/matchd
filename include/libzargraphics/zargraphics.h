/*
 * Zarqaa OS 0.1 - Graphics API (libzargraphics)
 * Creator: Abdelhak Chabbi & AI
 *
 * Public API for applications to draw graphics.
 * Applications link against this instead of calling the renderer directly.
 * In a microkernel architecture, these functions would use IPC
 * to send rendering commands to the Desktop Server.
 */

#ifndef LIBZARGRAPHICS_H
#define LIBZARGRAPHICS_H

#include "zarqaa/types.h"

/* Opaque handle for a surface in user-space */
typedef void* zar_surface_t;

/* Initialize the graphics library connection */
void zar_graphics_init(void);

/* Create a new drawing surface (e.g., for a window) */
zar_surface_t zar_create_surface(int width, int height);

/* Destroy a surface */
void zar_destroy_surface(zar_surface_t surf);

/* Draw a filled rectangle */
void zar_draw_rect(zar_surface_t surf, int x, int y, int w, int h, color_t color);

/* Draw a line */
void zar_draw_line(zar_surface_t surf, int x0, int y0, int x1, int y1, color_t color);

/* Draw text */
void zar_draw_text(zar_surface_t surf, int x, int y, const char *text, color_t fg, color_t bg);

/* Blit (copy) one surface onto another */
void zar_blit_surface(zar_surface_t dst, zar_surface_t src, int dx, int dy, int sx, int sy, int w, int h, bool blend, uint8_t alpha);

/* Tell the compositor this surface is ready to be presented */
void zar_present(zar_surface_t surf);

#endif /* LIBZARGRAPHICS_H */
