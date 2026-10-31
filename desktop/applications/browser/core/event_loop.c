#include "ui.h"
#include "renderer.h"
#include "../platform/canvas.h"
#include "../platform/theme.h"
#include "../platform/scheduler.h"
#include "zarqaa/string.h"
#include "drivers/framebuffer.h"

void browser_on_draw(struct window *win) {
    browser_context_t *ctx = (browser_context_t *)win->user_data;
    if (!ctx) return;
    
    canvas_t *canvas = ctx->canvas;
    
    if (canvas && canvas->target_surface) {
        if (ctx->back_buffer_width != win->width || ctx->back_buffer_height != win->height) {
            if (ctx->back_buffer) kfree(ctx->back_buffer);
            ctx->back_buffer = kmalloc(win->width * win->height * 4);
            ctx->back_buffer_width = win->width;
            ctx->back_buffer_height = win->height;
        }

        if ((int)canvas->target_surface->width != win->width || (int)canvas->target_surface->height != win->height) {
            browser_scheduler_invalidate_layout();
            browser_scheduler_invalidate_paint();
        }
        
        canvas->target_surface->cpu_pointer = ctx->back_buffer; // Draw to back buffer
        canvas->target_surface->pixels = ctx->back_buffer;
        canvas->target_surface->width = win->width;
        canvas->target_surface->height = win->height;
        canvas->target_surface->pitch = win->width * 4;
        canvas->target_surface->format = PIXEL_FORMAT_ARGB32;
    }
    
    if (ctx->back_buffer) {
        fb_set_render_target(ctx->back_buffer, win->width, win->height, win->width * 4);
    }

    canvas_begin(canvas);
    
    const browser_theme_t *theme = browser_theme_get_current();
    
    // Render Web Page Content
    int view_x = 0;
    int view_y = 80; // Start below omnibox
    int view_w = win->width;
    int view_h = win->height - ctx->statusbar_height - 80;
    
    // Fill ONLY chrome areas with dark theme color (toolbar + statusbar)
    canvas_fill_rect(canvas, 0, 0, win->width, view_y, theme->bg_base);
    canvas_fill_rect(canvas, 0, view_y + view_h, win->width, ctx->statusbar_height + 10, theme->bg_base);
    
    // Pre-fill the page content area with white (default page background).
    canvas_fill_rect(canvas, 0, view_y, win->width, view_h, 0xFFFFFFFF);
    
    if (ctx->tab_count > 0) {
        browser_tab_t *active_tab = &ctx->tabs[ctx->active_tab_index];
        if (active_tab->is_loading) {
            /* Loading indicator */
            canvas_fill_rect(canvas, win->width/2 - 60, view_y + view_h/2 - 12,
                             120, 24, 0xFF2196F3);
        } else if (active_tab->layout_root) {
            if (browser_scheduler_needs_layout()) {
                browser_renderer_compute_layout(active_tab->layout_root, view_x, view_y, view_w);
                browser_scheduler_clear_layout_flag();
            }
            // Pass fully initialized surface to renderer
            surface_t tmp_surf;
            memset(&tmp_surf, 0, sizeof(tmp_surf));
            tmp_surf.width = win->width;
            tmp_surf.height = win->height;
            tmp_surf.pitch = win->width * 4;
            tmp_surf.format = PIXEL_FORMAT_ARGB32;
            tmp_surf.cpu_pointer = ctx->back_buffer;
            tmp_surf.pixels = ctx->back_buffer;
            
            browser_renderer_paint(&tmp_surf, active_tab->layout_root, active_tab->scroll_y, view_y, view_h);
        }
    }

    // Render UI Components LAST (on top)
    ui_tabbar_render(ctx, canvas);
    ui_omnibox_render(ctx, canvas);
    ui_statusbar_render(ctx, canvas);
    
    if (ctx->tab_count > 0) {
        browser_tab_t *active_tab = &ctx->tabs[ctx->active_tab_index];
        if (active_tab->layout_root) {
            int content_h = browser_renderer_get_document_height(active_tab->layout_root);
            ui_draw_scrollbar(ctx, canvas, view_x, view_y, view_w, view_h, content_h);
        }
    }
    
    canvas_end(canvas);
    browser_scheduler_clear_paint_flag();
    
    // Copy the back buffer to the window's content buffer
    if (ctx->back_buffer && win->content_buffer) {
        memcpy(win->content_buffer, ctx->back_buffer, win->width * win->height * 4);
    }
}

void browser_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    browser_context_t *ctx = (browser_context_t *)win->user_data;
    if (!ctx) return;
    
    // 'left' comes from mouse_left_clicked() via wm_update — it is already
    // a one-shot flag that is true for exactly one frame per physical click.
    // No additional debounce is needed; adding one was the root cause of
    // clicks being silently swallowed.
    if (left) {
        ui_handle_mouse_click(ctx, mx, my);
        win->needs_redraw = true;
    }
}

void browser_on_key(struct window *win, char key, uint8_t scancode) {
    // scancode & 0x80 indicates key-release in raw PS/2; wm currently
    // always passes 0, so this guard is a no-op but kept for safety.
    if (scancode & 0x80) {
        return;
    }
    
    browser_context_t *ctx = (browser_context_t *)win->user_data;
    if (!ctx) return;
    
    ui_handle_keyboard(ctx, scancode, key);
    // Always mark redraw so typed characters / cursor appear immediately
    win->needs_redraw = true;
}

void browser_on_close(struct window *win) {
    (void)win;
    // Called when the window is closed
}

void browser_on_resize(struct window *win, int w, int h) {
    browser_context_t *ctx = (browser_context_t *)win->user_data;
    if (!ctx) return;
    
    if (ctx->canvas && ctx->canvas->target_surface) {
        surface_t *s = ctx->canvas->target_surface;
        s->width = w;
        s->height = h;
        s->pitch = w * 4;
        s->cpu_pointer = win->content_buffer;
    }
    
    win->needs_redraw = true;
    browser_scheduler_invalidate_layout();
    browser_scheduler_invalidate_paint();
}
