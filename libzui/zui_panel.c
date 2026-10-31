/*
 * Zarqaa UI (ZUI) Panel Implementation
 * Creator: Abdelhak Chabbi
 */

#include "libzui/zui_panel.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "kernel/heap.h"

static void zui_panel_draw(zui_widget_t *base, int abs_x, int abs_y) {
    zui_panel_t *pnl = (zui_panel_t *)base;
    
    if (base->bg_color != 0x0) {
        if (pnl->corner_radius > 0) {
            gfx_draw_rounded_rect_filled(abs_x, abs_y, base->width, base->height, pnl->corner_radius, base->bg_color);
        } else {
            fb_fill_rect(abs_x, abs_y, base->width, base->height, base->bg_color);
        }
    }
}

zui_panel_t *zui_panel_create(int x, int y, int width, int height, color_t bg_color) {
    zui_panel_t *pnl = kmalloc(sizeof(zui_panel_t));
    if (!pnl) return NULL;
    
    zui_widget_init(&pnl->base, ZUI_WIDGET_PANEL, x, y, width, height);
    
    pnl->base.draw = zui_panel_draw;
    pnl->base.bg_color = bg_color;
    pnl->corner_radius = 8;
    
    return pnl;
}
