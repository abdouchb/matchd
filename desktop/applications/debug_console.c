#include "desktop/window_manager.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"

struct debug_state {
    char *log_buffer;
};

static void debug_on_draw(struct window *win) {
    struct debug_state *state = (struct debug_state *)win->user_data;
    
    // Set render target to window content buffer
    fb_set_render_target(win->content_buffer, win->width, win->height, win->width * 4);
    
    // Dark background
    fb_fill_rect(0, 0, win->width, win->height, 0xFF1E1E2E);
    
    // Read kernel logs
    int len = klog_read(state->log_buffer, 8192);
    
    // Find last 30 lines
    int line_count = 0;
    int start_idx = 0;
    for (int i = len - 1; i >= 0; i--) {
        if (state->log_buffer[i] == '\n') {
            line_count++;
            if (line_count > 30) {
                start_idx = i + 1;
                break;
            }
        }
    }
    
    int y = 10;
    int x = 10;
    for (int i = start_idx; i < len; i++) {
        if (state->log_buffer[i] == '\n') {
            y += 16;
            x = 10;
        } else if (state->log_buffer[i] != '\r') {
            char c[2] = {state->log_buffer[i], '\0'};
            fb_draw_string_transparent(x, y, c, 0xFFA6E3A1); // Green text
            x += 8;
        }
    }
    
    fb_reset_render_target();
    
    // Auto refresh continuously for live logs
    win->needs_redraw = true;
}

static void debug_on_close(struct window *win) {
    struct debug_state *state = (struct debug_state *)win->user_data;
    if (state) {
        if (state->log_buffer) kfree(state->log_buffer);
        kfree(state);
    }
}

void debug_console_open(void) {
    struct window *win = wm_create_window("Debug Console", 50, 50, 600, 500);
    if (!win) return;
    
    struct debug_state *state = kcalloc(1, sizeof(struct debug_state));
    state->log_buffer = kcalloc(1, 8192);
    win->user_data = state;
    
    win->on_draw = debug_on_draw;
    win->on_close = debug_on_close;
    
    // Note: To automatically update, we'd need a timer or just redraw it every frame
    // For now, wm_update doesn't automatically redraw unless needs_redraw is set
    win->needs_redraw = true;
}
