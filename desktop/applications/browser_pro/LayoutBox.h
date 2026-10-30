#ifndef LAYOUT_BOX_H
#define LAYOUT_BOX_H

#include "ComputedStyle.h"
#include <stddef.h>

extern "C" {
    struct lxb_dom_node;
}

class LayoutBox {
public:
    struct lxb_dom_node* dom_node;
    ComputedStyle style;
    
    // Geometry computed by layout
    int x, y;
    int width, height;
    
    // Hierarchy
    LayoutBox* parent;
    LayoutBox* first_child;
    LayoutBox* last_child;
    LayoutBox* next_sibling;
    LayoutBox* prev_sibling;
    
    LayoutBox(struct lxb_dom_node* node) : dom_node(node), x(0), y(0), width(0), height(0) {
        parent = nullptr;
        first_child = last_child = nullptr;
        next_sibling = prev_sibling = nullptr;
    }
    
    virtual ~LayoutBox() {
        LayoutBox* child = first_child;
        while (child) {
            LayoutBox* next = child->next_sibling;
            delete child;
            child = next;
        }
    }
    
    void appendChild(LayoutBox* child) {
        child->parent = this;
        if (!first_child) {
            first_child = last_child = child;
        } else {
            last_child->next_sibling = child;
            child->prev_sibling = last_child;
            last_child = child;
        }
    }
    
    virtual void layout(int max_width) = 0;
    virtual void paint(struct window* win) = 0;
};

#endif // LAYOUT_BOX_H
