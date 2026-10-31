/*
 * Zarqaa UI (ZUI) Button Implementation
 * Creator: Abdelhak Chabbi
 */

#include "libzui/zui_button.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

static void zui_button_draw(zui_widget_t *base, int abs_x, int abs_y) {
    zui_button_t *btn = (zui_button_t *)base;
    
    color_t bg = base->bg_color;
    if (btn->is_pressed) {
        bg = gfx_alpha_blend(0xFF000000, bg, 100); // Darken
    } else if (btn->is_hovered) {
        bg = gfx_alpha_blend(0xFFFFFFFF, bg, 50); // Lighten
    }
    
    // Draw button background
    gfx_draw_rounded_rect_filled(abs_x, abs_y, base->width, base->height, 6, bg);
    
    // Draw border
    gfx_draw_rounded_rect(abs_x, abs_y, base->width, base->height, 6, 0xFF334455);
    
    // Draw text (centered simply)
    int text_len = strlen(btn->text);
    int text_w = text_len * 8; // 8x16 font
    int text_x = abs_x + (base->width - text_w) / 2;
    int text_y = abs_y + (base->height - 16) / 2;
    
    fb_draw_string_transparent(text_x, text_y, btn->text, base->fg_color);
}

static void zui_button_on_mouse(zui_widget_t *base, int local_x, int local_y, bool is_click) {
    zui_button_t *btn = (zui_button_t *)base;
    (void)local_x; (void)local_y;
    
    btn->is_hovered = true; // Since the mouse event arrived here, we are hovering
    
    if (is_click) {
        btn->is_pressed = true;
        if (btn->on_click) {
            btn->on_click(btn);
        }
    } else {
        btn->is_pressed = false;
    }
    
    base->needs_redraw = true;
}

zui_button_t *zui_button_create(int x, int y, int width, int height, const char *text) {
    zui_button_t *btn = kmalloc(sizeof(zui_button_t));
    if (!btn) return NULL;
    
    zui_widget_init(&btn->base, ZUI_WIDGET_BUTTON, x, y, width, height);
    
    btn->base.draw = zui_button_draw;
    btn->base.on_mouse = zui_button_on_mouse;
    
    btn->base.bg_color = 0xFF142A40; // Default button color
    btn->base.fg_color = 0xFFFFFFFF; // Default text color
    
    btn->is_hovered = false;
    btn->is_pressed = false;
    btn->on_click = NULL;
    
    if (text) {
        strncpy(btn->text, text, 63);
        btn->text[63] = '\0';
    } else {
        btn->text[0] = '\0';
    }
    
    return btn;
}
