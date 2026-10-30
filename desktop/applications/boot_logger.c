/*
 * Zarqaa OS 0.1 - Smart Boot Logger
 * Creator: Abdelhak Chabbi
 */

#include "desktop/window_manager.h"
#include "gui/graphics.h"
#include "gui/renderer.h"
#include "drivers/gpu_renderer.h"
#include "drivers/font.h"
#include "kernel/syslog.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"
#include "drivers/framebuffer.h"

// Define colors matching QuantumPeak
#define QP_BG_DARK    0xFF070B12
#define QP_BG_PANEL   0xFF141F30
#define QP_TEXT_MAIN  0xFFFFFFFF
#define QP_TEXT_MUTED 0xFF80A0C4
#define QP_ERROR      0xFFFF4A4A
#define QP_WARN       0xFFFFAE00
#define QP_SUCCESS    0xFF00FF7F
#define QP_CYAN       0xFF00F0FF

#define SCROLLBAR_WIDTH 16

struct boot_logger_state {
    int scroll_y;
    int last_z;
    
    int sel_start_line;
    int sel_end_line;
    bool is_selecting;
    bool is_dragging_sb;
};

extern int klog_read(char *out_buf, int max_len);

static void boot_logger_on_draw(struct window *win) {
    if (!win || !win->content_buffer) return;
    struct boot_logger_state *state = (struct boot_logger_state *)win->user_data;

    surface_t surf = {0};
    surf.width = win->width;
    surf.height = win->height;
    surf.pitch = win->width * 4;
    surf.format = PIXEL_FORMAT_ARGB32;
    surf.cpu_pointer = win->content_buffer;

    // Draw Background
    renderer_execute_fill_rect(&surf, 0, 0, win->width, win->height, QP_BG_DARK);

    // Draw Top Header
    renderer_execute_fill_rect(&surf, 0, 0, win->width, 40, QP_BG_PANEL);

    renderer_execute_draw_text(&surf, 20, 15, "System Boot Logger (Smart Console)", QP_CYAN, 0, true);

    char *log_buffer = (char *)kmalloc(8192);
    if (!log_buffer) {
        renderer_execute_draw_text(&surf, 20, 60, "Out of memory.", QP_ERROR, 0, true);
        return;
    }

    int log_length = klog_read(log_buffer, 8192);
    if (log_length == 0) {
        renderer_execute_draw_text(&surf, 20, 60, "No logs available.", QP_TEXT_MUTED, 0, true);
        kfree(log_buffer);
        return;
    }

    int total_lines = 0;
    int y_pos = 50 - (state ? state->scroll_y : 0);
    char line_buf[256];
    int l_idx = 0;

    for (int i = 0; i < log_length; i++) {
        char c = log_buffer[i];
        if (c == '\n' || l_idx >= 255) {
            line_buf[l_idx] = '\0';
            
            // Only draw if visible
            if (y_pos >= 40 && y_pos < win->height) {
                int sel_min = state ? state->sel_start_line : -1;
                int sel_max = state ? state->sel_end_line : -1;
                if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }
                
                bool is_selected = (sel_min >= 0 && total_lines >= sel_min && total_lines <= sel_max);
                
                if (is_selected) {
                    renderer_execute_fill_rect(&surf, 10, y_pos, win->width - SCROLLBAR_WIDTH - 20, 16, 0xFF0078D7);
                }
                
                uint32_t color = is_selected ? 0xFFFFFFFF : QP_TEXT_MUTED;
                
                // Smart Highlighting (override if not selected)
                if (!is_selected) {
                    if (strstr(line_buf, "[ERROR]") || strstr(line_buf, "Panic") || strstr(line_buf, "failed") || strstr(line_buf, "FAILED")) {
                        color = QP_ERROR;
                    } else if (strstr(line_buf, "[WARN]") || strstr(line_buf, "Stub") || strstr(line_buf, "Invalid") || strstr(line_buf, "WARNING")) {
                        color = QP_WARN;
                    } else if (strstr(line_buf, "Success") || strstr(line_buf, "complete") || strstr(line_buf, "SUCCESS") || strstr(line_buf, "Found")) {
                        color = QP_SUCCESS;
                    } else if (strstr(line_buf, "[HDA]") || strstr(line_buf, "[AC97]") || strstr(line_buf, "[RTL8139]") || strstr(line_buf, "[ACPI]")) {
                        color = QP_CYAN;
                    }
                }

                renderer_execute_draw_text(&surf, 10, y_pos, line_buf, color, 0, true);
            }

            y_pos += 16; // Line height
            total_lines++;
            l_idx = 0;
        } else {
            line_buf[l_idx++] = c;
        }
    }
    
    kfree(log_buffer);

    if (state) {
        int max_scroll = (total_lines * 16) - (win->height - 50);
        if (max_scroll > 0) {
            int sb_w = SCROLLBAR_WIDTH;
            int sb_x = win->width - sb_w;
            int sb_y = 40;
            int sb_h = win->height - 40;
            
            // Track
            renderer_execute_fill_rect(&surf, sb_x, sb_y, sb_w, sb_h, 0xFF101926);
            
            // Thumb
            int thumb_h = (win->height * win->height) / ((total_lines * 16) + win->height);
            if (thumb_h < 24) thumb_h = 24;
            
            int thumb_y = sb_y + (state->scroll_y * (sb_h - thumb_h)) / max_scroll;
            renderer_execute_fill_rect(&surf, sb_x + 3, thumb_y, sb_w - 6, thumb_h, 0xFF3A86FF);
        }
    }
}

static void boot_logger_on_mouse_impl(struct window *win, int mx, int my, bool left_clicked, bool left_pressed, bool right) {
    (void)right;
    struct boot_logger_state *state = (struct boot_logger_state *)win->user_data;
    if (!state) return;
    
    // Count total lines safely
    int total_lines = 0;
    char *log_buffer = (char *)kmalloc(8192);
    if (log_buffer) {
        int log_length = klog_read(log_buffer, 8192);
        for (int i = 0; i < log_length; i++) {
            if (log_buffer[i] == '\n') total_lines++;
        }
        kfree(log_buffer);
    }

    int max_scroll = (total_lines * 16) - (win->height - 50);
    if (max_scroll < 0) max_scroll = 0;
    
    int sb_x = win->width - SCROLLBAR_WIDTH - 6; // Expanded hit area for scrollbar
    int sb_h = win->height - 40;
    int thumb_h = (win->height * win->height) / ((total_lines * 16) + win->height);
    if (thumb_h < 24) thumb_h = 24;

    // ── SCROLLBAR DRAG / CLICK HANDLING ──
    if (left_clicked && mx >= sb_x && my >= 40) {
        state->is_dragging_sb = true;
        state->is_selecting = false; // Never select text while clicking scrollbar
    }

    if (state->is_dragging_sb) {
        if (left_pressed || left_clicked) {
            int click_y = my - 40;
            if (sb_h > thumb_h && max_scroll > 0) {
                int new_scroll = ((click_y - thumb_h / 2) * max_scroll) / (sb_h - thumb_h);
                if (new_scroll < 0) new_scroll = 0;
                if (new_scroll > max_scroll) new_scroll = max_scroll;
                if (state->scroll_y != new_scroll) {
                    state->scroll_y = new_scroll;
                    win->needs_redraw = true;
                }
            }
        }
        return; // CRITICAL: Skip line selection completely while interacting with scrollbar!
    }

    // ── TEXT SELECTION HANDLING (ONLY IN CONTENT AREA) ──
    if (mx < sb_x && my >= 40) {
        int click_y = my - 50 + state->scroll_y;
        int line_idx = click_y / 16;
        if (line_idx < 0) line_idx = 0;
        if (line_idx > total_lines) line_idx = total_lines;
        
        if (left_clicked) {
            state->sel_start_line = line_idx;
            state->sel_end_line = line_idx;
            state->is_selecting = true;
            win->needs_redraw = true;
        } else if (left_pressed && state->is_selecting) {
            if (state->sel_end_line != line_idx) {
                state->sel_end_line = line_idx;
                win->needs_redraw = true;
            }
        }
    }
}

static void boot_logger_on_key(struct window *win, char key, uint8_t scancode) {
    (void)scancode;
    struct boot_logger_state *state = (struct boot_logger_state *)win->user_data;
    if (!state) return;
    
    extern bool keyboard_key_pressed(uint8_t scancode);
    bool ctrl = keyboard_key_pressed(0x1D);
    
    int sel_min = state->sel_start_line;
    int sel_max = state->sel_end_line;
    if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }
    
    if (ctrl && (key == 'c' || key == 'C')) {
        char *log_buffer = (char *)kmalloc(8192);
        if (log_buffer) {
            int log_length = klog_read(log_buffer, 8192);
            if (log_length > 0 && ((sel_min != sel_max) || (sel_min == sel_max && sel_min >= 0))) {
                char *clip = (char *)kmalloc(8192);
                if (clip) {
                    int clip_len = 0;
                    int curr_line = 0;
                    for (int i = 0; i < log_length && clip_len < 8190; i++) {
                        if (curr_line >= sel_min && curr_line <= sel_max) {
                            clip[clip_len++] = log_buffer[i];
                        }
                        if (log_buffer[i] == '\n') curr_line++;
                    }
                    clip[clip_len] = '\0';
                    extern void wm_set_clipboard(const char *text);
                    wm_set_clipboard(clip);
                    kfree(clip);
                }
            }
            kfree(log_buffer);
        }
    } else if (ctrl && (key == 'a' || key == 'A')) {
        char *log_buffer = (char *)kmalloc(8192);
        if (log_buffer) {
            int log_length = klog_read(log_buffer, 8192);
            int total_lines = 0;
            state->sel_start_line = 0;
            for (int i = 0; i < log_length; i++) {
                if (log_buffer[i] == '\n') total_lines++;
            }
            state->sel_end_line = total_lines;
            win->needs_redraw = true;
            kfree(log_buffer);
        }
    }
}

extern bool mouse_left_pressed(void);
static void boot_logger_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    struct boot_logger_state *state = (struct boot_logger_state *)win->user_data;
    if (!state) return;

    // Mouse wheel scrolling
    extern int mouse_get_z(void);
    int current_z = mouse_get_z();
    int dz = current_z - state->last_z;
    state->last_z = current_z;
    
    if (dz != 0) {
        state->scroll_y -= dz * 32;
        if (state->scroll_y < 0) state->scroll_y = 0;
        win->needs_redraw = true;
    }

    bool is_left_down = left || mouse_left_pressed();
    if (is_left_down) {
        boot_logger_on_mouse_impl(win, mx, my, left, mouse_left_pressed(), right);
    } else {
        state->is_dragging_sb = false;
        state->is_selecting = false;
    }
}

static void boot_logger_on_close(struct window *win) {
    if (win && win->user_data) {
        kfree(win->user_data);
        win->user_data = NULL;
    }
}

struct window *boot_logger_open(void) {
    int w = 700;
    int h = 500;
    int screen_w = fb_get_width();
    int screen_h = fb_get_height();
    int x = (screen_w - w) / 2;
    int y = (screen_h - h) / 2;

    struct window *win = wm_create_window("Boot Logger", x, y, w, h);
    if (!win) return NULL;

    struct boot_logger_state *state = (struct boot_logger_state *)kmalloc(sizeof(struct boot_logger_state));
    if (state) {
        memset(state, 0, sizeof(struct boot_logger_state));
        state->scroll_y = 0;
        extern int mouse_get_z(void);
        state->last_z = mouse_get_z();
        state->sel_start_line = -1;
        state->sel_end_line = -1;
        state->is_selecting = false;
        state->is_dragging_sb = false;
        win->user_data = state;
    }

    win->on_draw = boot_logger_on_draw;
    win->on_mouse = boot_logger_on_mouse;
    win->on_key = boot_logger_on_key;
    win->on_close = boot_logger_on_close;

    return win;
}
