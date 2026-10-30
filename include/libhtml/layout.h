#ifndef LIBHTML_LAYOUT_H
#define LIBHTML_LAYOUT_H

#include "libhtml/dom.h"

/* Compute the layout (X, Y, Width, Height) for the entire DOM tree, starting at the root */
void layout_compute(dom_node_t *root, int parent_x, int parent_y, int max_w, int max_h);

#endif /* LIBHTML_LAYOUT_H */
