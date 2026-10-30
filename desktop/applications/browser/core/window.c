#include "browser.h"
#include "ui.h"
#include "renderer.h"

void browser_on_draw(struct window *win);
void browser_on_resize(struct window *win, int w, int h);
void browser_on_mouse(struct window *win, int mx, int my, bool left, bool right);
void browser_on_key(struct window *win, char key, uint8_t scancode);
void browser_on_close(struct window *win);

void browser_open_url(const char *url) {
    struct window *win = wm_create_window("Zarqaa Browser", 100, 100, 800, 600);
    if (!win) return;
    
    browser_init(win);
    
    win->on_draw = browser_on_draw;
    win->on_resize = browser_on_resize;
    win->on_mouse = browser_on_mouse;
    win->on_key = browser_on_key;
    win->on_close = browser_on_close;
    
    win->needs_redraw = true;
    
    browser_context_t *ctx = (browser_context_t *)win->user_data;
    if (url) {
        browser_load_url(ctx, url);
    } else {
        browser_load_url(ctx, "/hd0/www/index.html");
    }
}

void browser_open(void) {
    browser_open_url(NULL);
}
