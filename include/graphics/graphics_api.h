/*
 * Zarqaa OS - Unified Graphics API (Phase 1)
 */

#ifndef GRAPHICS_API_H
#define GRAPHICS_API_H

#include "zarqaa/types.h"

/* Forward declaration for Render Queue (Phase 2) */
struct RenderQueue;
typedef struct RenderQueue RenderQueue;

/* Transform Matrix (3x3 for 2D transformations) */
typedef struct {
    float m[3][3];
} matrix_t;

/* Render Context - Encapsulates all state for drawing */
typedef struct render_context {
    RenderQueue *queue;      /* NULL in Phase 1, used in Phase 2 */
    rect_t clip;
    matrix_t transform;
    color_t color;
    void *font;              /* Pointer to font, NULL for default */
    float opacity;           /* 0.0 to 1.0 */
    uint32_t flags;
} render_context_t;

/* Initialize a default context */
void gapi_context_init(render_context_t *ctx);

/* Set the active context for wrappers (used by window_manager) */
void gapi_set_current_context(render_context_t *ctx);
render_context_t *gapi_get_current_context(void);

/* 
 * The Unified Graphics API
 * In Phase 1, these act as wrappers directly calling framebuffer functions.
 */

void gapi_fill_rect(render_context_t *ctx, int x, int y, int w, int h, color_t color);
void gapi_draw_rect(render_context_t *ctx, int x, int y, int w, int h, color_t color);
void gapi_draw_line(render_context_t *ctx, int x0, int y0, int x1, int y1, color_t color);
void gapi_draw_string(render_context_t *ctx, int x, int y, const char *s, color_t fg, color_t bg);
void gapi_draw_string_transparent(render_context_t *ctx, int x, int y, const char *s, color_t fg);
void gapi_draw_image(render_context_t *ctx, int x, int y, int w, int h, uint32_t *pixels);

#endif /* GRAPHICS_API_H */
