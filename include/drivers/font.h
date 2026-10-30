/*
 * Zarqaa OS 0.1 - Font Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DRIVERS_FONT_H
#define DRIVERS_FONT_H

#include "zarqaa/types.h"

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

const uint8_t *font_get_glyph(char c);
void font_build_glyph_aa_cache(void);
const uint8_t *font_get_glyph_alpha_row(char c, int y);
void fb_draw_string_aa_clipped(int x, int y, const char *s, color_t fg, const rect_t *clip);

#endif /* DRIVERS_FONT_H */
