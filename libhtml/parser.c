#include "libhtml/parser.h"
#include "libhtml/css.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"

/* starts_with removed to fix warning */

static tag_type_t get_tag_type(const char *tag_name) {
    if (strcmp(tag_name, "div") == 0 ||
        strcmp(tag_name, "body") == 0 ||
        strcmp(tag_name, "html") == 0 ||
        strcmp(tag_name, "main") == 0 ||
        strcmp(tag_name, "section") == 0 ||
        strcmp(tag_name, "header") == 0 ||
        strcmp(tag_name, "footer") == 0 ||
        strcmp(tag_name, "nav") == 0) return TAG_DIV;
        
    if (strcmp(tag_name, "span") == 0 ||
        strcmp(tag_name, "p") == 0 ||
        strcmp(tag_name, "h1") == 0 ||
        strcmp(tag_name, "h2") == 0 ||
        strcmp(tag_name, "h3") == 0 ||
        strcmp(tag_name, "a") == 0 ||
        strcmp(tag_name, "label") == 0) return TAG_SPAN;
        
    if (strcmp(tag_name, "svg") == 0) return TAG_SVG;
    if (strcmp(tag_name, "path") == 0) return TAG_PATH;
    if (strcmp(tag_name, "circle") == 0) return TAG_CIRCLE;
    if (strcmp(tag_name, "rect") == 0) return TAG_RECT;
    if (strcmp(tag_name, "button") == 0) return TAG_BUTTON;
    
    return TAG_UNKNOWN;
}

/* Parse attributes from inside a tag: <div id="x" class="y" style="z"> */
static void parse_attributes(dom_node_t *node, const char *attr_str, int len) {
    char buf[512];
    if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
    strncpy(buf, attr_str, len);
    buf[len] = '\0';
    
    char *cursor = buf;
    while (*cursor) {
        while (*cursor == ' ') cursor++;
        if (!*cursor) break;
        
        char *eq = strchr(cursor, '=');
        if (!eq) break;
        
        *eq = '\0';
        char *name = cursor;
        char *val_start = eq + 1;
        
        char quote = *val_start;
        if (quote == '"' || quote == '\'') {
            val_start++;
        } else {
            quote = ' ';
        }
        
        char *val_end = strchr(val_start, quote);
        if (!val_end) {
            val_end = val_start + strlen(val_start);
            cursor = val_end;
        } else {
            *val_end = '\0';
            cursor = val_end + 1;
        }
        
        /* Save attributes */
        if (strcmp(name, "id") == 0) {
            strncpy(node->id, val_start, sizeof(node->id) - 1);
        } else if (strcmp(name, "class") == 0) {
            strncpy(node->classes, val_start, sizeof(node->classes) - 1);
        } else if (strcmp(name, "style") == 0) {
            css_parse_style(&node->style, val_start);
        } else if (strcmp(name, "d") == 0) {
            strncpy(node->svg_d, val_start, sizeof(node->svg_d) - 1);
        } else if (strcmp(name, "cx") == 0) {
            node->cx = atoi(val_start);
        } else if (strcmp(name, "cy") == 0) {
            node->cy = atoi(val_start);
        } else if (strcmp(name, "r") == 0) {
            node->r = atoi(val_start);
        } else if (strcmp(name, "href") == 0) {
            strncpy(node->href, val_start, sizeof(node->href) - 1);
        }
    }
}

dom_node_t *html_parse(const char *html) {
    if (!html) return NULL;
    
    dom_node_t *root = dom_create_node(TAG_DIV);
    strcpy(root->name, "root");
    
    dom_node_t *current = root;
    const char *cursor = html;
    
    while (*cursor) {
        if (*cursor == '<') {
            /* Check if it's a closing tag */
            if (*(cursor + 1) == '/') {
                const char *end = strchr(cursor, '>');
                if (!end) break;
                
                char close_tag[16] = {0};
                int len = end - (cursor + 2);
                if (len > 15) len = 15;
                strncpy(close_tag, cursor + 2, len);
                
                if (strcmp(close_tag, "circle") != 0 && strcmp(close_tag, "path") != 0 && strcmp(close_tag, "br") != 0 && strcmp(close_tag, "img") != 0) {
                    if (current->parent) {
                        current = current->parent;
                    }
                }
                cursor = end + 1;
                continue;
            }
            
            /* Opening tag */
            const char *end = strchr(cursor, '>');
            if (!end) break;
            
            /* Check for self-closing */
            bool self_closing = false;
            if (*(end - 1) == '/') {
                self_closing = true;
            }
            
            /* Extract tag name */
            const char *name_start = cursor + 1;
            while (*name_start == ' ') name_start++;
            
            const char *name_end = name_start;
            while (name_end < end && *name_end != ' ' && *name_end != '/') name_end++;
            
            char tag_name[16] = {0};
            int name_len = name_end - name_start;
            if (name_len > 15) name_len = 15;
            strncpy(tag_name, name_start, name_len);
            
            tag_type_t type = get_tag_type(tag_name);
            dom_node_t *new_node = dom_create_node(type);
            strcpy(new_node->name, tag_name);
            
            /* Parse attributes */
            if (name_end < end) {
                int attr_len = (self_closing ? end - 1 : end) - name_end;
                parse_attributes(new_node, name_end, attr_len);
            }
            
            dom_add_child(current, new_node);
            
            if (!self_closing && type != TAG_UNKNOWN && 
                strcmp(tag_name, "circle") != 0 && strcmp(tag_name, "path") != 0 && 
                strcmp(tag_name, "br") != 0 && strcmp(tag_name, "img") != 0) {
                current = new_node;
            }
            
            cursor = end + 1;
        } else {
            /* Text content */
            const char *next_tag = strchr(cursor, '<');
            if (!next_tag) next_tag = cursor + strlen(cursor);
            
            int text_len = next_tag - cursor;
            
            /* Trim whitespace */
            while (text_len > 0 && (*cursor == ' ' || *cursor == '\n' || *cursor == '\r')) {
                cursor++;
                text_len--;
            }
            
            if (text_len > 0) {
                dom_node_t *text_node = dom_create_node(TAG_TEXT);
                if (text_len > 255) text_len = 255;
                strncpy(text_node->text, cursor, text_len);
                text_node->text[text_len] = '\0';
                
                dom_add_child(current, text_node);
            }
            
            cursor = next_tag;
        }
    }
    
    return root;
}
