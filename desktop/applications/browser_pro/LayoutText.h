#ifndef LAYOUT_TEXT_H
#define LAYOUT_TEXT_H

#include "LayoutBox.h"

class LayoutText : public LayoutBox {
private:
    char* m_text;
    int m_num_lines;
    int m_max_width;  // Used for word wrapping during paint

public:
    LayoutText(struct lxb_dom_node* node, const char* text);
    virtual ~LayoutText();
    
    const char* getText() const { return m_text; }
    
    virtual void layout(int max_width) override;
    virtual void paint(struct window* win) override;
};

#endif // LAYOUT_TEXT_H
