/*
 * Zarqaa OS 0.1 - Widget System Header
 * Creator: Abdelhak Chabbi
 */

#ifndef GUI_WIDGET_H
#define GUI_WIDGET_H

#include "zarqaa/types.h"

enum widget_type {
    WIDGET_LABEL,
    WIDGET_BUTTON,
    WIDGET_TEXTBOX,
    WIDGET_PANEL
};

struct rect_t {
    int x, y, w, h;
};

struct widget {
    enum widget_type type;
    struct rect_t bounds;
    char text[64];
    color_t bg_color;
    color_t fg_color;
    bool visible;
    bool focused;
    bool hovered;
    void (*on_click)(struct widget *self);
    struct widget *parent;
};

struct widget *widget_create(enum widget_type type, int x, int y, int w, int h, const char *text);
void widget_draw(struct widget *w);
void widget_handle_mouse(struct widget *w, int mx, int my, bool clicked);
void widget_set_text(struct widget *w, const char *text);

#endif /* GUI_WIDGET_H */
