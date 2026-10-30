#pragma once

#include "libhtml/dom.h"
#include "graphics/libvector/scene_graph.h"

// Converts an HTML/SVG DOM tree into a Vector Scene Graph
// root: The root of the DOM tree
// returns: The root of the resulting Scene Graph
scene_node_t* dom_to_scene_graph(dom_node_t *root);
