#ifndef BROWSER_CANVAS_H
#define BROWSER_CANVAS_H

#include "gui/surface.h"
#include "gui/graphics.h"

// Define a rect structure for clipping
typedef struct {
    int x;
    int y;
    int w;
    int h;
} canvas_rect_t;

typedef struct {
    surface_t *target_surface;
    int offset_x;
    int offset_y;
    canvas_rect_t clip_rect;
    bool has_clip;
} canvas_t;

// Canvas lifecycle
canvas_t *canvas_create(surface_t *surf);
void canvas_destroy(canvas_t *canvas);

// State management
void canvas_begin(canvas_t *canvas);
void canvas_end(canvas_t *canvas);
void canvas_set_clip(canvas_t *canvas, int x, int y, int w, int h);
void canvas_clear_clip(canvas_t *canvas);

// Coordinate translation
void canvas_set_offset(canvas_t *canvas, int x, int y);

// Drawing primitives
void canvas_fill_rect(canvas_t *canvas, int x, int y, int w, int h, color_t color);
void canvas_draw_rect(canvas_t *canvas, int x, int y, int w, int h, color_t color);
void canvas_fill_rounded_rect(canvas_t *canvas, int x, int y, int w, int h, int radius, color_t color);
void canvas_draw_line(canvas_t *canvas, int x1, int y1, int x2, int y2, color_t color);
void canvas_draw_text(canvas_t *canvas, int x, int y, const char *text, color_t color);
void canvas_draw_image(canvas_t *canvas, int x, int y, void *image_data, int w, int h);
void canvas_draw_image_rounded(canvas_t *canvas, int x, int y, void *image_data, int w, int h, int radius);
int canvas_measure_text(const char *text);

#endif
