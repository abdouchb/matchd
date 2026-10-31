/*
 * Zarqaa UI (ZUI) Window Widget
 * Creator: Abdelhak Chabbi
 */

#ifndef LIBZUI_ZUI_WINDOW_H
#define LIBZUI_ZUI_WINDOW_H

#include "libzui/zui.h"

typedef struct zui_window zui_window_t;

struct zui_window {
    zui_widget_t base;
    char title[64];
    zui_widget_t *content_pane; // Inner container for widgets
};

zui_window_t *zui_window_create(int x, int y, int width, int height, const char *title);

#endif /* LIBZUI_ZUI_WINDOW_H */
