#include "libhtml/css.h"
#include "zarqaa/string.h"

/* Convert a hex string like "#FF0000" or "FF0000" to color_t */
static color_t parse_color(const char *hex) {
    if (hex[0] == '#') hex++;
    
    uint32_t val = 0;
    int len = strlen(hex);
    if (len > 6) len = 6;
    
    for (int i = 0; i < len; i++) {
        char c = hex[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
    }
    
    if (len == 3) {
        /* Expand #RGB to #RRGGBB */
        uint32_t r = (val >> 8) & 0xF;
        uint32_t g = (val >> 4) & 0xF;
        uint32_t b = val & 0xF;
        val = (r << 20) | (r << 16) | (g << 12) | (g << 8) | (b << 4) | b;
    }
    
    return (0xFF << 24) | val;
}

/* Parse an integer value, optionally ignoring "px" or "%" */
static int parse_int(const char *val_str) {
    int val = 0;
    bool negative = false;
    
    if (*val_str == '-') {
        negative = true;
        val_str++;
    }
    
    while (*val_str >= '0' && *val_str <= '9') {
        val = val * 10 + (*val_str - '0');
        val_str++;
    }
    
    return negative ? -val : val;
}

void css_parse_style(css_style_t *style, const char *style_str) {
    if (!style || !style_str) return;
    
    char buf[512];
    strncpy(buf, style_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    char *cursor = buf;
    while (*cursor) {
        while (*cursor == ' ') cursor++;
        if (!*cursor) break;
        
        char *colon = strchr(cursor, ':');
        if (!colon) break;
        
        *colon = '\0';
        char *prop = cursor;
        char *val_start = colon + 1;
        
        while (*val_start == ' ') val_start++;
        
        char *semicolon = strchr(val_start, ';');
        if (semicolon) {
            *semicolon = '\0';
            cursor = semicolon + 1;
        } else {
            cursor = val_start + strlen(val_start);
        }
        
        /* Trim leading and trailing spaces from prop and val */
        while (*prop == ' ') prop++;
        char *p = prop + strlen(prop) - 1;
        while (p > prop && *p == ' ') *p-- = '\0';
        
        while (*val_start == ' ') val_start++;
        p = val_start + strlen(val_start) - 1;
        while (p > val_start && *p == ' ') *p-- = '\0';
        
        /* Apply properties */
        if (strcmp(prop, "width") == 0) {
            style->width = parse_int(val_start);
        } else if (strcmp(prop, "height") == 0) {
            style->height = parse_int(val_start);
        } else if (strcmp(prop, "color") == 0) {
            style->text_color = parse_color(val_start);
        } else if (strcmp(prop, "background-color") == 0 || strcmp(prop, "background") == 0) {
            if (strncmp(val_start, "transparent", 11) == 0) {
                style->bg_color = 0;
            } else {
                style->bg_color = parse_color(val_start);
            }
        } else if (strcmp(prop, "border-radius") == 0) {
            style->border_radius = parse_int(val_start);
        } else if (strcmp(prop, "border-color") == 0) {
            style->border_color = parse_color(val_start);
        } else if (strcmp(prop, "border-width") == 0) {
            style->border_width = parse_int(val_start);
        } else if (strcmp(prop, "left") == 0) {
            style->left = parse_int(val_start);
        } else if (strcmp(prop, "top") == 0) {
            style->top = parse_int(val_start);
        } else if (strcmp(prop, "display") == 0) {
            if (strcmp(val_start, "flex") == 0) style->is_flex = true;
        } else if (strcmp(prop, "flex-direction") == 0) {
            if (strcmp(val_start, "column") == 0) style->flex_col = true;
        } else if (strcmp(prop, "gap") == 0) {
            style->gap = parse_int(val_start);
        } else if (strcmp(prop, "z-index") == 0) {
            style->z_index = parse_int(val_start);
        }
    }
}
