#include "RenderEngine.h"
#include "LayoutBox.h"
#include "LayoutBlock.h"
#include "LayoutInline.h"
#include "LayoutText.h"
#include "LayoutFlex.h"

extern "C" {
    void kprintf(const char* fmt, ...);
    #include "zarqaa/string.h"
    #include "net/url.h"
    #include "resource/resource_manager.h"
}

#include "Theme.h"

RenderEngine::RenderEngine() {
    m_html_parser = nullptr;
    m_document = nullptr;
    m_css_parser = nullptr;
    m_css_memory = nullptr;
    m_stylesheet = nullptr;
    m_layoutRoot = nullptr;
}

RenderEngine::~RenderEngine() {
    clear();
    if (m_html_parser) {
        lxb_html_parser_destroy(m_html_parser);
        m_html_parser = nullptr;
    }
}

bool RenderEngine::init() {
    kprintf("[RenderEngine] Initializing Lexbor HTML & CSS Engine...\n");
    m_html_parser = lxb_html_parser_create();
    lxb_html_parser_init(m_html_parser);
    return true;
}

void RenderEngine::clear() {
    if (m_layoutRoot) {
        delete m_layoutRoot;
        m_layoutRoot = nullptr;
    }
    if (m_document) {
        lxb_html_document_destroy(m_document);
        m_document = nullptr;
    }
    if (m_stylesheet) {
        lxb_css_stylesheet_destroy(m_stylesheet, true);
        m_stylesheet = nullptr;
    }
    if (m_html_parser) {
        lxb_html_parser_clean(m_html_parser);
    }
}

bool RenderEngine::parseHTML(const char* html_data, size_t length) {
    clear();
    
    kprintf("[RenderEngine] Parsing HTML document (%d bytes)...\n", length);
    if (!m_html_parser) {
        kprintf("[RenderEngine] Error: HTML parser not initialized!\n");
        return false;
    }
    
    m_document = lxb_html_parse(m_html_parser, (const lxb_char_t*)html_data, length);
    if (m_document == nullptr) {
        kprintf("[RenderEngine] HTML parse failed (m_document is null)\n");
        return false;
    }
    
    kprintf("[RenderEngine] HTML parsed successfully.\n");
    return true;
}

static void onSubresourceLoaded(const char* data, int len, void* arg) {
    (void)data;
    (void)arg;
    if (len > 0) {
        kprintf("[RenderEngine] Subresource loaded: %d bytes\n", len);
        // Here we could parse CSS or decode images based on node type
    }
}

static void traverseAndFetch(lxb_dom_node_t *node, const char* base_url) {
    if (!node) return;
    
    if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        size_t len;
        const lxb_char_t *tag = lxb_dom_element_qualified_name(lxb_dom_interface_element(node), &len);
        if (tag) {
            if (strcmp((char*)tag, "img") == 0) {
                size_t slen;
                const lxb_char_t *src = lxb_dom_element_get_attribute(lxb_dom_interface_element(node), (const lxb_char_t*)"src", 3, &slen);
                if (src && slen > 0) {
                    char resolved[512];
                    if (url_resolve(base_url, (char*)src, resolved, sizeof(resolved))) {
                        parsed_url_t purl;
                        if (url_parse(resolved, &purl)) {
                            kprintf("[RenderEngine] Fetching subresource IMG: %s\n", resolved);
                            resource_load(&purl, onSubresourceLoaded, node);
                        }
                    }
                }
            } else if (strcmp((char*)tag, "link") == 0) {
                size_t rlen;
                const lxb_char_t *rel = lxb_dom_element_get_attribute(lxb_dom_interface_element(node), (const lxb_char_t*)"rel", 3, &rlen);
                if (rel && strcmp((char*)rel, "stylesheet") == 0) {
                    size_t slen;
                    const lxb_char_t *href = lxb_dom_element_get_attribute(lxb_dom_interface_element(node), (const lxb_char_t*)"href", 4, &slen);
                    if (href && slen > 0) {
                        char resolved[512];
                        if (url_resolve(base_url, (char*)href, resolved, sizeof(resolved))) {
                            parsed_url_t purl;
                            if (url_parse(resolved, &purl)) {
                                kprintf("[RenderEngine] Fetching subresource CSS: %s\n", resolved);
                                resource_load(&purl, onSubresourceLoaded, node);
                            }
                        }
                    }
                }
            }
        }
    }
    
    lxb_dom_node_t *child = lxb_dom_node_first_child(node);
    while (child) {
        traverseAndFetch(child, base_url);
        child = lxb_dom_node_next(child);
    }
}

void RenderEngine::fetchSubresources(const char* base_url) {
    if (!m_document) return;
    lxb_dom_node_t *body = lxb_dom_interface_node(m_document->body);
    if (!body) {
        lxb_html_body_element_t *body_el = lxb_html_document_body_element(m_document);
        if (body_el) body = lxb_dom_interface_node(body_el);
    }
    
    if (body) {
        traverseAndFetch(body, base_url);
    }
}

// Helper: check if a tag name should be completely skipped (invisible content)
static bool isInvisibleTag(const char* tag) {
    static const char* const tags[] = {
        "area", "audio", "canvas", "datalist", "dialog", "embed", "head", 
        "iframe", "link", "map", "meta", "noscript", "object", "picture", 
        "portal", "script", "source", "style", "svg", "template", "track", "video"
    };
    int l = 0, r = (sizeof(tags)/sizeof(tags[0])) - 1;
    while (l <= r) {
        int m = l + (r - l) / 2;
        int cmp = strcmp(tag, tags[m]);
        if (cmp == 0) return true;
        if (cmp < 0) r = m - 1;
        else l = m + 1;
    }
    return false;
}

// Helper: check if a tag is inline-level
static bool isInlineTag(const char* tag) {
    static const char* const tags[] = {
        "a", "abbr", "b", "cite", "code", "em", "font", "i", "kbd", "label", 
        "mark", "q", "s", "small", "span", "strong", "sub", "sup", "time", 
        "tt", "u", "var", "wbr"
    };
    int l = 0, r = (sizeof(tags)/sizeof(tags[0])) - 1;
    while (l <= r) {
        int m = l + (r - l) / 2;
        int cmp = strcmp(tag, tags[m]);
        if (cmp == 0) return true;
        if (cmp < 0) r = m - 1;
        else l = m + 1;
    }
    return false;
}

// Helper: parse a simple hex color like #RRGGBB from a CSS value string
static uint32_t parseHexColor(const char* p) {
    if (*p != '#') return 0;
    p++;
    uint32_t val = 0;
    for (int i = 0; i < 6; i++) {
        val <<= 4;
        char c = *p++;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        else return 0; // Invalid
    }
    return 0xFF000000 | val; // Add alpha
}

// Helper: extract px value after a CSS property like "width:123px"
static int parsePxValue(const char* s) {
    int val = 0;
    // skip whitespace
    while (*s == ' ' || *s == ':') s++;
    bool negative = false;
    if (*s == '-') { negative = true; s++; }
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return negative ? -val : val;
}

// Normalize text: collapse whitespace, trim leading/trailing
static char* normalizeText(const char* text, size_t len) {
    // Allocate max possible size
    extern void* kmalloc(size_t);
    char* out = (char*)kmalloc(len + 1);
    if (!out) return nullptr;
    
    int j = 0;
    bool last_was_space = true; // Start true to trim leading whitespace
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            if (!last_was_space) {
                out[j++] = ' ';
                last_was_space = true;
            }
        } else {
            out[j++] = c;
            last_was_space = false;
        }
    }
    // Trim trailing space
    if (j > 0 && out[j-1] == ' ') j--;
    out[j] = '\0';
    return out;
}

#define MAX_TREE_DEPTH 40
#define MAX_TOTAL_NODES 1200
#define MAX_CHILDREN_PER_NODE 100

static int g_total_nodes = 0;

static LayoutBox* buildLayoutTree(lxb_dom_node_t* node, int depth = 0) {
    if (!node) return nullptr;
    if (depth > MAX_TREE_DEPTH) return nullptr;
    if (g_total_nodes >= MAX_TOTAL_NODES) return nullptr;
    
    LayoutBox* box = nullptr;
    
    if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        size_t len;
        const lxb_char_t *tag = lxb_dom_element_qualified_name(lxb_dom_interface_element(node), &len);
        const char* tag_str = (const char*)tag;
        
        // 1. Skip invisible tags entirely
        if (isInvisibleTag(tag_str)) {
            return nullptr;
        }
        
        // 2. Check for hidden attribute
        lxb_dom_element_t *element = lxb_dom_interface_element(node);
        const lxb_char_t *hidden_val = lxb_dom_element_get_attribute(element, (const lxb_char_t*)"hidden", 6, &len);
        if (hidden_val) return nullptr;
        
        const lxb_char_t *aria_hidden = lxb_dom_element_get_attribute(element, (const lxb_char_t*)"aria-hidden", 11, &len);
        if (aria_hidden && strcmp((const char*)aria_hidden, "true") == 0) return nullptr;
        
        // 3. Check for style attribute - display:none
        const lxb_char_t *style_val = lxb_dom_element_get_attribute(element, (const lxb_char_t*)"style", 5, &len);
        if (style_val) {
            const char* sv = (const char*)style_val;
            if (strstr(sv, "display:none") || strstr(sv, "display: none") ||
                strstr(sv, "visibility:hidden") || strstr(sv, "visibility: hidden")) {
                return nullptr;
            }
        }
        
        // 4. Determine display type from tag
        DisplayType display = DisplayType::Block;
        bool is_img_tag = false;
        bool is_a_tag = false;
        bool is_bold = false;
        bool is_italic = false;
        bool is_underline = false;
        bool is_centered = false;
        bool is_line_break = false; (void)is_line_break;
        bool is_hr = false; (void)is_hr;
        int tag_margin_top = 0;
        int tag_margin_bottom = 0;
        int tag_padding = 0;
        uint32_t tag_color = 0; // 0 = inherit
        uint32_t tag_bg = 0;
        uint32_t tag_border_color = 0;
        int tag_font_size = 0;
        
        // Inline tags
        if (isInlineTag(tag_str)) {
            display = DisplayType::Inline;
            if (strcmp(tag_str, "a") == 0) {
                is_a_tag = true;
                tag_color = QP_CYAN; // Cyan links instead of standard blue
                is_underline = true;
            }
            if (strcmp(tag_str, "b") == 0 || strcmp(tag_str, "strong") == 0) {
                is_bold = true;
            }
            if (strcmp(tag_str, "em") == 0 || strcmp(tag_str, "i") == 0) {
                is_italic = true;
            }
            if (strcmp(tag_str, "u") == 0) {
                is_underline = true;
            }
            if (strcmp(tag_str, "code") == 0 || strcmp(tag_str, "kbd") == 0 || strcmp(tag_str, "tt") == 0) {
                tag_bg = QP_MNT_LIGHT; // Dark theme code bg
                tag_color = 0xFFff7b72; // Reddish code text
            }
            if (strcmp(tag_str, "small") == 0) {
                tag_font_size = 12;
            }
        }
        // Self-closing / special tags
        else if (strcmp(tag_str, "br") == 0) {
            box = new LayoutBlock(node);
            if (!box) return nullptr;
            box->style.display = DisplayType::Block;
            box->style.is_line_break = true;
            box->style.height = 16;
            g_total_nodes++;
            return box;
        }
        else if (strcmp(tag_str, "hr") == 0) {
            box = new LayoutBlock(node);
            if (!box) return nullptr;
            box->style.display = DisplayType::Block;
            box->style.is_horizontal_rule = true;
            box->style.height = 1;
            box->style.margin_top = 8;
            box->style.margin_bottom = 8;
            box->style.bg_color = 0xFFCCCCCC;
            g_total_nodes++;
            return box;
        }
        // Input elements
        else if (strcmp(tag_str, "button") == 0) {
            display = DisplayType::InlineBlock;
            tag_bg = QP_ACTIVE_HI;
            tag_color = QP_TEXT_MAIN;
            tag_padding = 8;
        }
        else if (strcmp(tag_str, "input") == 0) {
            display = DisplayType::InlineBlock;
            tag_bg = QP_MNT_LIGHT;
            tag_color = QP_TEXT_MAIN;
            tag_border_color = QP_BORDER;
            tag_padding = 4;
        }
        else if (strcmp(tag_str, "select") == 0 || strcmp(tag_str, "option") == 0 || strcmp(tag_str, "textarea") == 0) {
            return nullptr; // Skip form elements for now
        }
        else if (strcmp(tag_str, "img") == 0) {
            display = DisplayType::InlineBlock;
            is_img_tag = true;
        }
        // Heading tags
        else if (strcmp(tag_str, "h1") == 0) {
            tag_font_size = 32; tag_margin_top = 16; tag_margin_bottom = 10; is_bold = true;
        }
        else if (strcmp(tag_str, "h2") == 0) {
            tag_font_size = 24; tag_margin_top = 14; tag_margin_bottom = 8; is_bold = true;
        }
        else if (strcmp(tag_str, "h3") == 0) {
            tag_font_size = 20; tag_margin_top = 12; tag_margin_bottom = 6; is_bold = true;
        }
        else if (strcmp(tag_str, "h4") == 0) {
            tag_font_size = 16; tag_margin_top = 10; tag_margin_bottom = 4; is_bold = true;
        }
        else if (strcmp(tag_str, "h5") == 0) {
            tag_font_size = 14; tag_margin_top = 8; tag_margin_bottom = 4; is_bold = true;
        }
        else if (strcmp(tag_str, "h6") == 0) {
            tag_font_size = 12; tag_margin_top = 6; tag_margin_bottom = 4; is_bold = true;
        }
        // Paragraph
        else if (strcmp(tag_str, "p") == 0) {
            tag_margin_top = 8; tag_margin_bottom = 8;
            tag_color = QP_TEXT_MUTED; // Slightly muted text for readability
        }
        else if (strcmp(tag_str, "body") == 0 || strcmp(tag_str, "html") == 0) {
            tag_bg = QP_BG_DARK; // Force dark background
            tag_color = QP_TEXT_MAIN;
        }
        // Center tag
        else if (strcmp(tag_str, "center") == 0) {
            is_centered = true;
        }
        // List items
        else if (strcmp(tag_str, "li") == 0) {
            tag_margin_top = 2; tag_margin_bottom = 2; tag_padding = 16; // Indent
        }
        else if (strcmp(tag_str, "ul") == 0 || strcmp(tag_str, "ol") == 0) {
            tag_margin_top = 4; tag_margin_bottom = 4; tag_padding = 8;
        }
        // Block quote
        else if (strcmp(tag_str, "blockquote") == 0) {
            tag_padding = 16; tag_bg = QP_BG_PANEL;
        }
        // Pre/code block
        else if (strcmp(tag_str, "pre") == 0) {
            tag_bg = QP_MNT_DARK; tag_padding = 8;
        }
        // Table elements - render as blocks
        else if (strcmp(tag_str, "table") == 0) {
            tag_margin_top = 8; tag_margin_bottom = 8;
        }
        else if (strcmp(tag_str, "tr") == 0) {
            // Table rows are block-level
        }
        else if (strcmp(tag_str, "td") == 0 || strcmp(tag_str, "th") == 0) {
            display = DisplayType::Inline;
            tag_padding = 4;
            if (strcmp(tag_str, "th") == 0) is_bold = true;
        }
        
        // 5. Parse inline styles for additional properties
        bool is_flex = false;
        uint32_t inline_color = 0;
        uint32_t inline_bg = 0;
        int inline_height = -1;
        int inline_width = -1;
        int inline_margin_top = -1;
        
        if (style_val) {
            const char* sv = (const char*)style_val;
            
            // Flexbox
            if (strstr(sv, "display:flex") || strstr(sv, "display: flex")) {
                is_flex = true;
            }
            
            // Color
            const char* color_ptr = strstr(sv, "color:");
            if (color_ptr) {
                color_ptr += 6;
                while (*color_ptr == ' ') color_ptr++;
                if (*color_ptr == '#') {
                    inline_color = parseHexColor(color_ptr);
                }
            }
            
            // Background-color
            const char* bg_ptr = strstr(sv, "background-color:");
            if (!bg_ptr) bg_ptr = strstr(sv, "background:");
            if (bg_ptr) {
                while (*bg_ptr && *bg_ptr != ':') bg_ptr++;
                if (*bg_ptr == ':') bg_ptr++;
                while (*bg_ptr == ' ') bg_ptr++;
                if (*bg_ptr == '#') {
                    inline_bg = parseHexColor(bg_ptr);
                }
            }
            
            // Height
            const char* h_ptr = strstr(sv, "height:");
            if (h_ptr) {
                inline_height = parsePxValue(h_ptr + 7);
            }
            
            // Width
            const char* w_ptr = strstr(sv, "width:");
            if (w_ptr) {
                inline_width = parsePxValue(w_ptr + 6);
            }
            
            // Margin-top
            const char* m_ptr = strstr(sv, "margin-top:");
            if (m_ptr) {
                inline_margin_top = parsePxValue(m_ptr + 11);
            }
            
            // Text-align center
            if (strstr(sv, "text-align:center") || strstr(sv, "text-align: center")) {
                is_centered = true;
            }
        }
        
        // 6. Create the layout box
        if (is_flex) {
            box = new LayoutFlex(node);
            if (!box) return nullptr;
            box->style.justify_content = JustifyContent::Center;
            box->style.align_items = AlignItems::Center;
        } else if (display == DisplayType::Inline || display == DisplayType::InlineBlock) {
            box = new LayoutInline(node);
            if (!box) return nullptr;
            if (strcmp(tag_str, "button") == 0) {
                box->style.padding_left = 12;
                box->style.padding_right = 12;
                box->style.padding_top = 6;
                box->style.padding_bottom = 6;
                box->style.margin_right = 4;
                box->style.border_radius = 4;
            } else if (strcmp(tag_str, "input") == 0) {
                // Get type attribute
                const lxb_char_t *type_val = lxb_dom_element_get_attribute(element, (const lxb_char_t*)"type", 4, &len);
                if (type_val && strcmp((const char*)type_val, "hidden") == 0) {
                    delete box;
                    return nullptr;
                }
                box->style.width = 200;
                box->style.height = 28;
                box->style.border_radius = 4;
                
                // Add value or placeholder as a text child
                const lxb_char_t *val = lxb_dom_element_get_attribute(element, (const lxb_char_t*)"value", 5, &len);
                if (!val || len == 0) {
                    val = lxb_dom_element_get_attribute(element, (const lxb_char_t*)"placeholder", 11, &len);
                }
                if (val && len > 0) {
                    LayoutText* text_node = new LayoutText(node, (const char*)val);
                    if (text_node) {
                        text_node->style.color = 0xFF777777; // Grey text for inputs
                        text_node->style.padding_left = 6;
                        text_node->style.padding_top = 6;
                        box->appendChild(text_node);
                    }
                }
            }
        } else {
            box = new LayoutBlock(node);
            if (!box) return nullptr;
        }
        
        // 7. Apply tag-based styling
        if (tag_color != 0) { box->style.color = tag_color; box->style.color_set = true; }
        if (tag_bg != 0) box->style.bg_color = tag_bg;
        if (tag_border_color != 0) box->style.border_color = tag_border_color;
        if (tag_font_size > 0) box->style.font_size = tag_font_size;
        if (tag_margin_top > 0) box->style.margin_top = tag_margin_top;
        if (tag_margin_bottom > 0) box->style.margin_bottom = tag_margin_bottom;
        if (tag_padding > 0) {
            box->style.padding_left = tag_padding;
            box->style.padding_right = tag_padding;
        }
        
        box->style.is_image = is_img_tag;
        box->style.is_link = is_a_tag;
        box->style.is_bold = is_bold;
        box->style.is_italic = is_italic;
        box->style.is_underline = is_underline;
        box->style.is_centered = is_centered;
        
        // 8. Apply inline style overrides (higher priority than tag defaults)
        if (inline_color != 0) { box->style.color = inline_color; box->style.color_set = true; }
        if (inline_bg != 0) box->style.bg_color = inline_bg;
        if (inline_height >= 0) box->style.height = inline_height;
        if (inline_width >= 0) box->style.width = inline_width;
        if (inline_margin_top >= 0) box->style.margin_top = inline_margin_top;
        
        // 9. Build children recursively (with limits)
        g_total_nodes++;
        lxb_dom_node_t *child_node = lxb_dom_node_first_child(node);
        int child_count = 0;
        while (child_node && child_count < MAX_CHILDREN_PER_NODE && g_total_nodes < MAX_TOTAL_NODES) {
            LayoutBox* child_box = buildLayoutTree(child_node, depth + 1);
            if (child_box) {
                // Inherit text color from parent if child has default
                if (!child_box->style.color_set && box->style.color_set) {
                    child_box->style.color = box->style.color;
                    child_box->style.color_set = true;
                }
                // Inherit bold/italic
                if (box->style.is_bold) child_box->style.is_bold = true;
                if (box->style.is_italic) child_box->style.is_italic = true;
                if (box->style.is_underline) child_box->style.is_underline = true;
                
                box->appendChild(child_box);
                child_count++;
            }
            child_node = lxb_dom_node_next(child_node);
        }
        
    } else if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
        size_t len;
        const lxb_char_t* text = lxb_dom_node_text_content(node, &len);
        if (!text || len == 0) return nullptr;
        
        // Cap text length to prevent excessive memory use
        if (len > 256) {
            len = 256;
            while (len > 0 && (text[len-1] & 0xC0) == 0x80) len--;
        }
        
        // Normalize whitespace (collapse spaces, trim)
        extern void kfree(void*);
        char* normalized = normalizeText((const char*)text, len);
        if (!normalized || normalized[0] == '\0') {
            if (normalized) kfree(normalized);
            return nullptr;
        }
        
        box = new LayoutText(node, normalized);
        kfree(normalized);
        if (!box) return nullptr;
        g_total_nodes++;
    }
    
    return box;
}

void RenderEngine::computeStyles() {
    if (!m_document) return;
    kprintf("[RenderEngine] Computing DOM styles and building Layout Tree...\n");
    
    if (m_layoutRoot) {
        delete m_layoutRoot;
        m_layoutRoot = nullptr;
    }
    
    lxb_dom_node_t *body = nullptr;
    if (m_document->body) {
        body = lxb_dom_interface_node(m_document->body);
    }
    if (!body) {
        lxb_html_body_element_t *body_el = lxb_html_document_body_element(m_document);
        if (body_el) {
            body = lxb_dom_interface_node(body_el);
        }
    }
    if (!body) {
        body = lxb_dom_interface_node(lxb_dom_document_element(&m_document->dom_document));
    }
    
    if (body) {
        g_total_nodes = 0; // Reset node counter
        m_layoutRoot = buildLayoutTree(body);
        if (m_layoutRoot) {
            // QuantumPeak browser defaults: dark background, white text
            m_layoutRoot->style.bg_color = QP_BG_DARK;
            m_layoutRoot->style.color = QP_TEXT_MAIN;
            m_layoutRoot->style.padding_left = 12;
            m_layoutRoot->style.padding_right = 12;
            m_layoutRoot->style.padding_top = 8;
        }
    }
}

void RenderEngine::layout(int viewport_width, int viewport_height) {
    if (!m_layoutRoot) return;
    kprintf("[RenderEngine] Performing Layout (Viewport: %dx%d)...\n", viewport_width, viewport_height);
    
    m_layoutRoot->x = 0;
    m_layoutRoot->y = 110;
    
    // Force root element to cover at least the viewport height
    if (m_layoutRoot->style.height < (viewport_height - 110)) {
        m_layoutRoot->style.height = viewport_height - 110;
    }
    
    m_layoutRoot->layout(viewport_width);
}

void RenderEngine::paint(void* surface) {
    if (!m_layoutRoot) return;
    
    struct window* win = (struct window*)surface;
    m_layoutRoot->paint(win);
}
