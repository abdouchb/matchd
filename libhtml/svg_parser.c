#include "libhtml/svg_parser.h"
#include "zarqaa/string.h"

static const char* skip_whitespace_and_comma(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == ',') {
        s++;
    }
    return s;
}

static float parse_float(const char **str) {
    *str = skip_whitespace_and_comma(*str);
    if (!**str) return 0.0f;
    
    // Very basic float parser (since we don't have sscanf)
    float val = 0.0f;
    int sign = 1;
    if (**str == '-') {
        sign = -1;
        (*str)++;
    } else if (**str == '+') {
        (*str)++;
    }
    
    while (**str >= '0' && **str <= '9') {
        val = val * 10.0f + (**str - '0');
        (*str)++;
    }
    
    if (**str == '.') {
        (*str)++;
        float fraction = 0.1f;
        while (**str >= '0' && **str <= '9') {
            val += (**str - '0') * fraction;
            fraction *= 0.1f;
            (*str)++;
        }
    }
    
    return val * sign;
}

bool svg_parse_path_data(const char *d_attr, path_t *out_path) {
    if (!d_attr || !out_path) return false;
    
    const char *p = d_attr;
    char cmd = 0;
    
    while (*p) {
        p = skip_whitespace_and_comma(p);
        if (!*p) break;
        
        // If it's a letter, it's a new command
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
            cmd = *p;
            p++;
        }
        
        // Parse arguments based on command
        // Note: For simplicity, we only implement Absolute commands (M, L, Q, C, Z)
        switch (cmd) {
            case 'M': case 'm': {
                float x = parse_float(&p);
                float y = parse_float(&p);
                path_move_to(out_path, x, y);
                // Implicitly becomes L/l for subsequent coordinates
                cmd = (cmd == 'M') ? 'L' : 'l'; 
                break;
            }
            case 'L': case 'l': {
                float x = parse_float(&p);
                float y = parse_float(&p);
                path_line_to(out_path, x, y);
                break;
            }
            case 'Q': case 'q': {
                float cx = parse_float(&p);
                float cy = parse_float(&p);
                float x = parse_float(&p);
                float y = parse_float(&p);
                path_quad_to(out_path, cx, cy, x, y);
                break;
            }
            case 'C': case 'c': {
                float c1x = parse_float(&p);
                float c1y = parse_float(&p);
                float c2x = parse_float(&p);
                float c2y = parse_float(&p);
                float x = parse_float(&p);
                float y = parse_float(&p);
                path_cubic_to(out_path, c1x, c1y, c2x, c2y, x, y);
                break;
            }
            case 'Z': case 'z': {
                path_close(out_path);
                cmd = 0; // Require new command
                break;
            }
            default:
                // Unknown or unsupported command, just skip token
                p++;
                break;
        }
    }
    
    return true;
}
