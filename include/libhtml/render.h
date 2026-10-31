#ifndef LIBHTML_RENDER_H
#define LIBHTML_RENDER_H

#include "libhtml/dom.h"
#include "gui/surface.h"

/* Render the layout-computed DOM tree onto the given surface */
void html_render(surface_t *surf, dom_node_t *root, int offset_x, int offset_y, int clip_y, int clip_h);

#endif /* LIBHTML_RENDER_H */
