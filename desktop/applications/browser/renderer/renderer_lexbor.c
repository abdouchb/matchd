/*
 * ZarqaaKit - HTML/CSS Renderer (ZarqaaOS Browser Engine)
 * Replaces the old basic renderer with full CSS box-model support.
 *
 * Features:
 *  - Full CSS parsing via css_parser.c (inline + <style> tag)
 *  - Box model: padding, margin, border, border-radius
 *  - Typography: font-size, font-weight, text-align, color
 *  - Layout: block flow, inline, basic flex (row/column)
 *  - Scrolling, render culling
 *  - HTML elements: h1-h6, p, div, span, a, ul/ol/li, img, input, button, table, hr, br
 */

#include "renderer.h"
#include "css_parser.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"
#include "drivers/framebuffer.h"
#include "gui/renderer.h"
#include "../platform/canvas.h"
#include "filesystem/vfs.h"
#include "resource/resource_manager.h"
#include "net/url.h"
#define STBI_NO_STDIO
#include "../libzui/stb_image.h"

// Lexbor Core
#include "lexbor/html/parser.h"
#include "lexbor/html/interface.h"

/* ═══════════════════════════════════════════
 *  Internal layout node
 * ═══════════════════════════════════════════ */

struct render_node {
    lxb_dom_node_t *dom_node;

    /* Computed CSS */
    css_style_t     style;

    /* Resolved layout position/size */
    int x, y, width, height;

    /* Computed text color (after inheritance) */
    color_t text_color;

    /* Link href */
    char *href;

    /* Image data */
    uint32_t *img_pixels;
    int img_w, img_h;

    /* Tree */
    struct render_node *first_child;
    struct render_node *next_sibling;
};

/* ═══════════════════════════════════════════
 *  Global state
 * ═══════════════════════════════════════════ */

static lxb_html_parser_t   *global_parser = NULL;
static css_stylesheet_t     page_stylesheet;
static uint32_t             current_layout_generation = 0;

typedef struct {
    render_node_t *box;
    uint32_t generation;
} image_load_ctx_t;

/* ═══════════════════════════════════════════
 *  Tag metadata helpers
 * ═══════════════════════════════════════════ */

typedef enum {
    TAG_UNKNOWN,
    TAG_HTML, TAG_HEAD, TAG_BODY,
    TAG_H1, TAG_H2, TAG_H3, TAG_H4, TAG_H5, TAG_H6,
    TAG_P, TAG_DIV, TAG_SPAN,
    TAG_A, TAG_STRONG, TAG_EM, TAG_B, TAG_I, TAG_U,
    TAG_UL, TAG_OL, TAG_LI,
    TAG_TABLE, TAG_TR, TAG_TD, TAG_TH,
    TAG_IMG, TAG_INPUT, TAG_BUTTON, TAG_FORM, TAG_LABEL,
    TAG_HR, TAG_BR, TAG_PRE, TAG_CODE,
    TAG_HEADER, TAG_FOOTER, TAG_MAIN, TAG_SECTION, TAG_ARTICLE,
    TAG_NAV, TAG_ASIDE,
    TAG_STYLE, TAG_SCRIPT, TAG_META, TAG_LINK, TAG_TITLE,
} tag_id_t;

static tag_id_t get_tag_id(const char *name) {
    struct { const char *n; tag_id_t id; } map[] = {
        {"html",TAG_HTML},{"head",TAG_HEAD},{"body",TAG_BODY},
        {"h1",TAG_H1},{"h2",TAG_H2},{"h3",TAG_H3},
        {"h4",TAG_H4},{"h5",TAG_H5},{"h6",TAG_H6},
        {"p",TAG_P},{"div",TAG_DIV},{"span",TAG_SPAN},
        {"a",TAG_A},{"strong",TAG_STRONG},{"em",TAG_EM},
        {"b",TAG_B},{"i",TAG_I},{"u",TAG_U},
        {"ul",TAG_UL},{"ol",TAG_OL},{"li",TAG_LI},
        {"table",TAG_TABLE},{"tr",TAG_TR},{"td",TAG_TD},{"th",TAG_TH},
        {"img",TAG_IMG},{"input",TAG_INPUT},{"button",TAG_BUTTON},
        {"form",TAG_FORM},{"label",TAG_LABEL},
        {"hr",TAG_HR},{"br",TAG_BR},{"pre",TAG_PRE},{"code",TAG_CODE},
        {"header",TAG_HEADER},{"footer",TAG_FOOTER},{"main",TAG_MAIN},
        {"section",TAG_SECTION},{"article",TAG_ARTICLE},
        {"nav",TAG_NAV},{"aside",TAG_ASIDE},
        {"style",TAG_STYLE},{"script",TAG_SCRIPT},{"meta",TAG_META},
        {"link",TAG_LINK},{"title",TAG_TITLE},
        {NULL, TAG_UNKNOWN}
    };
    for (int i = 0; map[i].n; i++)
        if (strcmp(name, map[i].n) == 0) return map[i].id;
    return TAG_UNKNOWN;
}

static bool tag_is_skip(tag_id_t id) {
    return id == TAG_STYLE || id == TAG_SCRIPT || id == TAG_META ||
           id == TAG_LINK  || id == TAG_TITLE  || id == TAG_HEAD;
}

static bool tag_is_block(tag_id_t id) {
    return id == TAG_HTML || id == TAG_BODY   || id == TAG_DIV  ||
           id == TAG_P    || id == TAG_H1     || id == TAG_H2   ||
           id == TAG_H3   || id == TAG_H4     || id == TAG_H5   ||
           id == TAG_H6   || id == TAG_UL     || id == TAG_OL   ||
           id == TAG_LI   || id == TAG_TABLE  || id == TAG_TR   ||
           id == TAG_HEADER || id == TAG_FOOTER || id == TAG_MAIN ||
           id == TAG_SECTION || id == TAG_ARTICLE || id == TAG_NAV ||
           id == TAG_ASIDE || id == TAG_FORM  || id == TAG_PRE  ||
           id == TAG_HR   || id == TAG_BR;
}

/* Default font sizes for headings */
static int heading_font_size(tag_id_t id) {
    switch (id) {
        case TAG_H1: return 28;
        case TAG_H2: return 22;
        case TAG_H3: return 18;
        case TAG_H4: return 16;
        case TAG_H5: return 14;
        case TAG_H6: return 12;
        default: return 0;
    }
}

/* ═══════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════ */

void browser_renderer_init(void) {
    if (!global_parser) {
        global_parser = lxb_html_parser_create();
        lxb_html_parser_init(global_parser);
    }
    memset(&page_stylesheet, 0, sizeof(page_stylesheet));
}

void *browser_renderer_parse_html(const char *html_data, size_t length) {
    if (!global_parser) return NULL;
    /* Reset stylesheet for each new page */
    memset(&page_stylesheet, 0, sizeof(page_stylesheet));
    return lxb_html_parse(global_parser, (const lxb_char_t*)html_data, length);
}

void browser_renderer_destroy_document(void *document) {
    if (document)
        lxb_html_document_destroy((lxb_html_document_t *)document);
}

/* ═══════════════════════════════════════════
 *  Extract <style> tag content before build
 * ═══════════════════════════════════════════ */

/* Load a CSS file from VFS and inject into the page stylesheet */
static void load_css_file(const char *path) {
    if (!path || path[0] == '\0') return;
    /* Only handle local paths (starting with / or relative) */
    const char *load_path = path;
    char local_path[256];
    /* If path is relative (no scheme), treat as local */
    if (strstr(path, "://") == NULL) {
        if (path[0] != '/') {
            /* relative path - try from /hd0/www/ */
            snprintf(local_path, sizeof(local_path), "/hd0/www/%s", path);
            load_path = local_path;
        }
        struct vfs_node *file = vfs_open(load_path);
        if (file && file->size > 0) {
            char *buf = kcalloc(1, file->size + 1);
            int n = vfs_read(file, buf, file->size, 0);
            if (n > 0) {
                kprintf("[Renderer] Loading CSS: %s (%d bytes)\n", load_path, n);
                css_parse_stylesheet(buf, &page_stylesheet);
            }
            kfree(buf);
        } else {
            kprintf("[Renderer] CSS file not found: %s\n", load_path);
        }
    }
}

extern void browser_scheduler_invalidate_paint(void);

static void image_resource_loaded_cb(const char *data, int len, void *arg) {
    image_load_ctx_t *ctx = (image_load_ctx_t *)arg;
    if (!ctx) return;
    
    if (ctx->generation == current_layout_generation && ctx->box && data && len > 0) {
        render_node_t *box = ctx->box;
        int width, height, channels;
        uint8_t *img_data = stbi_load_from_memory((const uint8_t *)data, len, &width, &height, &channels, 4);
        if (img_data) {
            box->img_pixels = kmalloc(width * height * 4);
            box->img_w = width;
            box->img_h = height;
            for (int i = 0; i < width * height; i++) {
                uint8_t r = img_data[i * 4 + 0];
                uint8_t g = img_data[i * 4 + 1];
                uint8_t b = img_data[i * 4 + 2];
                uint8_t a = img_data[i * 4 + 3];
                box->img_pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
            }
            kfree(img_data); 
            
            /* Tell the browser to repaint since the image just arrived */
            browser_scheduler_invalidate_paint();
            extern void browser_trigger_redraw(void);
            browser_trigger_redraw();
        }
    }
    kfree(ctx);
}

static void load_image_file(render_node_t *box, const char *path) {
    if (!path || path[0] == '\0') return;
    
    char full_url[1280];
    if (strstr(path, "://") == NULL) {
        if (path[0] == '/') {
            extern const char *browser_get_current_url(void);
            const char *base = browser_get_current_url();
            parsed_url_t parsed_base;
            if (base && url_parse(base, &parsed_base) && (parsed_base.type == URL_TYPE_HTTP || parsed_base.type == URL_TYPE_HTTPS)) {
                snprintf(full_url, sizeof(full_url), "%s://%s%s", parsed_base.scheme, parsed_base.host, path);
            } else {
                snprintf(full_url, sizeof(full_url), "file://%s", path);
            }
        } else {
            snprintf(full_url, sizeof(full_url), "file:///hd0/www/%s", path);
        }
    } else {
        strncpy(full_url, path, sizeof(full_url) - 1);
        full_url[sizeof(full_url) - 1] = '\0';
    }
    
    parsed_url_t parsed;
    if (url_parse(full_url, &parsed)) {
        image_load_ctx_t *ctx = kmalloc(sizeof(image_load_ctx_t));
        ctx->box = box;
        ctx->generation = current_layout_generation;
        resource_load(&parsed, image_resource_loaded_cb, ctx);
    }
}

static void extract_styles(lxb_dom_node_t *node) {
    if (!node) return;
    if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        size_t len;
        const lxb_char_t *tag = lxb_dom_element_qualified_name(
            lxb_dom_interface_element(node), &len);
        if (tag && strcmp((const char *)tag, "style") == 0) {
            /* Get text content of <style> */
            size_t tlen;
            const lxb_char_t *text = lxb_dom_node_text_content(node, &tlen);
            if (text && tlen > 0) {
                char *buf = kcalloc(1, tlen + 1);
                memcpy(buf, text, tlen);
                css_parse_stylesheet(buf, &page_stylesheet);
                kfree(buf);
            }
            goto recurse;
        }
        /* Handle <link rel="stylesheet" href="..."> */
        if (tag && strcmp((const char *)tag, "link") == 0) {
            size_t attr_len;
            const lxb_char_t *rel = lxb_dom_element_get_attribute(
                lxb_dom_interface_element(node),
                (const lxb_char_t *)"rel", 3, &attr_len);
            if (rel && strncmp((const char *)rel, "stylesheet", 10) == 0) {
                const lxb_char_t *href = lxb_dom_element_get_attribute(
                    lxb_dom_interface_element(node),
                    (const lxb_char_t *)"href", 4, &attr_len);
                if (href && attr_len > 0 && attr_len < 255) {
                    char path_buf[256] = {0};
                    memcpy(path_buf, href, attr_len);
                    load_css_file(path_buf);
                }
            }
            goto recurse;
        }
    }
recurse:;
    lxb_dom_node_t *child = node->first_child;
    while (child) {
        extract_styles(child);
        child = child->next;
    }
}

/* ═══════════════════════════════════════════
 *  Build layout tree
 * ═══════════════════════════════════════════ */

static render_node_t *build_node(lxb_dom_node_t *dom_node,
                                 const css_style_t *parent_style) {
    if (!dom_node) return NULL;

    render_node_t *box = kcalloc(1, sizeof(render_node_t));
    box->dom_node = dom_node;
    css_style_init_default(&box->style);

    if (parent_style)
        css_inherit(parent_style, &box->style);

    /* Text node */
    if (dom_node->type == LXB_DOM_NODE_TYPE_TEXT) {
        box->text_color = box->style.color != CSS_COLOR_NONE
                        ? box->style.color : 0xFF111111;
        return box;
    }

    if (dom_node->type != LXB_DOM_NODE_TYPE_ELEMENT &&
        dom_node->type != LXB_DOM_NODE_TYPE_DOCUMENT) {
        kfree(box);
        return NULL;
    }

    char tag_name[32] = {0};
    tag_id_t tid = TAG_UNKNOWN;

    if (dom_node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        /* Get tag name */
        size_t tlen;
        const lxb_char_t *tag_lxb =
            lxb_dom_element_qualified_name(lxb_dom_interface_element(dom_node), &tlen);
        if (!tag_lxb) { kfree(box); return NULL; }

        int tn = (int)tlen < 31 ? (int)tlen : 31;
        memcpy(tag_name, tag_lxb, tn);
        /* lowercase */
        for (int i = 0; i < tn; i++)
            if (tag_name[i] >= 'A' && tag_name[i] <= 'Z') tag_name[i] += 32;

        tid = get_tag_id(tag_name);

        /* Skip non-visual nodes */
        if (tag_is_skip(tid)) { kfree(box); return NULL; }
    } else {
        /* Document node */
        strcpy(tag_name, "html");
        tid = TAG_HTML;
    }

    /* Set block/inline default by tag */
    if (!(box->style.set_flags & CSS_F_DISPLAY)) {
        if (tag_is_block(tid)) box->style.display = CSS_DISPLAY_BLOCK;
        else box->style.display = CSS_DISPLAY_INLINE;
    }

    /* Get class, id, style attributes (only for elements) */
    char class_buf[128] = {0}, id_buf[64] = {0};
    if (dom_node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        size_t attr_len;
        const lxb_char_t *style_attr = lxb_dom_element_get_attribute(
            lxb_dom_interface_element(dom_node), (const lxb_char_t *)"style", 5, &attr_len);
        const lxb_char_t *class_attr = lxb_dom_element_get_attribute(
            lxb_dom_interface_element(dom_node), (const lxb_char_t *)"class", 5, &attr_len);
        const lxb_char_t *id_attr = lxb_dom_element_get_attribute(
            lxb_dom_interface_element(dom_node), (const lxb_char_t *)"id", 2, &attr_len);

        if (class_attr) strncpy(class_buf, (const char *)class_attr, 127);
        if (id_attr)    strncpy(id_buf,    (const char *)id_attr,    63);

        /* Apply stylesheet rules first (lower priority) */
        css_apply_rules(&page_stylesheet, tag_name, class_buf, id_buf, &box->style);

        /* Apply inline style (higher priority) */
        if (style_attr) {
            char style_buf[512] = {0};
            int sl = (int)strlen((const char *)style_attr);
            if (sl > 511) sl = 511;
            memcpy(style_buf, style_attr, sl);
            css_parse_inline(style_buf, &box->style);
        }

        /* Legacy HTML presentation attributes (bgcolor, text, color) */
        {
            const lxb_char_t *bgcolor = lxb_dom_element_get_attribute(
                lxb_dom_interface_element(dom_node), (const lxb_char_t *)"bgcolor", 7, &attr_len);
            if (bgcolor && attr_len > 0 && !(box->style.set_flags & CSS_F_BG_COLOR)) {
                char bg_buf[32] = {0};
                int bgl = (int)attr_len < 31 ? (int)attr_len : 31;
                memcpy(bg_buf, bgcolor, bgl);
                css_color_t c = css_parse_color(bg_buf);
                if (c != CSS_COLOR_NONE) {
                    box->style.background_color = c;
                    box->style.set_flags |= CSS_F_BG_COLOR;
                }
            }
            const lxb_char_t *txt_color = lxb_dom_element_get_attribute(
                lxb_dom_interface_element(dom_node), (const lxb_char_t *)"text", 4, &attr_len);
            if (txt_color && attr_len > 0 && !(box->style.set_flags & CSS_F_COLOR)) {
                char tc_buf[32] = {0};
                int tcl = (int)attr_len < 31 ? (int)attr_len : 31;
                memcpy(tc_buf, txt_color, tcl);
                css_color_t c = css_parse_color(tc_buf);
                if (c != CSS_COLOR_NONE) {
                    box->style.color = c;
                    box->style.set_flags |= CSS_F_COLOR;
                }
            }
        }
    }

    /* Apply semantic defaults for headings */
    int hfs = heading_font_size(tid);
    if (hfs > 0 && !(box->style.set_flags & CSS_F_FONT_SIZE)) {
        box->style.font_size   = hfs;
        box->style.font_weight = CSS_FONT_WEIGHT_BOLD;
    }
    if ((tid == TAG_H1 || tid == TAG_H2 || tid == TAG_H3 ||
         tid == TAG_H4 || tid == TAG_H5 || tid == TAG_H6) &&
        !(box->style.set_flags & CSS_F_MARGIN)) {
        box->style.margin_top    = 8;
        box->style.margin_bottom = 8;
    }
    if (tid == TAG_P && !(box->style.set_flags & CSS_F_MARGIN)) {
        box->style.margin_top = 6; box->style.margin_bottom = 6;
    }
    if (tid == TAG_LI && !(box->style.set_flags & CSS_F_MARGIN)) {
        box->style.margin_left  = 20;
        box->style.margin_top   = 2;
        box->style.margin_bottom = 2;
    }
    if (tid == TAG_A  && !(box->style.set_flags & CSS_F_COLOR))
        box->style.color = 0xFF1565C0;
    if (tid == TAG_BODY && !(box->style.set_flags & CSS_F_BG_COLOR))
        box->style.background_color = 0xFFFFFFFF;
    if (tid == TAG_BODY && !(box->style.set_flags & CSS_F_COLOR))
        box->style.color = 0xFF111111;

    /* Resolve text color */
    box->text_color = box->style.color != CSS_COLOR_NONE
                    ? box->style.color : 0xFF111111;

    /* href / src */
    if (dom_node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        size_t attr_len;
        const lxb_char_t *href_val = lxb_dom_element_get_attribute(
            lxb_dom_interface_element(dom_node), (const lxb_char_t *)"href", 4, &attr_len);
        if (!href_val || attr_len == 0)
            href_val = lxb_dom_element_get_attribute(
                lxb_dom_interface_element(dom_node), (const lxb_char_t *)"src", 3, &attr_len);
        if (href_val && attr_len > 0) {
            box->href = kcalloc(1, attr_len + 1);
            memcpy(box->href, href_val, attr_len);
        }
        
        /* Fetch image data if this is an img tag */
        if (tid == TAG_IMG && box->href) {
            load_image_file(box, box->href);
        }
    }

    /* Recurse into children */
    render_node_t *prev = NULL;
    lxb_dom_node_t *child = dom_node->first_child;
    while (child) {
        if (child->type == LXB_DOM_NODE_TYPE_ELEMENT ||
            child->type == LXB_DOM_NODE_TYPE_TEXT) {
            render_node_t *cbox = build_node(child, &box->style);
            if (cbox) {
                if (!box->first_child) box->first_child = cbox;
                else prev->next_sibling = cbox;
                prev = cbox;
            }
        }
        child = child->next;
    }
    return box;
}

render_node_t *browser_renderer_build_layout(void *document) {
    current_layout_generation++;
    lxb_html_document_t *doc = (lxb_html_document_t *)document;
    /* Extract stylesheet first */
    extract_styles(lxb_dom_interface_node(doc));
    return build_node(lxb_dom_interface_node(doc), NULL);
}

/* ═══════════════════════════════════════════
 *  Layout computation (Box Model)
 * ═══════════════════════════════════════════ */

static void compute_node(render_node_t *box, int px, int py, int max_w);

static int measure_text(const char *text, int max_w, int font_size) {
    /* Approximate: 8px per char, 16px line height scaled by font_size/14 */
    (void)font_size;
    if (!text || !text[0]) return 0;
    int len = strlen(text);
    int chars_per_line = max_w / 8;
    if (chars_per_line < 1) chars_per_line = 1;
    int lines = (len + chars_per_line - 1) / chars_per_line;
    return lines * 18;
}

static void shift_children(render_node_t *first_child, render_node_t *end_child, int dx, int dy) {
    render_node_t *c = first_child;
    while (c && c != end_child) {
        c->x += dx;
        c->y += dy;
        /* we should also shift their children recursively if they are just repositioned */
        if (c->first_child) shift_children(c->first_child, NULL, dx, dy);
        c = c->next_sibling;
    }
}

static void compute_node(render_node_t *box, int px, int py, int max_w) {
    if (!box) return;
    if (box->style.display == CSS_DISPLAY_NONE) {
        box->x = px; box->y = py; box->width = 0; box->height = 0;
        return;
    }

    const css_style_t *s = &box->style;
    bool is_block = (s->display == CSS_DISPLAY_BLOCK) || 
                    (box->dom_node->type == LXB_DOM_NODE_TYPE_DOCUMENT);

    /* Apply margins */
    int cx = px + s->margin_left;
    int cy = py + s->margin_top;
    int avail_w = max_w - s->margin_left - s->margin_right;
    if (avail_w < 10) avail_w = 10;

    /* Override with explicit width */
    if (s->width >= 0) avail_w = s->width;

    box->x = cx;
    box->y = cy;
    box->width = (is_block || (s->display == CSS_DISPLAY_INLINE_BLOCK && s->width >= 0)) ? avail_w : 0;
    box->height = 0;

    /* Text node */
    if (box->dom_node->type == LXB_DOM_NODE_TYPE_TEXT) {
        size_t len;
        const lxb_char_t *text = lxb_dom_node_text_content(box->dom_node, &len);
        if (text && len > 0) {
            char *tmp = kcalloc(1, len + 1);
            memcpy(tmp, text, len);
            /* collapse whitespace */
            bool has_real = false;
            for (size_t i = 0; i < len; i++) {
                if (tmp[i] == '\n' || tmp[i] == '\r' || tmp[i] == '\t') tmp[i] = ' ';
                if (tmp[i] > 32) has_real = true;
            }
            if (has_real) {
                int txt_len = strlen(tmp);
                int txt_w = txt_len * 8;
                if (txt_w > avail_w) txt_w = avail_w;
                box->width = txt_w;
                box->height = measure_text(tmp, avail_w, s->font_size);
            }
            kfree(tmp);
        }
        return;
    }

    /* Padding inner area */
    int inner_x = cx + s->padding_left + s->border_width;
    int inner_y = cy + s->padding_top  + s->border_width;
    int inner_w = avail_w - s->padding_left - s->padding_right - s->border_width * 2;
    if (inner_w < 10) inner_w = 10;

    /* Explicit height */
    int explicit_h = (s->height >= 0) ? s->height : -1;

    /* Special fixed-size elements */
    if (box->dom_node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        size_t tlen;
        const lxb_char_t *tn = lxb_dom_element_qualified_name(
            lxb_dom_interface_element(box->dom_node), &tlen);
        if (tn) {
            char tag[32] = {0};
            int n = (int)tlen < 31 ? (int)tlen : 31;
            memcpy(tag, tn, n);
            tag_id_t tid = get_tag_id(tag);
            if (tid == TAG_HR) { box->height = 2 + s->margin_top + s->margin_bottom; return; }
            if (tid == TAG_BR) { box->height = s->font_size + 4; box->width = 0; return; }
            if (tid == TAG_IMG) {
                box->width  = s->width  >= 0 ? s->width  : 200;
                box->height = s->height >= 0 ? s->height : 150;
                return;
            }
            if (tid == TAG_INPUT) {
                box->width  = s->width  >= 0 ? s->width  : 180;
                box->height = s->height >= 0 ? s->height : 28;
                return;
            }
            if (tid == TAG_BUTTON) {
                box->width  = s->width  >= 0 ? s->width  : 100;
                box->height = s->height >= 0 ? s->height : 32;
                return;
            }
        }
    }

    /* Layout children */
    bool is_flex = (s->display == CSS_DISPLAY_FLEX);
    int child_x = inner_x;
    int child_y = inner_y;
    int line_h  = 0;
    int total_h = 0;

    render_node_t *child = box->first_child;
    while (child) {
        if (child->style.display == CSS_DISPLAY_NONE) {
            child = child->next_sibling;
            continue;
        }

        bool child_block = (child->style.display == CSS_DISPLAY_BLOCK ||
                            child->dom_node->type == LXB_DOM_NODE_TYPE_DOCUMENT);

        /* For block children or flex-column, always go to new line */
        bool force_newline = child_block || (is_flex && s->flex_direction == 1);

        if (force_newline && child_x > inner_x) {
            child_y += line_h + s->gap;
            child_x  = inner_x;
            line_h   = 0;
        }

        compute_node(child, child_x, child_y, inner_w);

        if (force_newline || (is_flex && s->flex_direction == 1)) {
            child_y += child->height + child->style.margin_bottom + s->gap;
            child_x  = inner_x;
            line_h   = 0;
        } else {
            /* Inline / flex-row */
            child_x += child->width + child->style.margin_right;
            if (child->height > line_h) line_h = child->height;
            if (child_x >= inner_x + inner_w) {
                child_y += line_h + s->gap;
                child_x  = inner_x;
                line_h   = 0;
            }
        }
        
        if (!is_block && s->width < 0) {
            int current_row_w = child_x - inner_x;
            if (current_row_w > box->width) box->width = current_row_w;
        }

        child = child->next_sibling;
    }
    /* Flush last inline line */
    if (child_x > inner_x) child_y += line_h;

    total_h = child_y - inner_y + s->padding_bottom + s->border_width;
    if (explicit_h >= 0) total_h = explicit_h;
    if (total_h < s->padding_top + s->padding_bottom) total_h = s->padding_top + s->padding_bottom;

    box->height = total_h + s->margin_bottom;

    /* Post-pass: Flex alignment (single-line simplified) */
    if (is_flex) {
        int avail_w_inner = inner_w;
        int avail_h_inner = (explicit_h >= 0) ? (explicit_h - s->padding_top - s->padding_bottom) : (total_h - s->padding_top - s->padding_bottom);
        if (avail_h_inner < 0) avail_h_inner = 0;

        if (s->flex_direction == 0) { /* Row */
            /* Compute total width of children (assuming no wrap for simple flex) */
            int total_children_w = 0;
            int max_child_h = 0;
            render_node_t *c = box->first_child;
            while (c) {
                if (c->style.display != CSS_DISPLAY_NONE) {
                    total_children_w += c->width + c->style.margin_left + c->style.margin_right + s->gap;
                    if (c->height > max_child_h) max_child_h = c->height;
                }
                c = c->next_sibling;
            }
            if (total_children_w > 0) total_children_w -= s->gap;

            int offset_x = 0;
            if (s->justify_content == CSS_FLEX_CENTER) {
                offset_x = (avail_w_inner - total_children_w) / 2;
            } else if (s->justify_content == CSS_FLEX_END) {
                offset_x = avail_w_inner - total_children_w;
            }

            if (offset_x > 0 || s->align_items == CSS_FLEX_CENTER) {
                c = box->first_child;
                int current_x = inner_x + offset_x;
                while (c) {
                    if (c->style.display != CSS_DISPLAY_NONE) {
                        int dx = current_x - c->x;
                        int dy = 0;
                        if (s->align_items == CSS_FLEX_CENTER) {
                            dy = (max_child_h - c->height) / 2;
                        }
                        if (dx != 0 || dy != 0) {
                            c->x += dx;
                            c->y += dy;
                            if (c->first_child) shift_children(c->first_child, NULL, dx, dy);
                        }
                        current_x += c->width + c->style.margin_left + c->style.margin_right + s->gap;
                    }
                    c = c->next_sibling;
                }
            }
        } else { /* Column */
            int total_children_h = 0;
            render_node_t *c = box->first_child;
            while (c) {
                if (c->style.display != CSS_DISPLAY_NONE) {
                    total_children_h += c->height + c->style.margin_top + c->style.margin_bottom + s->gap;
                }
                c = c->next_sibling;
            }
            if (total_children_h > 0) total_children_h -= s->gap;

            int offset_y = 0;
            if (s->justify_content == CSS_FLEX_CENTER) {
                offset_y = (avail_h_inner - total_children_h) / 2;
            } else if (s->justify_content == CSS_FLEX_END) {
                offset_y = avail_h_inner - total_children_h;
            }
            
            if (offset_y > 0 || s->align_items == CSS_FLEX_CENTER) {
                c = box->first_child;
                int current_y = inner_y + offset_y;
                while (c) {
                    if (c->style.display != CSS_DISPLAY_NONE) {
                        int dy = current_y - c->y;
                        int dx = 0;
                        if (s->align_items == CSS_FLEX_CENTER) {
                            dx = (avail_w_inner - c->width) / 2;
                        }
                        if (dx != 0 || dy != 0) {
                            c->x += dx;
                            c->y += dy;
                            if (c->first_child) shift_children(c->first_child, NULL, dx, dy);
                        }
                        current_y += c->height + c->style.margin_top + c->style.margin_bottom + s->gap;
                    }
                    c = c->next_sibling;
                }
            }
        }
    }
}

void browser_renderer_compute_layout(render_node_t *root, int x, int y, int width) {
    compute_node(root, x, y, width);
}

/* ═══════════════════════════════════════════
 *  Canvas helpers
 * ═══════════════════════════════════════════ */

static void draw_border(canvas_t *c, int x, int y, int w, int h,
                        const css_style_t *s) {
    if (!s || s->border_style == CSS_BORDER_NONE || s->border_width == 0) return;
    color_t col = (s->border_color != CSS_COLOR_NONE) ? s->border_color : 0xFF000000;
    for (int i = 0; i < s->border_width; i++)
        canvas_draw_rect(c, x + i, y + i, w - i*2, h - i*2, col);
}

/* ═══════════════════════════════════════════
 *  Paint pass
 * ═══════════════════════════════════════════ */

void browser_renderer_paint(surface_t *surf, render_node_t *box,
                             int scroll_y, int clip_y, int clip_h) {
    if (!box) return;
    if (box->style.display == CSS_DISPLAY_NONE) return;
    if (!box->style.visible) return;

    int abs_y = box->y - scroll_y;

    /* Culling */
    if (abs_y + box->height < clip_y || abs_y > clip_y + clip_h) goto recurse;

    {
        canvas_t tmp_canvas;
        tmp_canvas.target_surface = surf;
        tmp_canvas.offset_x = 0;
        tmp_canvas.offset_y = 0;
        tmp_canvas.has_clip = false;
        canvas_t *canvas = &tmp_canvas;

        const css_style_t *s = &box->style;

        /* Determine if this is the body/html element — fill full viewport */
        bool is_body_or_html = false;
        if (box->dom_node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            size_t tnl;
            const lxb_char_t *tn2 = lxb_dom_element_qualified_name(
                lxb_dom_interface_element(box->dom_node), &tnl);
            if (tn2) {
                char t2[16] = {0};
                int n2 = (int)tnl < 15 ? (int)tnl : 15;
                memcpy(t2, tn2, n2);
                /* MUST lowercase — get_tag_id is case-sensitive */
                for (int ki = 0; ki < n2; ki++)
                    if (t2[ki] >= 'A' && t2[ki] <= 'Z') t2[ki] += 32;
                tag_id_t t2id = get_tag_id(t2);
                is_body_or_html = (t2id == TAG_BODY || t2id == TAG_HTML);
            }
        } else if (box->dom_node->type == LXB_DOM_NODE_TYPE_DOCUMENT) {
            is_body_or_html = true;
        }

        /* For body/html with no background set: default to white so the
           browser theme dark color doesn't show through */
        css_color_t paint_bg = s->background_color;
        if (is_body_or_html &&
            (paint_bg == CSS_COLOR_TRANSPARENT || paint_bg == 0x00000000)) {
            paint_bg = 0xFFFFFFFF; /* white */
        }

        /* Background */
        if (paint_bg != CSS_COLOR_TRANSPARENT && paint_bg != 0x00000000) {
            int bg_x = box->x, bg_y = abs_y;
            int bg_w = box->width, bg_h = box->height;
            /* For body/html: fill entire visible content area */
            if (is_body_or_html) {
                bg_x = 0;
                bg_y = clip_y;
                bg_w = (int)surf->width;
                bg_h = clip_h;
            }
            if (s->border_radius > 0)
                canvas_fill_rounded_rect(canvas, bg_x, bg_y, bg_w,
                                         bg_h, s->border_radius, paint_bg);
            else
                canvas_fill_rect(canvas, bg_x, bg_y, bg_w, bg_h, paint_bg);
        }

        /* Border */
        draw_border(canvas, box->x, abs_y, box->width, box->height, s);

        /* Element-specific drawing */
        if (box->dom_node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            size_t tlen;
            const lxb_char_t *tn = lxb_dom_element_qualified_name(
                lxb_dom_interface_element(box->dom_node), &tlen);
            if (tn) {
                char tag[32] = {0};
                int n = (int)tlen < 31 ? (int)tlen : 31;
                memcpy(tag, tn, n);
                tag_id_t tid = get_tag_id(tag);

                switch (tid) {
                case TAG_HR:
                    canvas_fill_rect(canvas, box->x, abs_y + box->height / 2,
                                     box->width, 1,
                                     s->border_color != CSS_COLOR_NONE
                                     ? s->border_color : 0xFFCCCCCC);
                    break;
                case TAG_IMG: {
                    if (box->img_pixels) {
                        /* Scale image to box dimensions */
                        /* (Currently `canvas_draw_image_rounded` blits 1:1, so we just draw it)
                           For a complete engine, we'd add scaling. */
                        if (s->border_radius > 0) {
                            canvas_draw_image_rounded(canvas, box->x, abs_y, box->img_pixels, box->width, box->height, s->border_radius);
                        } else {
                            canvas_draw_image(canvas, box->x, abs_y, box->img_pixels, box->width, box->height);
                        }
                    } else {
                        /* Placeholder with camera icon */
                        color_t bg = s->background_color != CSS_COLOR_TRANSPARENT
                                   ? s->background_color : 0xFF2A2A2A;
                        canvas_fill_rounded_rect(canvas, box->x, abs_y, box->width,
                                                 box->height, 4, bg);
                        canvas_draw_rect(canvas, box->x, abs_y, box->width,
                                         box->height, 0xFF555555);
                        int tx = box->x + box->width  / 2 - 24;
                        int ty = abs_y  + box->height / 2 - 8;
                        canvas_draw_text(canvas, tx, ty, "[ Image ]", 0xFFAAAAAA);
                    }
                    break;
                }
                case TAG_INPUT: {
                    color_t bg = s->background_color != CSS_COLOR_TRANSPARENT
                               ? s->background_color : 0xFFFFFFFF;
                    canvas_fill_rounded_rect(canvas, box->x, abs_y, box->width,
                                             box->height, 4, bg);
                    canvas_draw_rect(canvas, box->x, abs_y, box->width,
                                     box->height, 0xFFAAAAAA);
                    break;
                }
                case TAG_BUTTON: {
                    color_t bg = s->background_color != CSS_COLOR_TRANSPARENT
                               ? s->background_color : 0xFF4C8EF5;
                    canvas_fill_rounded_rect(canvas, box->x, abs_y, box->width,
                                             box->height, 6, bg);
                    color_t fg = s->color != CSS_COLOR_NONE ? s->color : 0xFFFFFFFF;
                    /* Get button text from children */
                    char btn_text[64] = "Button";
                    render_node_t *tc = box->first_child;
                    while (tc) {
                        if (tc->dom_node->type == LXB_DOM_NODE_TYPE_TEXT) {
                            size_t l;
                            const lxb_char_t *t = lxb_dom_node_text_content(tc->dom_node, &l);
                            if (t && l > 0) {
                                int cp = l < 63 ? l : 63;
                                memcpy(btn_text, t, cp);
                                btn_text[cp] = '\0';
                            }
                            break;
                        }
                        tc = tc->next_sibling;
                    }
                    int tx = box->x + (box->width - strlen(btn_text) * 8) / 2;
                    int ty = abs_y  + (box->height - 14) / 2;
                    canvas_draw_text(canvas, tx, ty, btn_text, fg);
                    goto recurse; /* skip children - already drew text */
                }
                case TAG_LI: {
                    /* Bullet point */
                    int bx = box->x - 14;
                    int by = abs_y + (box->height / 2) - 3;
                    canvas_fill_rect(canvas, bx, by, 6, 6,
                                     s->color != CSS_COLOR_NONE ? s->color : 0xFF333333);
                    break;
                }
                default: break;
                }
            }
        }

        /* Text nodes */
        if (box->dom_node->type == LXB_DOM_NODE_TYPE_TEXT) {
            size_t len;
            const lxb_char_t *text = lxb_dom_node_text_content(box->dom_node, &len);
            if (text && len > 0 && box->height > 0) {
                char *tmp = kcalloc(1, len + 1);
                memcpy(tmp, text, len);
                for (size_t i = 0; i < len; i++)
                    if (tmp[i] == '\n' || tmp[i] == '\r' || tmp[i] == '\t')
                        tmp[i] = ' ';
                color_t col = box->text_color;
                renderer_execute_draw_text_wrapped(surf, box->x, abs_y,
                                                   box->width, tmp, col, 0, true);
                kfree(tmp);
            }
        }
    }

recurse:;
    render_node_t *child = box->first_child;
    while (child) {
        browser_renderer_paint(surf, child, scroll_y, clip_y, clip_h);
        child = child->next_sibling;
    }
}

/* ═══════════════════════════════════════════
 *  Utilities
 * ═══════════════════════════════════════════ */

void browser_renderer_free_layout(render_node_t *box) {
    if (!box) return;
    render_node_t *child = box->first_child;
    while (child) {
        render_node_t *next = child->next_sibling;
        browser_renderer_free_layout(child);
        child = next;
    }
    if (box->href) kfree(box->href);
    if (box->img_pixels) kfree(box->img_pixels);
    kfree(box);
}

int browser_renderer_get_document_height(render_node_t *root) {
    return root ? root->height : 0;
}

render_node_t *browser_renderer_get_node_at(render_node_t *root, int x, int y) {
    if (!root) return NULL;
    render_node_t *child = root->first_child;
    while (child) {
        render_node_t *hit = browser_renderer_get_node_at(child, x, y);
        if (hit) return hit;
        child = child->next_sibling;
    }
    if (x >= root->x && x <= root->x + root->width &&
        y >= root->y && y <= root->y + root->height)
        return root;
    return NULL;
}

const char *browser_renderer_get_href(render_node_t *node) {
    return node ? node->href : NULL;
}

void browser_renderer_extract_links(render_node_t *node, char *buffer, int max_len) {
    if (!node || !buffer || max_len <= 0) return;
    if (node->href) {
        char li[512];
        strcpy(li, "<li><a href=\"");
        strncat(li, node->href, 200);
        strcat(li, "\">");
        strncat(li, node->href, 200);
        strcat(li, "</a></li>");
        if (strlen(buffer) + strlen(li) < (size_t)max_len)
            strcat(buffer, li);
    }
    browser_renderer_extract_links(node->first_child, buffer, max_len);
    browser_renderer_extract_links(node->next_sibling, buffer, max_len);
}
