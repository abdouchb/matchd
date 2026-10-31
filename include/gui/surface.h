/*
 * Zarqaa OS - Window Surface and Scene Graph
 */

#ifndef GUI_SURFACE_H
#define GUI_SURFACE_H

#include "graphics/graphics_api.h"
#include "graphics/render_queue.h"

#define PIXEL_FORMAT_ARGB32 1

typedef struct surface {
    RenderQueue queue;
    render_context_t context;
    int x;
    int y;
    int width;
    int height;
    float opacity; /* For Compositor Phase 5 */
    bool visible;
    int pitch;
    int format;
    void *cpu_pointer;
    void *pixels;
} surface_t;

static inline void surface_init(surface_t *surf, int x, int y, int w, int h) {
    if (!surf) return;
    render_queue_init(&surf->queue);
    gapi_context_init(&surf->context);
    surf->context.queue = &surf->queue; /* Bind context to this queue */
    surf->x = x;
    surf->y = y;
    surf->width = w;
    surf->height = h;
    surf->opacity = 1.0f;
    surf->visible = true;
}

static inline void surface_destroy(surface_t *surf) {
    if (!surf) return;
    render_queue_destroy(&surf->queue);
}

#endif /* GUI_SURFACE_H */
