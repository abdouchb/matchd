/*
 * ZarqaaKit - CSS Parser Implementation
 * Lightweight CSS engine for ZarqaaOS
 * Supports: colors, box model, typography, flexbox, border, display
 */

#include "css_parser.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"

/* ═══════════════════════════════════════════
 *  Utility helpers
 * ═══════════════════════════════════════════ */

static inline bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
static inline char to_lower(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static void str_trim(const char *in, char *out, int maxlen) {
    while (*in && is_space(*in)) in++;
    int len = strlen(in);
    while (len > 0 && is_space(in[len-1])) len--;
    if (len >= maxlen) len = maxlen - 1;
    memcpy(out, in, len);
    out[len] = '\0';
}

static void str_lower(char *s) {
    while (*s) { *s = to_lower(*s); s++; }
}

static int str_startswith(const char *s, const char *prefix) {
    while (*prefix) {
        if (to_lower(*s) != to_lower(*prefix)) return 0;
        s++; prefix++;
    }
    return 1;
}

/* Simple atoi that handles negative */
static int simple_atoi(const char *s) {
    int neg = 0, val = 0;
    if (*s == '-') { neg = 1; s++; }
    while (is_digit(*s)) { val = val * 10 + (*s - '0'); s++; }
    return neg ? -val : val;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return 0;
}

/* ═══════════════════════════════════════════
 *  css_parse_color
 * ═══════════════════════════════════════════ */

css_color_t css_parse_color(const char *raw) {
    if (!raw) return CSS_COLOR_NONE;

    char s[64];
    str_trim(raw, s, sizeof(s));
    str_lower(s);

    /* transparent */
    if (strcmp(s, "transparent") == 0) return CSS_COLOR_TRANSPARENT;

    /* #RGB or #RRGGBB */
    if (s[0] == '#') {
        const char *h = s + 1;
        int len = strlen(h);
        if (len == 3) {
            int r = hex_digit(h[0]) * 17;
            int g = hex_digit(h[1]) * 17;
            int b = hex_digit(h[2]) * 17;
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        } else if (len >= 6) {
            int r = hex_digit(h[0]) * 16 + hex_digit(h[1]);
            int g = hex_digit(h[2]) * 16 + hex_digit(h[3]);
            int b = hex_digit(h[4]) * 16 + hex_digit(h[5]);
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        return CSS_COLOR_NONE;
    }

    /* rgb(r, g, b) */
    if (str_startswith(s, "rgb(")) {
        int r = 0, g = 0, b = 0;
        const char *p = s + 4;
        r = simple_atoi(p);
        while (*p && *p != ',') p++; if (*p) p++;
        g = simple_atoi(p);
        while (*p && *p != ',') p++; if (*p) p++;
        b = simple_atoi(p);
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    /* rgba(r, g, b, a) */
    if (str_startswith(s, "rgba(")) {
        int r = 0, g = 0, b = 0;
        const char *p = s + 5;
        r = simple_atoi(p);
        while (*p && *p != ',') p++; if (*p) p++;
        g = simple_atoi(p);
        while (*p && *p != ',') p++; if (*p) p++;
        b = simple_atoi(p);
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    /* Named colors (common subset) */
    struct { const char *name; css_color_t color; } named[] = {
        {"white",       0xFFFFFFFF}, {"black",       0xFF000000},
        {"red",         0xFFFF0000}, {"green",        0xFF00C800},
        {"blue",        0xFF0000FF}, {"yellow",       0xFFFFFF00},
        {"orange",      0xFFFF8800}, {"purple",       0xFF800080},
        {"pink",        0xFFFF69B4}, {"gray",         0xFF808080},
        {"grey",        0xFF808080}, {"darkgray",     0xFFA9A9A9},
        {"lightgray",   0xFFD3D3D3}, {"lightgrey",    0xFFD3D3D3},
        {"darkblue",    0xFF00008B}, {"lightblue",    0xFFADD8E6},
        {"cyan",        0xFF00FFFF}, {"magenta",      0xFFFF00FF},
        {"lime",        0xFF00FF00}, {"teal",         0xFF008080},
        {"navy",        0xFF000080}, {"maroon",       0xFF800000},
        {"olive",       0xFF808000}, {"silver",       0xFFC0C0C0},
        {"darkgreen",   0xFF006400}, {"tomato",       0xFFFF6347},
        {"coral",       0xFFFF7F50}, {"salmon",       0xFFFA8072},
        {"gold",        0xFFFFD700}, {"khaki",        0xFFF0E68C},
        {"indigo",      0xFF4B0082}, {"violet",       0xFFEE82EE},
        {"crimson",     0xFFDC143C}, {"turquoise",    0xFF40E0D0},
        {"transparent", 0x00000000},
        {NULL, 0}
    };
    for (int i = 0; named[i].name; i++) {
        if (strcmp(s, named[i].name) == 0) return named[i].color;
    }

    return CSS_COLOR_NONE; /* Unknown */
}

/* ═══════════════════════════════════════════
 *  css_parse_length
 * ═══════════════════════════════════════════ */

int css_parse_length(const char *raw, int em_base) {
    if (!raw) return 0;
    char s[32];
    str_trim(raw, s, sizeof(s));
    if (!s[0]) return 0;

    int val = simple_atoi(s);

    /* Find unit */
    const char *p = s;
    if (*p == '-') p++;
    while (is_digit(*p)) p++;

    if (str_startswith(p, "em")) return val * em_base;
    if (str_startswith(p, "rem")) return val * 16;
    if (str_startswith(p, "%")) return -1; /* % not resolved here */
    /* px or unitless */
    return val;
}

/* ═══════════════════════════════════════════
 *  css_style_init_default
 * ═══════════════════════════════════════════ */

void css_style_init_default(css_style_t *style) {
    memset(style, 0, sizeof(*style));
    style->width  = -1; /* auto */
    style->height = -1; /* auto */
    style->background_color = CSS_COLOR_TRANSPARENT;
    style->color  = CSS_COLOR_NONE; /* inherit */
    style->font_size   = 14;
    style->font_weight = CSS_FONT_WEIGHT_NORMAL;
    style->text_align  = CSS_TEXT_ALIGN_LEFT;
    style->display = CSS_DISPLAY_BLOCK;
    style->visible = true;
    style->border_color = CSS_COLOR_NONE;
    style->border_style = CSS_BORDER_NONE;
    style->border_width = 0;
    style->border_radius = 0;
    style->set_flags = 0;
}

/* ═══════════════════════════════════════════
 *  Parse a single "property: value" pair into style
 * ═══════════════════════════════════════════ */

static void apply_property(const char *prop, const char *value, css_style_t *out) {
    char p[64], v[128];
    str_trim(prop,  p, sizeof(p));
    str_trim(value, v, sizeof(v));
    str_lower(p);

    if (strcmp(p, "color") == 0) {
        css_color_t c = css_parse_color(v);
        if (c != CSS_COLOR_NONE) { out->color = c; out->set_flags |= CSS_F_COLOR; }

    } else if (strcmp(p, "background-color") == 0 ||
               strcmp(p, "background") == 0) {
        css_color_t c = css_parse_color(v);
        if (c != CSS_COLOR_NONE) { out->background_color = c; out->set_flags |= CSS_F_BG_COLOR; }

    } else if (strcmp(p, "width") == 0) {
        int n = css_parse_length(v, out->font_size);
        if (n >= 0) { out->width = n; out->set_flags |= CSS_F_WIDTH; }

    } else if (strcmp(p, "height") == 0) {
        int n = css_parse_length(v, out->font_size);
        if (n >= 0) { out->height = n; out->set_flags |= CSS_F_HEIGHT; }

    } else if (strcmp(p, "font-size") == 0) {
        int n = css_parse_length(v, 16);
        if (n > 0) { out->font_size = n; out->set_flags |= CSS_F_FONT_SIZE; }

    } else if (strcmp(p, "font-weight") == 0) {
        str_lower(v);
        if (strcmp(v, "bold") == 0 || simple_atoi(v) >= 600)
            out->font_weight = CSS_FONT_WEIGHT_BOLD;
        else
            out->font_weight = CSS_FONT_WEIGHT_NORMAL;
        out->set_flags |= CSS_F_FONT_WEIGHT;

    } else if (strcmp(p, "text-align") == 0) {
        str_lower(v);
        if (strcmp(v, "center") == 0) out->text_align = CSS_TEXT_ALIGN_CENTER;
        else if (strcmp(v, "right") == 0) out->text_align = CSS_TEXT_ALIGN_RIGHT;
        else out->text_align = CSS_TEXT_ALIGN_LEFT;
        out->set_flags |= CSS_F_TEXT_ALIGN;

    } else if (strcmp(p, "display") == 0) {
        str_lower(v);
        if (strcmp(v, "none") == 0) out->display = CSS_DISPLAY_NONE;
        else if (strcmp(v, "inline") == 0) out->display = CSS_DISPLAY_INLINE;
        else if (strcmp(v, "inline-block") == 0) out->display = CSS_DISPLAY_INLINE_BLOCK;
        else if (strcmp(v, "inline-flex") == 0) out->display = CSS_DISPLAY_INLINE_BLOCK;
        else if (strcmp(v, "flex") == 0) out->display = CSS_DISPLAY_FLEX;
        else if (strcmp(v, "grid") == 0) out->display = CSS_DISPLAY_BLOCK;
        else out->display = CSS_DISPLAY_BLOCK;
        out->set_flags |= CSS_F_DISPLAY;

    } else if (strcmp(p, "flex-direction") == 0) {
        str_lower(v);
        out->flex_direction = (strcmp(v, "column") == 0) ? 1 : 0;

    } else if (strcmp(p, "justify-content") == 0) {
        str_lower(v);
        if (strcmp(v, "center") == 0) out->justify_content = CSS_FLEX_CENTER;
        else if (strcmp(v, "flex-end") == 0 || strcmp(v, "end") == 0) out->justify_content = CSS_FLEX_END;
        else if (strcmp(v, "space-between") == 0) out->justify_content = CSS_FLEX_SPACE_BETWEEN;
        else if (strcmp(v, "space-around") == 0) out->justify_content = CSS_FLEX_SPACE_AROUND;
        else out->justify_content = CSS_FLEX_START;

    } else if (strcmp(p, "align-items") == 0) {
        str_lower(v);
        if (strcmp(v, "center") == 0) out->align_items = CSS_FLEX_CENTER;
        else if (strcmp(v, "flex-end") == 0 || strcmp(v, "end") == 0) out->align_items = CSS_FLEX_END;
        else out->align_items = CSS_FLEX_START;

    } else if (strcmp(p, "gap") == 0) {
        out->gap = css_parse_length(v, out->font_size);

    } else if (strcmp(p, "margin") == 0) {
        /* shorthand: 1-4 values */
        int vals[4] = {0,0,0,0}; int n = 0;
        char *tok = v;
        while (*tok && n < 4) {
            while (is_space(*tok)) tok++;
            if (!*tok) break;
            char tmp[16]; int i = 0;
            while (*tok && !is_space(*tok) && i < 15) tmp[i++] = *tok++;
            tmp[i] = '\0';
            vals[n++] = css_parse_length(tmp, out->font_size);
        }
        if (n == 1) { vals[1] = vals[2] = vals[3] = vals[0]; }
        else if (n == 2) { vals[2] = vals[0]; vals[3] = vals[1]; }
        else if (n == 3) { vals[3] = vals[1]; }
        out->margin_top    = vals[0];
        out->margin_right  = vals[1];
        out->margin_bottom = vals[2];
        out->margin_left   = vals[3];
        out->set_flags |= CSS_F_MARGIN;

    } else if (strcmp(p, "margin-top") == 0) {
        out->margin_top = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_MARGIN;
    } else if (strcmp(p, "margin-bottom") == 0) {
        out->margin_bottom = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_MARGIN;
    } else if (strcmp(p, "margin-left") == 0) {
        out->margin_left = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_MARGIN;
    } else if (strcmp(p, "margin-right") == 0) {
        out->margin_right = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_MARGIN;

    } else if (strcmp(p, "padding") == 0) {
        int vals[4] = {0,0,0,0}; int n = 0;
        char *tok = v;
        while (*tok && n < 4) {
            while (is_space(*tok)) tok++;
            if (!*tok) break;
            char tmp[16]; int i = 0;
            while (*tok && !is_space(*tok) && i < 15) tmp[i++] = *tok++;
            tmp[i] = '\0';
            vals[n++] = css_parse_length(tmp, out->font_size);
        }
        if (n == 1) { vals[1] = vals[2] = vals[3] = vals[0]; }
        else if (n == 2) { vals[2] = vals[0]; vals[3] = vals[1]; }
        else if (n == 3) { vals[3] = vals[1]; }
        out->padding_top    = vals[0];
        out->padding_right  = vals[1];
        out->padding_bottom = vals[2];
        out->padding_left   = vals[3];
        out->set_flags |= CSS_F_PADDING;

    } else if (strcmp(p, "padding-top") == 0) {
        out->padding_top = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_PADDING;
    } else if (strcmp(p, "padding-bottom") == 0) {
        out->padding_bottom = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_PADDING;
    } else if (strcmp(p, "padding-left") == 0) {
        out->padding_left = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_PADDING;
    } else if (strcmp(p, "padding-right") == 0) {
        out->padding_right = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_PADDING;

    } else if (strcmp(p, "border-radius") == 0) {
        out->border_radius = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_BORDER_RADIUS;

    } else if (strcmp(p, "border") == 0) {
        /* shorthand: "1px solid #333" */
        char tmp[128]; str_trim(v, tmp, sizeof(tmp));
        /* width */
        char *sp = tmp;
        while (*sp && !is_space(*sp)) sp++;
        char wstr[16] = {0};
        int wlen = (int)(sp - tmp);
        if (wlen >= 16) wlen = 15;
        memcpy(wstr, tmp, wlen);
        out->border_width = css_parse_length(wstr, out->font_size);

        /* style */
        while (is_space(*sp)) sp++;
        while (*sp && !is_space(*sp)) sp++;
        out->border_style = CSS_BORDER_SOLID; /* assume solid */

        /* color */
        while (is_space(*sp)) sp++;
        if (*sp) {
            out->border_color = css_parse_color(sp);
            if (out->border_color == CSS_COLOR_NONE)
                out->border_color = 0xFF000000;
        }
        out->set_flags |= CSS_F_BORDER;

    } else if (strcmp(p, "border-color") == 0) {
        out->border_color = css_parse_color(v);
        out->set_flags |= CSS_F_BORDER;
    } else if (strcmp(p, "border-width") == 0) {
        out->border_width = css_parse_length(v, out->font_size);
        out->set_flags |= CSS_F_BORDER;
    } else if (strcmp(p, "border-style") == 0) {
        str_lower(v);
        if (strcmp(v, "solid") == 0) out->border_style = CSS_BORDER_SOLID;
        else if (strcmp(v, "dashed") == 0) out->border_style = CSS_BORDER_DASHED;
        else out->border_style = CSS_BORDER_NONE;
        out->set_flags |= CSS_F_BORDER;

    } else if (strcmp(p, "visibility") == 0) {
        str_lower(v);
        out->visible = (strcmp(v, "hidden") != 0);
    }
    /* Unknown properties are silently ignored */
}

/* ═══════════════════════════════════════════
 *  css_parse_inline
 * ═══════════════════════════════════════════ */

void css_parse_inline(const char *style_str, css_style_t *out) {
    if (!style_str || !out) return;

    char buf[512];
    str_trim(style_str, buf, sizeof(buf));

    char *p = buf;
    while (*p) {
        /* Find ':' separator */
        char *colon = p;
        while (*colon && *colon != ':' && *colon != ';') colon++;
        if (*colon != ':') { /* no colon, skip to next semicolon */
            while (*p && *p != ';') p++;
            if (*p == ';') p++;
            continue;
        }

        /* Property */
        char prop[64] = {0};
        int plen = (int)(colon - p);
        if (plen >= 64) plen = 63;
        memcpy(prop, p, plen);

        /* Value: everything until ';' or end */
        char *val_start = colon + 1;
        char *semi = val_start;
        while (*semi && *semi != ';') semi++;
        char val[128] = {0};
        int vlen = (int)(semi - val_start);
        if (vlen >= 128) vlen = 127;
        memcpy(val, val_start, vlen);

        apply_property(prop, val, out);

        p = semi;
        if (*p == ';') p++;
    }
}

/* ═══════════════════════════════════════════
 *  css_parse_stylesheet
 *  Parses a full CSS text into a stylesheet of rules
 * ═══════════════════════════════════════════ */

void css_parse_stylesheet(const char *css_text, css_stylesheet_t *out) {
    if (!css_text || !out) return;

    char buf[4096];
    str_trim(css_text, buf, sizeof(buf));

    char *p = buf;
    while (*p && out->count < CSS_MAX_RULES) {
        /* Skip whitespace / comments */
        while (is_space(*p)) p++;
        if (!*p) break;

        /* Skip C-style comments */
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }

        /* Read selector: everything until '{' */
        char *brace = p;
        while (*brace && *brace != '{') brace++;
        if (!*brace) break;

        char selector[CSS_SELECTOR_MAX] = {0};
        int slen = (int)(brace - p);
        if (slen >= CSS_SELECTOR_MAX) slen = CSS_SELECTOR_MAX - 1;
        memcpy(selector, p, slen);
        /* Trim selector */
        {
            int l = 0, r = slen - 1;
            while (l <= r && is_space(selector[l])) l++;
            while (r >= l && is_space(selector[r])) r--;
            memmove(selector, selector + l, r - l + 1);
            selector[r - l + 1] = '\0';
        }
        str_lower(selector);

        /* Read body: everything until '}' */
        char *body_start = brace + 1;
        char *close = body_start;
        while (*close && *close != '}') close++;
        if (!*close) break;

        char body[512] = {0};
        int blen = (int)(close - body_start);
        if (blen >= 512) blen = 511;
        memcpy(body, body_start, blen);

        /* Store rule */
        css_rule_t *rule = &out->rules[out->count++];
        strncpy(rule->selector, selector, CSS_SELECTOR_MAX - 1);
        css_style_init_default(&rule->style);
        css_parse_inline(body, &rule->style);

        p = close + 1;
    }
}

/* ═══════════════════════════════════════════
 *  css_apply_rules
 *  Match rules by tag, .class or #id
 * ═══════════════════════════════════════════ */

static int selector_matches(const char *sel,
                             const char *tag,
                             const char *cls,
                             const char *id) {
    /* Element selector: "div", "p", "h1" etc. */
    if (tag && strcmp(sel, tag) == 0) return 1;

    /* Class selector: ".foo" */
    if (sel[0] == '.' && cls) {
        const char *needle = sel + 1;
        /* Class may be multiple space-separated */
        const char *h = cls;
        while (*h) {
            while (is_space(*h)) h++;
            const char *end = h;
            while (*end && !is_space(*end)) end++;
            int len = (int)(end - h);
            if (len == (int)strlen(needle) && strncmp(h, needle, len) == 0)
                return 1;
            h = end;
        }
    }

    /* ID selector: "#myid" */
    if (sel[0] == '#' && id && strcmp(sel + 1, id) == 0) return 1;

    /* Universal selector */
    if (strcmp(sel, "*") == 0) return 1;

    return 0;
}

void css_apply_rules(const css_stylesheet_t *sheet,
                     const char *tag_name,
                     const char *class_name,
                     const char *id,
                     css_style_t *out) {
    if (!sheet || !out) return;
    for (int i = 0; i < sheet->count; i++) {
        const css_rule_t *rule = &sheet->rules[i];
        if (!selector_matches(rule->selector, tag_name, class_name, id))
            continue;
        /* Merge: only set properties that were explicitly set in this rule */
        const css_style_t *src = &rule->style;
        if (src->set_flags & CSS_F_BG_COLOR)     out->background_color = src->background_color;
        if (src->set_flags & CSS_F_COLOR)         out->color = src->color;
        if (src->set_flags & CSS_F_WIDTH)         out->width = src->width;
        if (src->set_flags & CSS_F_HEIGHT)        out->height = src->height;
        if (src->set_flags & CSS_F_FONT_SIZE)     out->font_size = src->font_size;
        if (src->set_flags & CSS_F_FONT_WEIGHT)   out->font_weight = src->font_weight;
        if (src->set_flags & CSS_F_TEXT_ALIGN)    out->text_align = src->text_align;
        if (src->set_flags & CSS_F_DISPLAY)       out->display = src->display;
        if (src->set_flags & CSS_F_MARGIN) {
            out->margin_top    = src->margin_top;
            out->margin_right  = src->margin_right;
            out->margin_bottom = src->margin_bottom;
            out->margin_left   = src->margin_left;
        }
        if (src->set_flags & CSS_F_PADDING) {
            out->padding_top    = src->padding_top;
            out->padding_right  = src->padding_right;
            out->padding_bottom = src->padding_bottom;
            out->padding_left   = src->padding_left;
        }
        if (src->set_flags & CSS_F_BORDER) {
            out->border_width = src->border_width;
            out->border_style = src->border_style;
            out->border_color = src->border_color;
        }
        if (src->set_flags & CSS_F_BORDER_RADIUS) out->border_radius = src->border_radius;
        out->set_flags |= src->set_flags;
    }
}

/* ═══════════════════════════════════════════
 *  css_inherit
 *  Copy inheritable properties from parent to child (only if not set)
 * ═══════════════════════════════════════════ */

void css_inherit(const css_style_t *parent, css_style_t *child) {
    if (!parent || !child) return;
    /* Inheritable: color, font-size, font-weight, text-align */
    if (!(child->set_flags & CSS_F_COLOR) && parent->color != CSS_COLOR_NONE)
        child->color = parent->color;
    if (!(child->set_flags & CSS_F_FONT_SIZE))
        child->font_size = parent->font_size;
    if (!(child->set_flags & CSS_F_FONT_WEIGHT))
        child->font_weight = parent->font_weight;
    if (!(child->set_flags & CSS_F_TEXT_ALIGN))
        child->text_align = parent->text_align;
}
