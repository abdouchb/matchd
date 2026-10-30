/*
 * Zarqaa UI (ZUI) Panel Widget
 * Creator: Abdelhak Chabbi
 */

#ifndef LIBZUI_ZUI_PANEL_H
#define LIBZUI_ZUI_PANEL_H

#include "libzui/zui.h"

typedef struct zui_panel zui_panel_t;

struct zui_panel {
    zui_widget_t base;
    int corner_radius;
};

zui_panel_t *zui_panel_create(int x, int y, int width, int height, color_t bg_color);

#endif /* LIBZUI_ZUI_PANEL_H */
