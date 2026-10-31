#ifndef LAYOUT_INLINE_H
#define LAYOUT_INLINE_H

#include "LayoutBox.h"

class LayoutInline : public LayoutBox {
public:
    LayoutInline(struct lxb_dom_node* node) : LayoutBox(node) {
        style.display = DisplayType::Inline;
    }
    
    virtual void layout(int max_width) override;
    virtual void paint(struct window* win) override;
};

#endif // LAYOUT_INLINE_H
