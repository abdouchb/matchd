#ifndef ZARQAA_BROWSER_H
#define ZARQAA_BROWSER_H

#include "zarqaa/types.h"
#include "desktop/window_manager.h"
#include "gui/surface.h"
#include "../platform/canvas.h"
#include "kernel/js_engine.h"

// Forward declarations
struct browser_tab;

#define MAX_TABS 10
#define MAX_HISTORY 100
#define MAX_BOOKMARKS 50
#define MAX_URL_LEN 256

typedef struct browser_page {
    char url[MAX_URL_LEN];
    char title[100];
} browser_page_t;

typedef struct browser_tab {
    int id;
    
    browser_page_t history[MAX_HISTORY];
    int history_count;
    int history_index;
    
    // Legacy fields mapped from original struct
    bool is_loading;
    int redirects;
    void *document;
    void *layout_root;
    int scroll_y;
    int max_scroll;
} browser_tab_t;

typedef struct browser_context {
    struct window *win;
    
    // UI Layout constraints
    int sidebar_width;
    int omnibox_width;
    int omnibox_height;
    int statusbar_height;
    
    // Platform objects
    canvas_t *canvas;
    
    // State
    browser_tab_t tabs[MAX_TABS];
    int tab_count;
    int active_tab_index;
    int next_tab_id;
    
    browser_page_t bookmarks[MAX_BOOKMARKS];
    int bookmark_count;
    
    char url_bar_text[MAX_URL_LEN];
    int url_pos;
    bool is_omnibox_focused;
    
    // Double Buffering
    void *back_buffer;
    int back_buffer_width;
    int back_buffer_height;
    
    // JS Engine
    duk_context *js_ctx;
} browser_context_t;

void browser_init(struct window *win);
void browser_destroy(browser_context_t *ctx);
void browser_load_url(browser_context_t *ctx, const char *url);
void browser_render_current(browser_context_t *ctx);
void browser_toggle_bookmark(browser_context_t *ctx);

#endif
