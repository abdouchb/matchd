/*
 * Zarqaa UI (ZUI) Window Implementation
 * Creator: Abdelhak Chabbi
 */

#include "libzui/zui_window.h"
#include "libzui/zui_panel.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

static void zui_window_draw(zui_widget_t *base, int abs_x, int abs_y) {
    zui_window_t *win = (zui_window_t *)base;
    
    // Draw window background (glass effect)
    gfx_draw_glass_panel(abs_x, abs_y, base->width, base->height, 12, 0xFF0A1520, 0xFF1A3550, 200);
    
    // Draw titlebar area
    gfx_draw_rounded_rect_filled(abs_x, abs_y, base->width, 30, 12, 0xFF050D14);
    
    // Title text
    fb_draw_string_transparent(abs_x + 15, abs_y + 7, win->title, 0xFFFFFFFF);
}

zui_window_t *zui_window_create(int x, int y, int width, int height, const char *title) {
    zui_window_t *win = kmalloc(sizeof(zui_window_t));
    if (!win) return NULL;
    
    zui_widget_init(&win->base, ZUI_WIDGET_WINDOW, x, y, width, height);
    win->base.draw = zui_window_draw;
    
    if (title) {
        strncpy(win->title, title, 63);
        win->title[63] = '\0';
    } else {
        win->title[0] = '\0';
    }
    
    // Create inner content pane (offset by titlebar)
    zui_panel_t *pane = zui_panel_create(0, 30, width, height - 30, 0x0); // Transparent pane
    zui_widget_add_child(&win->base, &pane->base);
    win->content_pane = &pane->base;
    
    return win;
}
