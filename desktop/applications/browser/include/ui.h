#ifndef ZARQAA_BROWSER_UI_H
#define ZARQAA_BROWSER_UI_H

#include "browser.h"
#include "gui/surface.h"
#include "../platform/canvas.h"

// UI Component rendering functions
void ui_tabbar_render(browser_context_t *ctx, canvas_t *canvas);
void ui_omnibox_render(browser_context_t *ctx, canvas_t *canvas);
void ui_statusbar_render(browser_context_t *ctx, canvas_t *canvas);
void ui_draw_scrollbar(browser_context_t *ctx, canvas_t *canvas, int view_x, int view_y, int view_w, int view_h, int content_h);

// UI Event handlers
void ui_handle_mouse_click(browser_context_t *ctx, int mx, int my);
void ui_handle_keyboard(browser_context_t *ctx, uint32_t scancode, char ascii);

#endif
