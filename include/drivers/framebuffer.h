/*
 * Zarqaa OS 0.1 - Framebuffer Driver Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DRIVERS_FRAMEBUFFER_H
#define DRIVERS_FRAMEBUFFER_H

#include "zarqaa/types.h"
#include "limine.h"

void fb_init(struct limine_framebuffer *fb);
void fb_putpixel(int x, int y, color_t color);
color_t fb_getpixel(int x, int y);
void fb_fill_rect(int x, int y, int w, int h, color_t color);
void fb_fill_rect_alpha(int x, int y, int w, int h, color_t color, uint8_t alpha);
void fb_fill_rect_clipped(int x, int y, int w, int h, color_t color, const rect_t *clip);
void fb_fill_rect_alpha_clipped(int x, int y, int w, int h, color_t color, uint8_t alpha, const rect_t *clip);
void fb_blend_pixel(int x, int y, color_t color, uint8_t alpha);
void fb_blend_pixel_clipped(int x, int y, color_t color, uint8_t alpha, const rect_t *clip);
void fb_draw_rect(int x, int y, int w, int h, color_t color);
void fb_draw_line(int x0, int y0, int x1, int y1, color_t color);
void fb_draw_char(int x, int y, char c, color_t fg, color_t bg);
void fb_draw_string(int x, int y, const char *s, color_t fg, color_t bg);
void fb_draw_string_transparent(int x, int y, const char *s, color_t fg);
void fb_swap(void);
void fb_clear(color_t color);

/* Internal drawing functions (Phase 1 Compatibility) */
void fb_fill_rect_internal(int x, int y, int w, int h, color_t color);
void fb_draw_rect_internal(int x, int y, int w, int h, color_t color);
void fb_draw_line_internal(int x0, int y0, int x1, int y1, color_t color);
void fb_draw_string_internal(int x, int y, const char *s, color_t fg, color_t bg);
void fb_draw_string_transparent_internal(int x, int y, const char *s, color_t fg);

int fb_get_width(void);
int fb_get_height(void);
int fb_get_pitch(void);
uint32_t *fb_get_backbuffer(void);

/* Render Target API */
void fb_set_render_target(uint32_t *buffer, int w, int h, int pitch);
void fb_reset_render_target(void);

/* V-Sync Control API */
void fb_set_vsync(bool enable);
bool fb_get_vsync(void);
void fb_wait_vsync(void);

#endif /* DRIVERS_FRAMEBUFFER_H */
