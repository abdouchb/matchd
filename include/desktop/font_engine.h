/*
 * Zarqaa OS 0.1 - Font Engine
 *
 * Provides text rendering using gpu_renderer primitives.
 */

#ifndef DESKTOP_FONT_ENGINE_H
#define DESKTOP_FONT_ENGINE_H

#include "zarqaa/types.h"

/* Initialize the font engine (loads default fonts) */
void font_engine_init(void);

/* Draw text without transparency (fast) */
void font_engine_draw_string(int x, int y, const char *text, uint32_t color);

/* Draw text with alpha blending (e.g. over wallpaper or windows) */
void font_engine_draw_string_transparent(int x, int y, const char *text, uint32_t color);

/* Measure text width in pixels */
int font_engine_measure_text(const char *text);

#endif /* DESKTOP_FONT_ENGINE_H */
