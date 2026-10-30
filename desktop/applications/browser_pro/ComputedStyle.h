#ifndef COMPUTED_STYLE_H
#define COMPUTED_STYLE_H

#include <stdint.h>

enum class DisplayType {
    None,
    Block,
    Inline,
    InlineBlock,
    Flex
};

enum class FlexDirection {
    Row,
    Column
};

enum class AlignItems {
    Stretch,
    Center,
    FlexStart,
    FlexEnd
};

enum class JustifyContent {
    FlexStart,
    FlexEnd,
    Center,
    SpaceBetween,
    SpaceAround
};

enum class TextAlign {
    Left,
    Center,
    Right
};

class ComputedStyle {
public:
    DisplayType display;
    
    // Box Model
    int width;
    int height;
    int max_width;
    
    int margin_top, margin_right, margin_bottom, margin_left;
    int padding_top, padding_right, padding_bottom, padding_left;
    int border_top, border_right, border_bottom, border_left;
    uint32_t border_color;
    
    // Visual
    uint32_t bg_color;
    uint32_t color;
    bool color_set;
    int border_radius;
    
    // Flexbox
    FlexDirection flex_direction;
    AlignItems align_items;
    JustifyContent justify_content;
    int flex_grow;
    
    // Content flags
    bool is_image;
    bool is_link;
    bool is_bold;
    bool is_italic;
    bool is_centered;
    bool is_underline;
    bool is_line_break;    // <br> tag
    bool is_horizontal_rule; // <hr> tag
    
    // Text
    int font_size;
    TextAlign text_align;
    
    ComputedStyle() {
        display = DisplayType::Block;
        width = -1; // Auto
        height = -1; // Auto
        max_width = -1;
        
        margin_top = margin_right = margin_bottom = margin_left = 0;
        padding_top = padding_right = padding_bottom = padding_left = 0;
        border_top = border_right = border_bottom = border_left = 0;
        
        bg_color = 0; // Transparent
        color = 0xFF000000; // Black text (standard browser default)
        color_set = false;
        border_color = 0; // Transparent
        border_radius = 0;
        
        flex_direction = FlexDirection::Row;
        align_items = AlignItems::Stretch;
        justify_content = JustifyContent::FlexStart;
        flex_grow = 0;
        
        is_image = false;
        is_link = false;
        is_bold = false;
        is_italic = false;
        is_centered = false;
        is_underline = false;
        is_line_break = false;
        is_horizontal_rule = false;
        
        font_size = 16;
        text_align = TextAlign::Left;
    }
};

#endif // COMPUTED_STYLE_H
