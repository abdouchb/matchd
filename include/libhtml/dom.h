#ifndef LIBHTML_DOM_H
#define LIBHTML_DOM_H

#include "zarqaa/types.h"

/* CSS Box Model values */
typedef struct {
    int x;
    int y;
    int left;
    int top;
    int width;
    int height;
    
    /* Computed styles */
    color_t bg_color;
    color_t text_color;
    color_t border_color;
    int border_radius;
    int border_width;
    
    /* Flexbox properties */
    bool is_flex;
    bool flex_col;
    int gap;
    
    /* Z-index for compositing */
    int z_index;
    
    /* Layout values */
    int content_width;
    int content_height;
} css_style_t;

/* HTML Tag Types */
typedef enum {
    TAG_UNKNOWN,
    TAG_DIV,
    TAG_SPAN,
    TAG_SVG,
    TAG_PATH,
    TAG_CIRCLE,
    TAG_RECT,
    TAG_BUTTON,
    TAG_TEXT /* Special node for pure text */
} tag_type_t;

/* DOM Node */
typedef struct dom_node {
    tag_type_t type;
    char name[16];
    char id[32];
    char classes[128];
    char text[256]; /* For text nodes or simple content */
    
    /* Custom attributes e.g., SVG path data */
    char svg_d[512]; 
    int cx, cy, r; /* For SVG circles */
    char href[128]; /* For links */
    
    css_style_t style;
    
    /* Tree pointers */
    struct dom_node *parent;
    struct dom_node *children[64]; /* Simplified static array for now */
    int child_count;
} dom_node_t;

dom_node_t *dom_create_node(tag_type_t type);
void dom_add_child(dom_node_t *parent, dom_node_t *child);
void dom_free(dom_node_t *root);

#endif /* LIBHTML_DOM_H */
