/*
 * Zarqaa OS 0.1 - GUI Compositor Header
 * Creator: Abdelhak Chabbi & AI
 */

#ifndef GUI_COMPOSITOR_H
#define GUI_COMPOSITOR_H

#include "gui/surface.h"
#include "gui/layer.h"

static inline void compositor_add_surface(surface_t *surf, int x, int y, int z, uint8_t alpha) {
    (void)surf; (void)x; (void)y; (void)z; (void)alpha;
}

static inline void compositor_update_surface(surface_t *surf, int x, int y, int z, uint8_t alpha) {
    (void)surf; (void)x; (void)y; (void)z; (void)alpha;
}

#endif /* GUI_COMPOSITOR_H */
