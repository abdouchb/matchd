/*
 * Zarqaa UI (ZUI) Button Widget
 * Creator: Abdelhak Chabbi
 */

#ifndef LIBZUI_ZUI_BUTTON_H
#define LIBZUI_ZUI_BUTTON_H

#include "libzui/zui.h"

typedef struct zui_button zui_button_t;

struct zui_button {
    zui_widget_t base; // Must be first
    
    char text[64];
    bool is_hovered;
    bool is_pressed;
    
    void (*on_click)(zui_button_t *self);
};

zui_button_t *zui_button_create(int x, int y, int width, int height, const char *text);

#endif /* LIBZUI_ZUI_BUTTON_H */
