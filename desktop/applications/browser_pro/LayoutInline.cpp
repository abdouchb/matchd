#include "LayoutInline.h"

extern "C" {
    #include "desktop/window_manager.h"
}
#include "GraphicsUtils.h"

void LayoutInline::layout(int parent_width) {
    int current_x = style.padding_left;
    int max_h = 0;
    
    LayoutBox* child = first_child;
    while (child) {
        child->layout(parent_width); // Inline children or text
        
        child->x = current_x;
        child->y = style.padding_top; // Relative to line box
        
        current_x += child->width;
        if (child->height > max_h) {
            max_h = child->height;
        }
        
        child = child->next_sibling;
    }
    
    if (style.width >= 0) {
        width = style.width;
    } else {
        width = current_x + style.padding_right;
    }
    
    if (style.height >= 0) {
        height = style.height;
    } else {
        height = max_h + style.padding_top + style.padding_bottom;
    }
}

void LayoutInline::paint(struct window* win) {
    if (style.bg_color != 0 || style.is_image) {
        int abs_x = 0, abs_y = 0;
        LayoutBox* curr = this;
        while (curr) {
            abs_x += curr->x;
            abs_y += curr->y;
            curr = curr->parent;
        }
        
        if (style.is_image) {
            // Draw placeholder image box
            int img_w = style.width > 0 ? style.width : 50;
            int img_h = style.height > 0 ? style.height : 50;
            draw_rect(win, abs_x, abs_y, img_w, img_h, 0xFF555555);
            // Draw [IMG] text if we have space
            // Assuming we have a way to draw text here, or we can just leave it as a grey box
        } else {
            draw_rect(win, abs_x, abs_y, width, height, style.bg_color);
        }
        
        if (style.border_color != 0) {
            draw_rect(win, abs_x, abs_y, width, 1, style.border_color); // Top
            draw_rect(win, abs_x, abs_y + height - 1, width, 1, style.border_color); // Bottom
            draw_rect(win, abs_x, abs_y, 1, height, style.border_color); // Left
            draw_rect(win, abs_x + width - 1, abs_y, 1, height, style.border_color); // Right
        }
    }

    LayoutBox* child = first_child;
    while (child) {
        child->paint(win);
        child = child->next_sibling;
    }
}
