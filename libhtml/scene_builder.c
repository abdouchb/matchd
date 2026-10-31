#include "libhtml/scene_builder.h"
#include "libhtml/svg_parser.h"
#include "graphics/libvector/shape.h"

static color_f_t css_color_to_f(color_t c) {
    if (c == 0) return (color_f_t){0,0,0,0}; // transparent
    return color_to_f(c);
}

scene_node_t* dom_to_scene_graph(dom_node_t *node) {
    if (!node) return NULL;
    
    scene_node_t *snode = scene_node_create();
    if (!snode) return NULL;
    
    // Apply layout transform
    transform_translate(&snode->local_transform, (float)node->style.x, (float)node->style.y);
    
    snode->path = path_create();
    
    // Construct Path based on Node Type
    switch (node->type) {
        case TAG_DIV:
        case TAG_BUTTON:
        case TAG_SPAN:
            if (node->style.border_radius > 0) {
                shape_add_round_rect(snode->path, 0, 0, (float)node->style.width, (float)node->style.height, (float)node->style.border_radius);
            } else {
                shape_add_rect(snode->path, 0, 0, (float)node->style.width, (float)node->style.height);
            }
            break;
            
        case TAG_CIRCLE:
            shape_add_circle(snode->path, (float)node->cx, (float)node->cy, (float)node->r);
            break;
            
        case TAG_RECT:
            shape_add_rect(snode->path, 0, 0, (float)node->style.width, (float)node->style.height);
            break;
            
        case TAG_PATH:
            if (node->svg_d[0] != '\0') {
                svg_parse_path_data(node->svg_d, snode->path);
            }
            break;
            
        default:
            // Text nodes or unknown tags might not have a vector shape (unless we convert text to vectors!)
            // For now, no path.
            break;
    }
    
    // Apply Styles
    if (node->style.bg_color) {
        snode->has_fill = true;
        snode->fill_color = css_color_to_f(node->style.bg_color);
    }
    
    if (node->style.border_width > 0 && node->style.border_color) {
        snode->has_stroke = true;
        snode->stroke_width = (float)node->style.border_width;
        snode->stroke_color = css_color_to_f(node->style.border_color);
    }
    
    // Process Children
    for (int i = 0; i < node->child_count; i++) {
        scene_node_t *child_snode = dom_to_scene_graph(node->children[i]);
        if (child_snode) {
            scene_node_add_child(snode, child_snode);
        }
    }
    
    return snode;
}
