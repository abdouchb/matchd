#include "LayoutFlex.h"

extern "C" {
    #include "desktop/window_manager.h"
    #include "gui/graphics.h"
}
#include "GraphicsUtils.h"

void LayoutFlex::layout(int parent_width) {
    if (style.width >= 0) {
        width = style.width;
    } else {
        width = parent_width - style.margin_left - style.margin_right - style.padding_left - style.padding_right;
    }
    
    // Very simplified flexbox engine (only handles Row with Center justification for now)
    int total_child_width = 0;
    int max_child_height = 0;
    
    LayoutBox* child = first_child;
    while (child) {
        child->layout(width);
        total_child_width += child->width + child->style.margin_left + child->style.margin_right;
        if (child->height > max_child_height) max_child_height = child->height;
        child = child->next_sibling;
    }
    
    int start_x = style.padding_left;
    if (style.flex_direction == FlexDirection::Row && style.justify_content == JustifyContent::Center) {
        start_x += (width - total_child_width) / 2;
    }
    
    int current_x = start_x;
    child = first_child;
    while (child) {
        child->x = current_x + child->style.margin_left;
        child->y = style.padding_top + child->style.margin_top; // simplified
        
        // Align items center
        if (style.align_items == AlignItems::Center) {
            child->y += (max_child_height - child->height) / 2;
        }
        
        current_x += child->width + child->style.margin_left + child->style.margin_right;
        child = child->next_sibling;
    }
    
    if (style.height >= 0) {
        height = style.height;
    } else {
        height = max_child_height + style.padding_top + style.padding_bottom;
    }
}

void LayoutFlex::paint(struct window* win) {
    if (style.bg_color != 0) {
        int abs_x = 0, abs_y = 0;
        LayoutBox* curr = this;
        while (curr) {
            abs_x += curr->x;
            abs_y += curr->y;
            curr = curr->parent;
        }
        draw_rect(win, abs_x, abs_y, width, height, style.bg_color);
    }
    
    LayoutBox* child = first_child;
    while (child) {
        child->paint(win);
        child = child->next_sibling;
    }
}
