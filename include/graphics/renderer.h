/*
 * Zarqaa OS - Renderer Backend Interface
 */

#ifndef GRAPHICS_RENDERER_H
#define GRAPHICS_RENDERER_H

#include "graphics/render_queue.h"

typedef struct renderer_backend {
    bool (*begin_frame)(void);
    void (*execute_queue)(RenderQueue *queue);
    void (*end_frame)(void);
    void (*present)(void);
} renderer_backend_t;

/* Global active renderer backend */
extern renderer_backend_t *g_current_renderer;

/* Available backends */
extern renderer_backend_t g_software_renderer;
extern renderer_backend_t g_hardware_renderer;

void renderer_set_backend(renderer_backend_t *backend);

#endif /* GRAPHICS_RENDERER_H */
