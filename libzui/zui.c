/*
 * Zarqaa UI (ZUI) Core Implementation
 * Creator: Abdelhak Chabbi
 */

#include "libzui/zui.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"
#include "drivers/usb/usb_hid.h"

void zui_widget_init(zui_widget_t *w, zui_widget_type_t type, int x, int y, int width, int height) {
    if (!w) return;
    w->type = type;
    w->x = x;
    w->y = y;
    w->width = width;
    w->height = height;
    w->bg_color = 0x0;
    w->fg_color = 0xFFFFFFFF;
    w->visible = true;
    w->needs_redraw = true;
    w->parent = NULL;
    w->children = NULL;
    w->child_count = 0;
    w->child_capacity = 0;
    
    w->draw = NULL;
    w->on_mouse = NULL;
    w->destroy = NULL;
    w->user_data = NULL;
}

void zui_widget_add_child(zui_widget_t *parent, zui_widget_t *child) {
    if (!parent || !child) return;
    
    if (parent->child_count >= parent->child_capacity) {
        int new_cap = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        zui_widget_t **new_children = kmalloc(new_cap * sizeof(zui_widget_t *));
        if (parent->children) {
            for (int i = 0; i < parent->child_count; i++) {
                new_children[i] = parent->children[i];
            }
            kfree(parent->children);
        }
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }
    
    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

void zui_widget_draw(zui_widget_t *w, int abs_x, int abs_y) {
    if (!w || !w->visible) return;
    
    int current_abs_x = abs_x + w->x;
    int current_abs_y = abs_y + w->y;
    
    if (w->draw) {
        w->draw(w, current_abs_x, current_abs_y);
    }
    
    w->needs_redraw = false;
    
    for (int i = 0; i < w->child_count; i++) {
        zui_widget_draw(w->children[i], current_abs_x, current_abs_y);
    }
}

bool zui_widget_handle_mouse(zui_widget_t *w, int abs_x, int abs_y, bool is_click) {
    if (!w || !w->visible) return false;
    
    int current_abs_x = abs_x + w->x;
    int current_abs_y = abs_y + w->y;
    
    // Check if mouse is inside this widget
    int mx = mouse_get_x();
    int my = mouse_get_y();
    
    bool inside = (mx >= current_abs_x && mx < current_abs_x + w->width &&
                   my >= current_abs_y && my < current_abs_y + w->height);
                   
    // First, pass event to children (topmost first)
    // Note: To be perfect, we should iterate backwards, assuming later children are drawn on top.
    for (int i = w->child_count - 1; i >= 0; i--) {
        if (zui_widget_handle_mouse(w->children[i], current_abs_x, current_abs_y, is_click)) {
            return true; // Child handled it
        }
    }
    
    // If no child handled it, check if we handle it
    if (inside && w->on_mouse) {
        w->on_mouse(w, mx - current_abs_x, my - current_abs_y, is_click);
        return true;
    }
    
    return false;
}

void zui_widget_destroy(zui_widget_t *w) {
    if (!w) return;
    
    for (int i = 0; i < w->child_count; i++) {
        zui_widget_destroy(w->children[i]);
    }
    
    if (w->children) {
        kfree(w->children);
    }
    
    if (w->destroy) {
        w->destroy(w);
    }
    
    kfree(w);
}
