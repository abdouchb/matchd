/*
 * Zarqaa UI (ZUI) Label Implementation
 * Creator: Abdelhak Chabbi
 */

#include "libzui/zui_label.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

static void zui_label_draw(zui_widget_t *base, int abs_x, int abs_y) {
    zui_label_t *lbl = (zui_label_t *)base;
    fb_draw_string_transparent(abs_x, abs_y, lbl->text, base->fg_color);
}

zui_label_t *zui_label_create(int x, int y, const char *text) {
    zui_label_t *lbl = kmalloc(sizeof(zui_label_t));
    if (!lbl) return NULL;
    
    int len = text ? strlen(text) : 0;
    zui_widget_init(&lbl->base, ZUI_WIDGET_LABEL, x, y, len * 8, 16);
    
    lbl->base.draw = zui_label_draw;
    lbl->base.bg_color = 0x0; // Transparent
    lbl->base.fg_color = 0xFFFFFFFF; // White
    
    if (text) {
        strncpy(lbl->text, text, 127);
        lbl->text[127] = '\0';
    } else {
        lbl->text[0] = '\0';
    }
    
    return lbl;
}
