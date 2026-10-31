/*
 * Zarqaa UI (ZUI) Core API
 * Creator: Abdelhak Chabbi
 * Description: An Object-Oriented C GUI Toolkit.
 */

#ifndef LIBZUI_ZUI_H
#define LIBZUI_ZUI_H

#include "zarqaa/types.h"

typedef enum {
    ZUI_WIDGET_BASE,
    ZUI_WIDGET_BUTTON,
    ZUI_WIDGET_LABEL,
    ZUI_WIDGET_PANEL,
    ZUI_WIDGET_WINDOW
} zui_widget_type_t;

typedef struct zui_widget zui_widget_t;

/* Base Widget Structure */
struct zui_widget {
    zui_widget_type_t type;
    int x, y;          /* Relative to parent */
    int width, height;
    color_t bg_color;
    color_t fg_color;
    bool visible;
    bool needs_redraw;
    
    /* Hierarchy */
    zui_widget_t *parent;
    zui_widget_t **children;
    int child_count;
    int child_capacity;
    
    /* Virtual Methods */
    void (*draw)(zui_widget_t *self, int abs_x, int abs_y);
    void (*on_mouse)(zui_widget_t *self, int local_x, int local_y, bool is_click);
    void (*destroy)(zui_widget_t *self);
    
    /* Private */
    void *user_data;
};

/* Core Base Functions */
void zui_widget_init(zui_widget_t *w, zui_widget_type_t type, int x, int y, int width, int height);
void zui_widget_add_child(zui_widget_t *parent, zui_widget_t *child);
void zui_widget_draw(zui_widget_t *w, int abs_x, int abs_y);
bool zui_widget_handle_mouse(zui_widget_t *w, int abs_x, int abs_y, bool is_click);
void zui_widget_destroy(zui_widget_t *w);

#endif /* LIBZUI_ZUI_H */
