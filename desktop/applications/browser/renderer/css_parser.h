/*
 * ZarqaaKit - CSS Parser Header
 * A lightweight CSS engine for ZarqaaOS Browser
 * Supports: inline styles, <style> tags, basic cascade
 */

#ifndef ZARQAAKIT_CSS_PARSER_H
#define ZARQAAKIT_CSS_PARSER_H

#include "zarqaa/types.h"
#include <stdbool.h>

/* ── CSS Color type ── */
typedef uint32_t css_color_t;
#define CSS_COLOR_TRANSPARENT 0x00000000
#define CSS_COLOR_NONE        0xFF010101  /* Sentinel: "not set" */

/* ── Display type ── */
typedef enum {
    CSS_DISPLAY_BLOCK,
    CSS_DISPLAY_INLINE,
    CSS_DISPLAY_INLINE_BLOCK,
    CSS_DISPLAY_FLEX,
    CSS_DISPLAY_NONE,
    CSS_DISPLAY_INHERIT,
} css_display_t;

/* ── Flex Alignment ── */
typedef enum {
    CSS_FLEX_START,
    CSS_FLEX_END,
    CSS_FLEX_CENTER,
    CSS_FLEX_SPACE_BETWEEN,
    CSS_FLEX_SPACE_AROUND,
} css_flex_align_t;

/* ── Text alignment ── */
typedef enum {
    CSS_TEXT_ALIGN_LEFT,
    CSS_TEXT_ALIGN_CENTER,
    CSS_TEXT_ALIGN_RIGHT,
} css_text_align_t;

/* ── Font weight ── */
typedef enum {
    CSS_FONT_WEIGHT_NORMAL = 400,
    CSS_FONT_WEIGHT_BOLD   = 700,
} css_font_weight_t;

/* ── Border style ── */
typedef enum {
    CSS_BORDER_NONE,
    CSS_BORDER_SOLID,
    CSS_BORDER_DASHED,
} css_border_style_t;

/* ── Computed Style — all CSS properties we support ── */
typedef struct css_style {
    /* Box model */
    int         width;          /* -1 = auto */
    int         height;         /* -1 = auto */
    int         margin_top, margin_right, margin_bottom, margin_left;
    int         padding_top, padding_right, padding_bottom, padding_left;

    /* Colors */
    css_color_t background_color;
    css_color_t color;          /* text color */

    /* Border */
    int                border_width;
    css_border_style_t border_style;
    css_color_t        border_color;
    int                border_radius;

    /* Typography */
    int               font_size;    /* pixels */
    css_font_weight_t font_weight;
    css_text_align_t  text_align;

    /* Layout */
    css_display_t display;
    int           flex_direction; /* 0=row, 1=column */
    css_flex_align_t justify_content;
    css_flex_align_t align_items;
    int           gap;

    /* Opacity / visibility */
    bool visible;

    /* For cascade: which properties were explicitly set */
    uint32_t set_flags;
} css_style_t;

/* Flags for set_flags bitmask */
#define CSS_F_BG_COLOR       (1u << 0)
#define CSS_F_COLOR          (1u << 1)
#define CSS_F_WIDTH          (1u << 2)
#define CSS_F_HEIGHT         (1u << 3)
#define CSS_F_MARGIN         (1u << 4)
#define CSS_F_PADDING        (1u << 5)
#define CSS_F_BORDER         (1u << 6)
#define CSS_F_FONT_SIZE      (1u << 7)
#define CSS_F_FONT_WEIGHT    (1u << 8)
#define CSS_F_TEXT_ALIGN     (1u << 9)
#define CSS_F_DISPLAY        (1u << 10)
#define CSS_F_BORDER_RADIUS  (1u << 11)

/* ── Stylesheet rule (for <style> tag) ── */
#define CSS_SELECTOR_MAX 64
#define CSS_MAX_RULES    256

typedef struct css_rule {
    char       selector[CSS_SELECTOR_MAX];
    css_style_t style;
} css_rule_t;

typedef struct css_stylesheet {
    css_rule_t rules[CSS_MAX_RULES];
    int        count;
} css_stylesheet_t;

/* ── API ── */

/* Initialize a style with all defaults (white bg, black text, block, etc.) */
void css_style_init_default(css_style_t *style);

/* Parse inline style string "color: red; margin: 10px; ..." into style */
void css_parse_inline(const char *style_str, css_style_t *out);

/* Parse a full <style> block "h1 { color: red } .foo { ... }" */
void css_parse_stylesheet(const char *css_text, css_stylesheet_t *out);

/* Apply stylesheet rules matching a tag name or class to style */
void css_apply_rules(const css_stylesheet_t *sheet,
                     const char *tag_name,
                     const char *class_name,
                     const char *id,
                     css_style_t *out);

/* Merge child with parent (inheritance) */
void css_inherit(const css_style_t *parent, css_style_t *child);

/* Parse a CSS color string "#RRGGBB", "rgb(r,g,b)", named color */
css_color_t css_parse_color(const char *str);

/* Parse a CSS length "10px", "1em" (em based on 16px) → pixels */
int css_parse_length(const char *str, int em_base);

#endif /* ZARQAAKIT_CSS_PARSER_H */
