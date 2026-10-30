#ifndef LAYOUT_BLOCK_H
#define LAYOUT_BLOCK_H

#include "LayoutBox.h"

class LayoutBlock : public LayoutBox {
public:
    LayoutBlock(struct lxb_dom_node* node) : LayoutBox(node) {}
    
    virtual void layout(int max_width) override;
    virtual void paint(struct window* win) override;
};

#endif // LAYOUT_BLOCK_H
