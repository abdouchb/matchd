/*
 * Zarqaa UI (ZUI) Label Widget
 * Creator: Abdelhak Chabbi
 */

#ifndef LIBZUI_ZUI_LABEL_H
#define LIBZUI_ZUI_LABEL_H

#include "libzui/zui.h"

typedef struct zui_label zui_label_t;

struct zui_label {
    zui_widget_t base;
    char text[128];
};

zui_label_t *zui_label_create(int x, int y, const char *text);

#endif /* LIBZUI_ZUI_LABEL_H */
