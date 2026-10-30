/*
 * Zarqaa UI (ZUI) Mini HTML Parser Implementation
 * Creator: Abdelhak Chabbi
 */

#include "libzui/zui_html_parser.h"
#include "libzui/zui_label.h"
#include "libzui/zui_button.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"

// Simple strstr since we might not have it in zarqaa/string.h
static const char *kstrstr(const char *haystack, const char *needle) {
    if (!*needle) return haystack;
    for (const char *h = haystack; *h; h++) {
        const char *h_ptr = h;
        const char *n_ptr = needle;
        while (*h_ptr && *n_ptr && *h_ptr == *n_ptr) {
            h_ptr++;
            n_ptr++;
        }
        if (!*n_ptr) return h; // Match found
    }
    return NULL;
}

// Simple tag content extractor
static bool extract_tag_content(const char *html, const char *tag_start, const char *tag_end, char *out_buffer, int max_len, const char **next_pos) {
    const char *start = kstrstr(html, tag_start);
    if (!start) return false;
    
    start += strlen(tag_start);
    const char *end = kstrstr(start, tag_end);
    if (!end) return false;
    
    int len = end - start;
    if (len >= max_len) len = max_len - 1;
    
    for (int i = 0; i < len; i++) {
        out_buffer[i] = start[i];
    }
    out_buffer[len] = '\0';
    
    if (next_pos) {
        *next_pos = end + strlen(tag_end);
    }
    return true;
}

// Helper to extract an attribute (e.g. color="red")
static bool extract_attribute(const char *tag_str, const char *attr_name, char *out_val, int max_len) {
    const char *attr = kstrstr(tag_str, attr_name);
    if (!attr) return false;
    attr += strlen(attr_name);
    while (*attr && (*attr == ' ' || *attr == '=')) attr++;
    if (*attr == '"' || *attr == '\'') attr++; // Skip quote
    
    int i = 0;
    while (*attr && *attr != '"' && *attr != '\'' && *attr != ' ' && *attr != '>' && i < max_len - 1) {
        out_val[i++] = *attr++;
    }
    out_val[i] = '\0';
    return true;
}

// Convert a hex string like "FF0000" to color_t
static color_t parse_color(const char *hex) {
    if (hex[0] == '#') hex++;
    uint32_t val = 0;
    for (int i = 0; hex[i] && i < 6; i++) {
        char c = hex[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
    }
    return (0xFF << 24) | val;
}

void zui_parse_html(zui_widget_t *parent_panel, const char *html) {
    if (!parent_panel || !html) return;
    
    int margin_x = 20;
    int margin_y = 20;
    int cur_x = margin_x;
    int cur_y = margin_y;
    int max_row_h = 0;
    int container_w = parent_panel->width;
    
    const char *cursor = html;
    char content_buf[256];
    char attr_buf[32];
    
    while (cursor && *cursor) {
        const char *next_tag = kstrstr(cursor, "<");
        if (!next_tag) break;
        
        char tag_name[16] = {0};
        const char *t = next_tag + 1;
        int ti = 0;
        while (*t && *t != '>' && *t != ' ' && ti < 15) {
            tag_name[ti++] = *t++;
        }
        tag_name[ti] = '\0';
        
        char end_tag[32] = "</";
        strcat(end_tag, tag_name);
        strcat(end_tag, ">");
        
        // Find closing bracket of the start tag to parse attributes
        const char *tag_close = kstrstr(next_tag, ">");
        if (!tag_close) { cursor++; continue; }
        
        bool is_block = false;
        int elem_w = 0, elem_h = 0;
        zui_widget_t *new_widget = NULL;
        
        if (strcmp(tag_name, "h1") == 0 || strcmp(tag_name, "h2") == 0 || strcmp(tag_name, "h3") == 0) {
            is_block = true; // Headers force a new line
            if (extract_tag_content(next_tag, next_tag, end_tag, content_buf, sizeof(content_buf), &cursor)) {
                // We strip the actual opening tag manually since it has attributes
                const char *actual_content = kstrstr(content_buf, ">");
                actual_content = actual_content ? actual_content + 1 : content_buf;
                
                zui_label_t *lbl = zui_label_create(cur_x, cur_y, actual_content);
                lbl->base.fg_color = 0xFFFFFFFF;
                
                if (strcmp(tag_name, "h1") == 0) lbl->base.fg_color = 0xFF00F0FF; // Cyan 
                else if (strcmp(tag_name, "h2") == 0) lbl->base.fg_color = 0xFF00FF90; // Greenish
                else if (strcmp(tag_name, "h3") == 0) lbl->base.fg_color = 0xFFFFD700; // Gold
                
                if (extract_attribute(next_tag, "color", attr_buf, sizeof(attr_buf))) {
                    lbl->base.fg_color = parse_color(attr_buf);
                }
                
                new_widget = &lbl->base;
                elem_w = strlen(actual_content) * 8 + 10;
                elem_h = 30;
            } else { cursor++; }
        }
        else if (strcmp(tag_name, "p") == 0) {
            is_block = true;
            if (extract_tag_content(next_tag, next_tag, end_tag, content_buf, sizeof(content_buf), &cursor)) {
                const char *actual_content = kstrstr(content_buf, ">");
                actual_content = actual_content ? actual_content + 1 : content_buf;
                zui_label_t *lbl = zui_label_create(cur_x, cur_y, actual_content);
                lbl->base.fg_color = 0xFFCCCCCC; // Light gray
                if (extract_attribute(next_tag, "color", attr_buf, sizeof(attr_buf))) {
                    lbl->base.fg_color = parse_color(attr_buf);
                }
                new_widget = &lbl->base;
                elem_w = strlen(actual_content) * 8 + 10;
                elem_h = 24;
            } else { cursor++; }
        }
        else if (strcmp(tag_name, "button") == 0) {
            is_block = false; // Buttons are inline
            if (extract_tag_content(next_tag, next_tag, end_tag, content_buf, sizeof(content_buf), &cursor)) {
                const char *actual_content = kstrstr(content_buf, ">");
                actual_content = actual_content ? actual_content + 1 : content_buf;
                elem_w = strlen(actual_content) * 8 + 40;
                elem_h = 32;
                
                // Flow Layout Wrapping
                if (cur_x + elem_w > container_w - margin_x) {
                    cur_x = margin_x;
                    cur_y += max_row_h + 10;
                    max_row_h = 0;
                }
                
                zui_button_t *btn = zui_button_create(cur_x, cur_y, elem_w, elem_h, actual_content);
                new_widget = &btn->base;
            } else { cursor++; }
        }
        else if (strcmp(tag_name, "div") == 0) {
            is_block = true;
            if (extract_tag_content(next_tag, next_tag, end_tag, content_buf, sizeof(content_buf), &cursor)) {
                // A div currently just forces a new line and acts as a spacer in this PoC
                elem_w = container_w;
                elem_h = 20; 
                new_widget = NULL; // We don't actually create a widget for empty div yet
            } else { cursor++; }
        }
        else if (strcmp(tag_name, "br") == 0) {
            is_block = true;
            elem_w = container_w;
            elem_h = 10;
            cursor = tag_close + 1;
        }
        else {
            // Unrecognized tag
            cursor = tag_close + 1;
        }
        
        // Handle Layout
        if (new_widget) {
            zui_widget_add_child(parent_panel, new_widget);
        }
        
        if (is_block) {
            cur_x = margin_x;
            cur_y += elem_h + 5;
            max_row_h = 0;
        } else if (elem_w > 0) {
            cur_x += elem_w + 10;
            if (elem_h > max_row_h) max_row_h = elem_h;
        }
    }
}
