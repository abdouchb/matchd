#include "LayoutBlock.h"

extern "C" {
    #include "desktop/window_manager.h"
    #include "gui/graphics.h"
    void kprintf(const char* fmt, ...);
}

#include "Theme.h"
#include "GraphicsUtils.h"

// Check if a child is inline-level (text, inline, inlineblock)
static bool isInlineChild(LayoutBox* child) {
    if (!child) return false;
    return (child->style.display == DisplayType::Inline || 
            child->style.display == DisplayType::InlineBlock);
}

static LayoutBox* layoutInlineRun(LayoutBox* first_inline, LayoutBox* block_parent, int content_width, int parent_width, int& current_y) {
    int line_x = block_parent->style.padding_left;
    int line_height = 0;
    LayoutBox* line_start = first_inline;
    LayoutBox* child = first_inline;
    
    while (child && isInlineChild(child)) {
        child->layout(content_width);
        
        int child_total_w = child->style.margin_left + child->width + child->style.margin_right;
        
        if (line_x + child_total_w > parent_width - block_parent->style.padding_right && line_x > block_parent->style.padding_left) {
            // Apply centering before wrapping
            if (block_parent->style.is_centered && line_x < parent_width - block_parent->style.padding_right) {
                int offset = (parent_width - block_parent->style.padding_right - line_x) / 2;
                LayoutBox* curr = line_start;
                while (curr && curr != child) {
                    curr->x += offset;
                    curr = curr->next_sibling;
                }
            }
            
            current_y += line_height;
            line_x = block_parent->style.padding_left;
            line_height = 0;
            line_start = child;
        }
        
        child->x = line_x + child->style.margin_left;
        child->y = current_y + child->style.margin_top;
        
        line_x += child_total_w;
        
        int child_total_h = child->style.margin_top + child->height + child->style.margin_bottom;
        if (child_total_h > line_height) line_height = child_total_h;
        
        child = child->next_sibling;
    }
    
    // Account for the last line and center it
    if (block_parent->style.is_centered && line_x < parent_width - block_parent->style.padding_right && line_start) {
        int offset = (parent_width - block_parent->style.padding_right - line_x) / 2;
        LayoutBox* curr = line_start;
        while (curr && curr != child) {
            curr->x += offset;
            curr = curr->next_sibling;
        }
    }
    
    current_y += line_height;
    return child;
}

void LayoutBlock::layout(int parent_width) {
    // 1. Calculate own width
    if (style.width >= 0) {
        width = style.width;
    } else {
        width = parent_width - style.margin_left - style.margin_right;
    }
    
    if (style.max_width > 0 && width > style.max_width) {
        width = style.max_width;
    }
    
    int content_width = width - style.padding_left - style.padding_right;
    if (content_width < 0) content_width = 0;
    
    int current_y = style.padding_top;
    
    LayoutBox* child = first_child;
    while (child) {
        if (isInlineChild(child)) {
            child = layoutInlineRun(child, this, content_width, width, current_y);
        } else {
            // Block child
            child->layout(content_width);
            child->x = style.padding_left + child->style.margin_left;
            child->y = current_y + child->style.margin_top;
            
            current_y += child->style.margin_top + child->height + child->style.margin_bottom;
            child = child->next_sibling;
        }
    }
    
    // 3. Set own height
    if (style.height >= 0) {
        height = style.height;
    } else {
        height = current_y + style.padding_bottom;
    }
}

void LayoutBlock::paint(struct window* win) {
    // Calculate absolute position
    int abs_x = 0, abs_y = 0;
    LayoutBox* curr = this;
    while (curr) {
        abs_x += curr->x;
        abs_y += curr->y;
        curr = curr->parent;
    }
    
    // Viewport clipping: skip if entirely off-screen
    if (abs_y >= win->height || abs_y + height < 0) return;
    if (abs_x >= win->width || abs_x + width < 0) return;
    
    // Paint background
    if (style.bg_color != 0) {
        draw_rect(win, abs_x, abs_y, width, height, style.bg_color);
    }
    
    // Paint <hr> as a horizontal line
    if (style.is_horizontal_rule) {
        int line_w = width > 0 ? width : (win->width - abs_x);
        draw_rect(win, abs_x, abs_y, line_w, 1, 0xFFCCCCCC);
        return; // <hr> has no children
    }
    
    // Paint children
    LayoutBox* child = first_child;
    while (child) {
        child->paint(win);
        child = child->next_sibling;
    }
}
