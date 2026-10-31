/*
 * Zarqaa OS 0.1 - Graphics Primitives Header
 * Creator: Abdelhak Chabbi
 */

#ifndef GUI_GRAPHICS_H
#define GUI_GRAPHICS_H

#include "zarqaa/types.h"

void gfx_draw_rounded_rect(int x, int y, int w, int h, int radius, color_t color);
void gfx_draw_rounded_rect_filled(int x, int y, int w, int h, int radius, color_t color);
void gfx_draw_rounded_rect_aa(int x, int y, int w, int h, int radius, color_t color);
void gfx_init_corner_cache(void);
void gfx_draw_rounded_panel_aa(int x, int y, int w, int h, int radius, color_t color, uint8_t alpha);
void gfx_draw_rounded_panel_aa_clipped(int x, int y, int w, int h, int radius, color_t color, uint8_t alpha, const rect_t *clip);
void gfx_draw_gradient_v(int x, int y, int w, int h, color_t top_color, color_t bottom_color);
void gfx_draw_gradient_h(int x, int y, int w, int h, color_t left_color, color_t right_color);
color_t gfx_alpha_blend(color_t fg, color_t bg, uint8_t alpha);
void gfx_draw_circle(int cx, int cy, int r, color_t color);
void gfx_draw_circle_filled(int cx, int cy, int r, color_t color);
void gfx_draw_circle_aa(int cx, int cy, int r, color_t color);
void gfx_draw_glass_panel(int x, int y, int w, int h, int radius,
                          color_t fill, color_t border, uint8_t alpha);
void gfx_draw_dashed_line(int x0, int y0, int x1, int y1,
                          color_t color, int dash, int gap);
void gfx_draw_line_aa(int x0, int y0, int x1, int y1, color_t color);
void gfx_draw_progress_ring(int cx, int cy, int r, int percent,
                            color_t bg_color, color_t fg_color);

#endif /* GUI_GRAPHICS_H */
