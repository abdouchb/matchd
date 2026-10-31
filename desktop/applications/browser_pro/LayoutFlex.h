#ifndef LAYOUT_FLEX_H
#define LAYOUT_FLEX_H

#include "LayoutBox.h"

class LayoutFlex : public LayoutBox {
public:
    LayoutFlex(struct lxb_dom_node* node) : LayoutBox(node) {
        style.display = DisplayType::Flex;
    }
    
    virtual void layout(int max_width) override;
    virtual void paint(struct window* win) override;
};

#endif // LAYOUT_FLEX_H
