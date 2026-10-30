/*
 * Zarqaa OS 0.1 - Window Manager Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DESKTOP_WINDOW_MANAGER_H
#define DESKTOP_WINDOW_MANAGER_H

#include "zarqaa/types.h"
#include "gui/surface.h"

#define MAX_WINDOWS 32
#define TITLEBAR_HEIGHT 30

struct window {
    int id;
    char title[64];
    int x;
    int y;
    int width;
    int height;
    bool visible;
    bool minimized;
    bool maximized;
    bool focused;
    bool dragging;
    bool needs_redraw;
    bool frameless;
    int drag_offset_x;
    int drag_offset_y;
    
    int saved_x;
    int saved_y;
    int saved_w;
    int saved_h;

    uint32_t *content_buffer;
    surface_t surface;
    
    void (*on_draw)(struct window *win);
    void (*on_resize)(struct window *win, int w, int h);
    void (*on_key)(struct window *win, char key, uint8_t scancode);
    void (*on_mouse)(struct window *win, int mx, int my, bool left, bool right);
    void (*on_close)(struct window *win);
    void (*on_tick)(struct window *win);
    void *user_data;
};

void wm_init(void);
struct window *wm_create_window(const char *title, int x, int y, int w, int h);
void wm_destroy_window(struct window *win);
bool wm_update(bool left_clicked);
void wm_render(void);
struct window *wm_get_focused(void);
struct window *wm_find_window_by_title(const char *title);
void wm_focus_window(struct window *win);
int wm_get_window_count(void);

extern bool wm_enable_shadows;

void wm_set_clipboard(const char *text);
const char *wm_get_clipboard(void);
bool wm_has_maximized_window(void);

#endif /* DESKTOP_WINDOW_MANAGER_H */
