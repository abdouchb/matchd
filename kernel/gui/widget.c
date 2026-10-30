/*
 * Zarqaa OS 0.1 - Widget System
 * Creator: Abdelhak Chabbi
 */

#include "gui/widget.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

struct widget *widget_create(enum widget_type type, int x, int y, int w, int h, const char *text) {
    struct widget *w_ptr = kmalloc(sizeof(struct widget));
    if (!w_ptr) return NULL;
    
    w_ptr->type = type;
    w_ptr->bounds.x = x;
    w_ptr->bounds.y = y;
    w_ptr->bounds.w = w;
    w_ptr->bounds.h = h;
    
    if (text) {
        strncpy(w_ptr->text, text, 63);
        w_ptr->text[63] = '\0';
    } else {
        w_ptr->text[0] = '\0';
    }
    
    w_ptr->visible = true;
    w_ptr->focused = false;
    w_ptr->hovered = false;
    w_ptr->on_click = NULL;
    w_ptr->parent = NULL;
    
    switch (type) {
        case WIDGET_BUTTON:
            w_ptr->bg_color = COLOR_ACCENT;
            w_ptr->fg_color = COLOR_TEXT_LIGHT;
            break;
        case WIDGET_LABEL:
            w_ptr->bg_color = 0x0; // Transparent essentially
            w_ptr->fg_color = COLOR_TEXT_DARK;
            break;
        case WIDGET_TEXTBOX:
            w_ptr->bg_color = COLOR_WINDOW_BODY;
            w_ptr->fg_color = COLOR_TEXT_DARK;
            break;
        case WIDGET_PANEL:
            w_ptr->bg_color = COLOR_WINDOW_BODY;
            w_ptr->fg_color = 0x0;
            break;
    }
    
    return w_ptr;
}

void widget_draw(struct widget *w) {
    if (!w->visible) return;
    
    int x = w->bounds.x;
    int y = w->bounds.y;
    int bw = w->bounds.w;
    int bh = w->bounds.h;
    
    switch (w->type) {
        case WIDGET_BUTTON:
            if (w->hovered) {
                gfx_draw_rounded_rect_filled(x, y, bw, bh, 5, gfx_alpha_blend(COLOR_TEXT_LIGHT, w->bg_color, 50));
            } else {
                gfx_draw_rounded_rect_filled(x, y, bw, bh, 5, w->bg_color);
            }
            // Center text
            int tw = strlen(w->text) * 8;
            int th = 16;
            fb_draw_string_transparent(x + (bw - tw) / 2, y + (bh - th) / 2, w->text, w->fg_color);
            break;
            
        case WIDGET_LABEL:
            fb_draw_string_transparent(x, y, w->text, w->fg_color);
            break;
            
        case WIDGET_TEXTBOX:
            fb_fill_rect(x, y, bw, bh, w->bg_color);
            fb_draw_rect(x, y, bw, bh, w->focused ? COLOR_ACCENT : COLOR_WINDOW_BORDER);
            fb_draw_string_transparent(x + 5, y + 5, w->text, w->fg_color);
            if (w->focused) {
                // Draw cursor
                int cw = strlen(w->text) * 8;
                fb_draw_line(x + 5 + cw, y + 5, x + 5 + cw, y + 5 + 16, w->fg_color);
            }
            break;
            
        case WIDGET_PANEL:
            fb_fill_rect(x, y, bw, bh, w->bg_color);
            break;
    }
}

void widget_handle_mouse(struct widget *w, int mx, int my, bool clicked) {
    if (!w->visible) return;
    
    bool in_bounds = (mx >= w->bounds.x && mx < w->bounds.x + w->bounds.w &&
                      my >= w->bounds.y && my < w->bounds.y + w->bounds.h);
                      
    w->hovered = in_bounds;
    
    if (clicked) {
        if (in_bounds) {
            w->focused = true;
            if (w->on_click) {
                w->on_click(w);
            }
        } else {
            w->focused = false;
        }
    }
}

void widget_set_text(struct widget *w, const char *text) {
    if (text) {
        strncpy(w->text, text, 63);
        w->text[63] = '\0';
    } else {
        w->text[0] = '\0';
    }
}
