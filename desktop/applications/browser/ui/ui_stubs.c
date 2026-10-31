#include "ui.h"
#include "browser.h"
#include "renderer.h"
#include "zarqaa/string.h"
#include "../platform/theme.h"
#include "../platform/canvas.h"
#include "../platform/scheduler.h"
#include "kernel/heap.h"

// Chrome-like UI Rewrite

void ui_tabbar_render(browser_context_t *ctx, canvas_t *canvas) {
    const browser_theme_t *theme = browser_theme_get_current();
    
    // Tab bar background
    canvas_fill_rect(canvas, 0, 0, ctx->win->width, 40, theme->bg_overlay);
    
    int tab_w = 180;
    int tab_h = 32;
    int x = 10;
    int y = 8;
    
    for (int i = 0; i < ctx->tab_count; i++) {
        browser_tab_t *tab = &ctx->tabs[i];
        const char *title = tab->history_count > 0 ? tab->history[tab->history_index].title : "New Tab";
        
        if (i == ctx->active_tab_index) {
            canvas_fill_rounded_rect(canvas, x, y, tab_w, tab_h, 8, theme->bg_surface);
            canvas_draw_text(canvas, x + 10, y + 10, title, theme->text_primary);
        } else {
            canvas_fill_rounded_rect(canvas, x, y, tab_w, tab_h, 8, theme->bg_overlay);
            canvas_draw_text(canvas, x + 10, y + 10, title, theme->text_secondary);
        }
        
        // Close button 'x'
        canvas_draw_text(canvas, x + tab_w - 20, y + 10, "x", theme->text_secondary);
        
        // Separator
        if (i != ctx->active_tab_index && i + 1 != ctx->active_tab_index) {
            canvas_draw_line(canvas, x + tab_w + 5, y + 8, x + tab_w + 5, y + tab_h - 8, theme->border_color);
        }
        
        x += tab_w + 10;
    }
    
    // New Tab Button (+)
    canvas_draw_text(canvas, x + 10, y + 10, "+", theme->text_primary);
}

void ui_omnibox_render(browser_context_t *ctx, canvas_t *canvas) {
    const browser_theme_t *theme = browser_theme_get_current();
    
    int bar_y = 40;
    int bar_h = 40;
    
    // Toolbar background
    canvas_fill_rect(canvas, 0, bar_y, ctx->win->width, bar_h, theme->bg_surface);
    canvas_draw_line(canvas, 0, bar_y + bar_h - 1, ctx->win->width, bar_y + bar_h - 1, theme->border_color);
    
    // Navigation Buttons
    bool can_back = false, can_forward = false;
    if (ctx->tab_count > 0) {
        browser_tab_t *tab = &ctx->tabs[ctx->active_tab_index];
        can_back = tab->history_index > 0;
        can_forward = tab->history_index < tab->history_count - 1;
    }
    
    color_t c_back = can_back ? theme->text_primary : theme->text_secondary;
    color_t c_forward = can_forward ? theme->text_primary : theme->text_secondary;
    
    int bx = 10;
    int by = bar_y + 10;
    
    // Back (<-)
    canvas_draw_line(canvas, bx + 5, by + 10, bx + 15, by, c_back);
    canvas_draw_line(canvas, bx + 5, by + 10, bx + 15, by + 20, c_back);
    canvas_draw_line(canvas, bx + 5, by + 10, bx + 20, by + 10, c_back);
    
    // Forward (->)
    bx += 35;
    canvas_draw_line(canvas, bx + 15, by + 10, bx + 5, by, c_forward);
    canvas_draw_line(canvas, bx + 15, by + 10, bx + 5, by + 20, c_forward);
    canvas_draw_line(canvas, bx + 15, by + 10, bx, by + 10, c_forward);
    
    // Refresh
    bx += 35;
    canvas_draw_line(canvas, bx + 5, by + 5, bx + 15, by + 5, theme->text_primary);
    canvas_draw_line(canvas, bx + 15, by + 5, bx + 15, by + 15, theme->text_primary);
    canvas_draw_line(canvas, bx + 15, by + 15, bx + 5, by + 15, theme->text_primary);
    canvas_draw_line(canvas, bx + 5, by + 15, bx + 5, by + 8, theme->text_primary);
    
    // Omnibox URL Area
    int url_x = bx + 35;
    int url_w = ctx->win->width - url_x - 50;
    
    canvas_fill_rounded_rect(canvas, url_x, bar_y + 6, url_w, 28, 14, theme->bg_overlay);
    
    int max_disp_w = url_w - 40;
    int display_start = 0;
    
    while (display_start < ctx->url_pos) {
        char temp_str[256];
        strncpy(temp_str, &ctx->url_bar_text[display_start], ctx->url_pos - display_start);
        temp_str[ctx->url_pos - display_start] = '\0';
        if (canvas_measure_text(temp_str) < max_disp_w) break;
        display_start++;
    }
    
    char display_text[256];
    strncpy(display_text, &ctx->url_bar_text[display_start], 255);
    display_text[255] = '\0';
    while (strlen(display_text) > 0 && canvas_measure_text(display_text) > max_disp_w) {
        display_text[strlen(display_text) - 1] = '\0';
    }
    
    canvas_draw_text(canvas, url_x + 15, bar_y + 12, display_text, theme->text_primary);
    
    // Cursor
    if (ctx->is_omnibox_focused) {
        char cursor_str[256];
        strncpy(cursor_str, &ctx->url_bar_text[display_start], ctx->url_pos - display_start);
        cursor_str[ctx->url_pos - display_start] = '\0';
        int text_w = canvas_measure_text(cursor_str);
        int cx = url_x + 15 + text_w;
        canvas_draw_line(canvas, cx, bar_y + 10, cx, bar_y + 30, theme->accent_primary);
    }
    
    // Bookmark Star
    int star_x = url_x + url_w - 25;
    bool is_bookmarked = false;
    if (ctx->tab_count > 0 && ctx->tabs[ctx->active_tab_index].history_count > 0) {
        char *curl = ctx->tabs[ctx->active_tab_index].history[ctx->tabs[ctx->active_tab_index].history_index].url;
        for (int i = 0; i < ctx->bookmark_count; i++) {
            if (strcmp(ctx->bookmarks[i].url, curl) == 0) {
                is_bookmarked = true; break;
            }
        }
    }
    color_t star_col = is_bookmarked ? 0xFFFFD700 : theme->text_secondary;
    canvas_draw_text(canvas, star_x, bar_y + 12, "*", star_col);
    
    // Progress Line
    if (ctx->tab_count > 0 && ctx->tabs[ctx->active_tab_index].is_loading) {
        static int progress_x = 0;
        progress_x = (progress_x + 6);
        if (progress_x > url_w + 100) progress_x = 0;
        
        int bar_w = 100;
        int px = url_x + progress_x - bar_w;
        if (px < url_x) px = url_x;
        int pw = progress_x > bar_w ? bar_w : progress_x;
        if (px + pw > url_x + url_w) pw = url_x + url_w - px;
        
        if (pw > 0) {
            canvas_fill_rounded_rect(canvas, px, bar_y + 34, pw, 2, 1, theme->accent_primary);
        }
        ctx->win->needs_redraw = true; // Animate
    }
}

void ui_statusbar_render(browser_context_t *ctx, canvas_t *canvas) {
    const browser_theme_t *theme = browser_theme_get_current();
    int y = ctx->win->height - ctx->statusbar_height;
    
    canvas_fill_rect(canvas, 0, y, ctx->win->width, ctx->statusbar_height, theme->bg_surface);
    canvas_draw_line(canvas, 0, y, ctx->win->width, y, theme->border_color);
    
    if (ctx->tab_count > 0 && ctx->tabs[ctx->active_tab_index].is_loading) {
        canvas_draw_text(canvas, 10, y + 5, "Loading...", theme->accent_primary);
    } else {
        canvas_draw_text(canvas, 10, y + 5, "Ready", 0xFFA6E3A1);
    }
}

void ui_draw_scrollbar(browser_context_t *ctx, canvas_t *canvas, int view_x, int view_y, int view_w, int view_h, int content_h) {
    const browser_theme_t *theme = browser_theme_get_current();
    if (content_h <= view_h) return;

    int bar_w = 12;
    int bar_x = view_x + view_w - bar_w - 6;
    float ratio = (float)view_h / (float)content_h;
    int thumb_h = (int)(view_h * ratio);
    if (thumb_h < 30) thumb_h = 30;
    
    browser_tab_t *tab = &ctx->tabs[ctx->active_tab_index];
    int max_scroll = content_h - view_h;
    if (max_scroll < 1) max_scroll = 1;
    
    int thumb_y = view_y + (int)(((float)tab->scroll_y / (float)max_scroll) * (view_h - thumb_h));

    // Track
    canvas_fill_rounded_rect(canvas, bar_x, view_y + 10, bar_w, view_h - 20, 6, theme->bg_overlay);
    // Thumb
    canvas_fill_rounded_rect(canvas, bar_x + 2, thumb_y + 2, bar_w - 4, thumb_h - 4, 4, theme->text_secondary);
}

void ui_handle_mouse_click(browser_context_t *ctx, int mx, int my) {
    bool was_focused = ctx->is_omnibox_focused;
    ctx->is_omnibox_focused = false;
    if (was_focused) ctx->win->needs_redraw = true;

    // 1. Tab Bar (Y: 0 - 40)
    if (my >= 0 && my < 40) {
        int tab_w = 180;
        int x = 10;
        
        for (int i = 0; i < ctx->tab_count; i++) {
            // Close button
            if (mx >= x + tab_w - 25 && mx <= x + tab_w && my >= 8 && my <= 40) {
                if (ctx->tab_count > 1) {
                    browser_tab_t *closing_tab = &ctx->tabs[i];
                    if (closing_tab->layout_root) browser_renderer_free_layout((render_node_t *)closing_tab->layout_root);
                    if (closing_tab->document) browser_renderer_destroy_document(closing_tab->document);
                    for (int j = i; j < ctx->tab_count - 1; j++) ctx->tabs[j] = ctx->tabs[j+1];
                    ctx->tab_count--;
                    if (ctx->active_tab_index >= ctx->tab_count) ctx->active_tab_index = ctx->tab_count - 1;
                    browser_render_current(ctx);
                }
                return;
            }
            // Tab switch
            if (mx >= x && mx <= x + tab_w) {
                ctx->active_tab_index = i;
                browser_tab_t *tab = &ctx->tabs[i];
                if (tab->history_index >= 0) {
                    strncpy(ctx->url_bar_text, tab->history[tab->history_index].url, 255);
                    ctx->url_pos = strlen(ctx->url_bar_text);
                } else {
                    ctx->url_bar_text[0] = '\0';
                    ctx->url_pos = 0;
                }
                ctx->win->needs_redraw = true;
                return;
            }
            x += tab_w + 10;
        }
        
        // New Tab
        if (mx >= x && mx <= x + 30) {
            if (ctx->tab_count < MAX_TABS) {
                int new_idx = ctx->tab_count;
                ctx->tabs[new_idx].id = ctx->next_tab_id++;
                ctx->tabs[new_idx].history_count = 0;
                ctx->tabs[new_idx].history_index = -1;
                ctx->tabs[new_idx].document = NULL;
                ctx->tabs[new_idx].layout_root = NULL;
                ctx->tabs[new_idx].scroll_y = 0;
                ctx->active_tab_index = new_idx;
                ctx->tab_count++;
                ctx->url_bar_text[0] = '\0';
                ctx->url_pos = 0;
                ctx->win->needs_redraw = true;
                browser_load_url(ctx, "zarqaa://start");
            }
        }
        return;
    }
    
    // 2. Toolbar (Y: 40 - 80)
    if (my >= 40 && my < 80) {
        int bx = 10;
        if (mx >= bx && mx <= bx + 30) { // Back
            if (ctx->tab_count > 0 && ctx->tabs[ctx->active_tab_index].history_index > 0) {
                ctx->tabs[ctx->active_tab_index].history_index--;
                browser_render_current(ctx);
            }
        } else if (mx >= bx + 35 && mx <= bx + 65) { // Forward
            if (ctx->tab_count > 0 && ctx->tabs[ctx->active_tab_index].history_index < ctx->tabs[ctx->active_tab_index].history_count - 1) {
                ctx->tabs[ctx->active_tab_index].history_index++;
                browser_render_current(ctx);
            }
        } else if (mx >= bx + 70 && mx <= bx + 100) { // Refresh
            browser_render_current(ctx);
        } else if (mx >= bx + 105 && mx <= ctx->win->width - 50) { // Omnibox
            ctx->is_omnibox_focused = true;
            ctx->win->needs_redraw = true;
        } else if (mx > ctx->win->width - 40) { // Bookmark
            browser_toggle_bookmark(ctx);
        }
        return;
    }
    
    // 3. Web Page Content (Y: 80 - Statusbar)
    int view_y = 80;
    int view_h = ctx->win->height - ctx->statusbar_height - view_y;
    
    if (my >= view_y && my <= view_y + view_h) {
        if (ctx->tab_count > 0) {
            browser_tab_t *tab = &ctx->tabs[ctx->active_tab_index];
            int content_h = tab->layout_root ? browser_renderer_get_document_height(tab->layout_root) : 0;
            if (content_h > view_h) {
                int bar_w = 12;
                int bar_x = ctx->win->width - bar_w - 6;
                if (mx >= bar_x && mx <= bar_x + bar_w) {
                    float ratio = (float)view_h / (float)content_h;
                    int thumb_h = (int)(view_h * ratio);
                    if (thumb_h < 30) thumb_h = 30;
                    int click_y = (my - view_y) - (thumb_h / 2);
                    if (click_y < 0) click_y = 0;
                    if (click_y > view_h - thumb_h) click_y = view_h - thumb_h;
                    tab->scroll_y = (int)(((float)click_y / (float)(view_h - thumb_h)) * (content_h - view_h));
                    ctx->win->needs_redraw = true;
                    return;
                }
            }
            
            if (tab->layout_root) {
                int content_x = mx;
                int content_y = my - view_y + tab->scroll_y;
                render_node_t *hit = browser_renderer_get_node_at(tab->layout_root, content_x, content_y);
                if (hit) {
                    const char *href = browser_renderer_get_href(hit);
                    if (href) {
                        browser_load_url(ctx, href);
                        return;
                    }
                }
            }
        }
    }
}

void ui_handle_keyboard(browser_context_t *ctx, uint32_t scancode, char ascii) {
    (void)scancode;
    if (ascii == 20) { // Ctrl+T
        if (ctx->tab_count < MAX_TABS) {
            int new_idx = ctx->tab_count;
            ctx->tabs[new_idx].id = ctx->next_tab_id++;
            ctx->tabs[new_idx].history_count = 0;
            ctx->tabs[new_idx].history_index = -1;
            ctx->tabs[new_idx].document = NULL;
            ctx->tabs[new_idx].layout_root = NULL;
            ctx->tabs[new_idx].scroll_y = 0;
            ctx->active_tab_index = new_idx;
            ctx->tab_count++;
            ctx->url_bar_text[0] = '\0';
            ctx->url_pos = 0;
            ctx->win->needs_redraw = true;
            browser_load_url(ctx, "zarqaa://start");
        }
        return;
    } else if (ascii == 23) { // Ctrl+W
        if (ctx->tab_count > 1) {
            int i = ctx->active_tab_index;
            browser_tab_t *closing_tab = &ctx->tabs[i];
            if (closing_tab->layout_root) browser_renderer_free_layout((render_node_t *)closing_tab->layout_root);
            if (closing_tab->document) browser_renderer_destroy_document(closing_tab->document);
            for (int j = i; j < ctx->tab_count - 1; j++) ctx->tabs[j] = ctx->tabs[j+1];
            ctx->tab_count--;
            if (ctx->active_tab_index >= ctx->tab_count) ctx->active_tab_index = ctx->tab_count - 1;
            browser_render_current(ctx);
        }
        return;
    } else if (ascii == 12) { // Ctrl+L
        ctx->url_pos = strlen(ctx->url_bar_text);
        ctx->is_omnibox_focused = true;
        ctx->win->needs_redraw = true;
        return;
    }
    
    if (!ctx->is_omnibox_focused) return;
    
    if (ascii == '\b' || ascii == 127) {
        if (ctx->url_pos > 0) {
            ctx->url_pos--;
            ctx->url_bar_text[ctx->url_pos] = '\0';
        }
    } else if (ascii == '\n' || ascii == '\r') {
        browser_load_url(ctx, ctx->url_bar_text);
    } else if (ascii >= 32 && ascii <= 126) {
        if (ctx->url_pos < 255) {
            ctx->url_bar_text[ctx->url_pos] = ascii;
            ctx->url_pos++;
            ctx->url_bar_text[ctx->url_pos] = '\0';
        }
    }
    ctx->win->needs_redraw = true;
}
