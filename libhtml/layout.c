#include "libhtml/layout.h"

void layout_compute(dom_node_t *node, int parent_x, int parent_y, int max_w, int max_h) {
    if (!node) return;
    
    /* 1. Base position */
    node->style.x = parent_x + node->style.left;
    node->style.y = parent_y + node->style.top;
    
    /* Default to max width if block-level, else 0 (to be computed by children) */
    if (node->style.width == 0 && (node->type == TAG_DIV || node->type == TAG_SVG)) {
        node->style.width = max_w;
    }
    
    /* 2. Layout children */
    int current_child_x = node->style.x;
    int current_child_y = node->style.y;
    int max_child_w = 0;
    int max_child_h = 0;
    
    for (int i = 0; i < node->child_count; i++) {
        dom_node_t *child = node->children[i];
        
        /* Available space for child */
        int avail_w = node->style.width > 0 ? node->style.width : max_w;
        int avail_h = max_h;
        
        layout_compute(child, current_child_x, current_child_y, avail_w, avail_h);
        
        /* Advance layout cursor based on flexbox */
        if (node->type != TAG_SVG) { /* SVG children are absolute */
            if (node->style.is_flex) {
                if (node->style.flex_col) {
                    current_child_y += child->style.height + node->style.gap;
                    if (child->style.width > max_child_w) max_child_w = child->style.width;
                    max_child_h += child->style.height + node->style.gap;
                } else {
                    current_child_x += child->style.width + node->style.gap;
                    max_child_w += child->style.width + node->style.gap;
                    if (child->style.height > max_child_h) max_child_h = child->style.height;
                }
            } else {
                /* Default Block Layout */
                current_child_y += child->style.height;
                if (child->style.width > max_child_w) max_child_w = child->style.width;
                max_child_h += child->style.height;
            }
        }
    }
    
    /* 3. Compute own size if not set */
    if (node->style.width == 0) {
        if (node->type == TAG_TEXT) {
            /* Simple text size estimation: 8px width per char */
            int len = 0;
            while(node->text[len]) len++;
            node->style.width = len * 8;
        } else {
            node->style.width = max_child_w;
        }
    }
    
    if (node->style.height == 0) {
        if (node->type == TAG_TEXT) {
            node->style.height = 16; /* 16px font height */
        } else {
            node->style.height = max_child_h;
        }
    }
}
