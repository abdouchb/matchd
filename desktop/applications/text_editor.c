/*

Zarqaa OS 0.1 - Notepad Reimagined (Enhanced Version)

Creator: Abdelhak Chabbi

Enhanced with Line Numbers, Mouse Positioning, Scrollbar, and Adv. Navigation.
*/

#include "desktop/window_manager.h"
#include "drivers/framebuffer.h"
#include "gui/graphics.h"
#include "filesystem/vfs.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"
#include "drivers/pit.h"
#include "drivers/font.h"

#define MARGIN_LEFT 40
#define TOP_BAR_HEIGHT 40
#define MARGIN_TOP 10

enum font_style {
STYLE_NORMAL,
STYLE_BOLD,
STYLE_SHADOWED
};

struct editor_state {
char text_buffer[8192];
int text_length;
int cursor_pos;
char filename[64];
bool modified;
bool show_saved_msg;
int saved_msg_timer;

int sel_start;
int sel_end;
bool is_selecting;

int zoom_level;
enum font_style style;

bool search_active;
char search_buffer[64];
int search_length;

int scroll_y;
int total_height;

    bool show_context_menu;
    int context_menu_x;
    int context_menu_y;
};

// Helper function to draw integers (like line numbers)
static void draw_number(int num, int x, int y, color_t color) {
char buf[16];
int idx = 14;
buf[15] = '\0';
if (num == 0) {
buf[idx--] = '0';
} else {
while (num > 0 && idx >= 0) {
buf[idx--] = '0' + (num % 10);
num /= 10;
}
}
fb_draw_string_transparent(x, y, &buf[idx + 1], color);
}

static void editor_recalculate_scroll(struct window *win, struct editor_state *state) {
int cx = MARGIN_LEFT + 10;
int cy = TOP_BAR_HEIGHT + MARGIN_TOP;

int char_w = FONT_WIDTH * state->zoom_level;
int char_h = FONT_HEIGHT * state->zoom_level;

int cursor_y = cy;

for (int i = 0; i < state->text_length; i++) {
    if (i == state->cursor_pos) cursor_y = cy;
    
    if (state->text_buffer[i] == '\n') {
        cy += char_h;
        cx = MARGIN_LEFT + 10;
    } else {
        cx += char_w;
        if (cx + char_w > win->width - 20) { // Keep 20px padding for scrollbar
            cy += char_h;
            cx = MARGIN_LEFT + 10;
        }
    }
}
if (state->cursor_pos == state->text_length) cursor_y = cy;

// Save total height for scrollbar calculation
state->total_height = (cy - TOP_BAR_HEIGHT) + char_h + 20;

int actual_y = cursor_y - state->scroll_y;
int visible_start = TOP_BAR_HEIGHT + MARGIN_TOP;

if (actual_y < visible_start) {
    state->scroll_y -= (visible_start - actual_y);
}
if (actual_y + char_h > win->height - 10) {
    state->scroll_y += (actual_y + char_h - (win->height - 10));
}
if (state->scroll_y < 0) state->scroll_y = 0;


}

static void editor_draw(struct window *win) {
struct editor_state *state = (struct editor_state *)win->user_data;

// Main Background
fb_fill_rect(0, 0, win->width, win->height, 0xFFF0F5F9);

// Line Numbers Margin Background
fb_fill_rect(0, TOP_BAR_HEIGHT, MARGIN_LEFT, win->height - TOP_BAR_HEIGHT, 0xFFE2E8F0);
fb_draw_line(MARGIN_LEFT, TOP_BAR_HEIGHT, MARGIN_LEFT, win->height, 0xFFCBD5E1);

// Text rendering
int cx = MARGIN_LEFT + 10;
int cy = TOP_BAR_HEIGHT + MARGIN_TOP - state->scroll_y;
int cursor_draw_x = cx;
int cursor_draw_y = cy;

int char_w = FONT_WIDTH * state->zoom_level;
int char_h = FONT_HEIGHT * state->zoom_level;

int line_num = 1;
bool draw_ln = true;

for (int i = 0; i <= state->text_length; i++) {
    if (i == state->cursor_pos) {
        cursor_draw_x = cx;
        cursor_draw_y = cy;
    }
    
    if (i == state->text_length) break;

    // Draw Line Number if it's visible
    if (draw_ln && cy > TOP_BAR_HEIGHT && cy < win->height) {
        draw_number(line_num, 5, cy + (char_h/2 - FONT_HEIGHT/2), 0xFF94A3B8);
        draw_ln = false;
    }
    
    char c = state->text_buffer[i];
    
    bool is_highlighted = false;
    if (state->search_active && state->search_length > 0) {
        for (int offset = 0; offset < state->search_length; offset++) {
            int start = i - offset;
            if (start >= 0 && start + state->search_length <= state->text_length) {
                bool match = true;
                for (int j = 0; j < state->search_length; j++) {
                    if (state->text_buffer[start + j] != state->search_buffer[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    is_highlighted = true;
                    break;
                }
            }
        }
    }
    
    int sel_min = state->sel_start;
    int sel_max = state->sel_end;
    if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }
    bool is_selected = (i >= sel_min && i < sel_max);
    
    if (c == '\n') {
        cy += char_h;
        cx = MARGIN_LEFT + 10;
        line_num++;
        draw_ln = true;
    } else {
        if (cy + char_h > TOP_BAR_HEIGHT && cy < win->height) {
            if (is_selected) {
                fb_fill_rect(cx, cy, char_w, char_h, 0xFF0078D7); // Windows Blue Highlight
            } else if (is_highlighted) {
                fb_fill_rect(cx, cy, char_w, char_h, COLOR_WARNING);
            }
            
            const uint8_t *glyph = font_get_glyph(c);
            for (int g_y = 0; g_y < FONT_HEIGHT; g_y++) {
                uint8_t row = glyph[g_y];
                for (int g_x = 0; g_x < FONT_WIDTH; g_x++) {
                    if (row & (1 << (FONT_WIDTH - 1 - g_x))) {
                        int px = cx + g_x * state->zoom_level;
                        int py = cy + g_y * state->zoom_level;
                        
                        color_t fg = (is_highlighted || is_selected) ? 0xFF000000 : 0xFF223344;
                        
                        if (state->style == STYLE_SHADOWED) {
                            fb_fill_rect(px + 1, py + 1, state->zoom_level, state->zoom_level, 0xFF8899AA);
                        }
                        
                        fb_fill_rect(px, py, state->zoom_level, state->zoom_level, fg);
                        
                        if (state->style == STYLE_BOLD) {
                            fb_fill_rect(px + 1, py, state->zoom_level, state->zoom_level, fg);
                        }
                    }
                }
            }
        }
        
        cx += char_w;
        if (cx + char_w > win->width - 20) {
            cy += char_h;
            cx = MARGIN_LEFT + 10;
            // Wrapping does not increase logical line_num, but requires line number reset
        }
    }
}

// Draw cursor
if (win->focused && (pit_get_ticks() % 100) < 50 && !state->search_active) {
    if (cursor_draw_y >= TOP_BAR_HEIGHT && cursor_draw_y < win->height) {
        fb_fill_rect(cursor_draw_x, cursor_draw_y, 2, char_h, 0xFF000000);
    }
}

// Draw Visual Scrollbar
int visible_height = win->height - TOP_BAR_HEIGHT;
if (state->total_height > visible_height && state->total_height > 0) {
    int scrollbar_h = (visible_height * visible_height) / state->total_height;
    if (scrollbar_h < 20) scrollbar_h = 20;
    int scrollbar_y = TOP_BAR_HEIGHT + (state->scroll_y * visible_height) / state->total_height;
    
    // Prevent scrollbar from overflowing bottom
    if (scrollbar_y + scrollbar_h > win->height - 2) {
        scrollbar_y = win->height - scrollbar_h - 2;
    }
    
    gfx_draw_rounded_rect_filled(win->width - 12, scrollbar_y, 8, scrollbar_h, 4, 0xFF94A3B8);
}

// Modern Glassmorphism Toolbar
gfx_draw_glass_panel(0, 0, win->width, TOP_BAR_HEIGHT, 0, 0xFFFFFFFF, 0xFFCBD5E1, 200);
fb_draw_line(0, TOP_BAR_HEIGHT, win->width, TOP_BAR_HEIGHT, 0xFFCBD5E1);

// Save Button
gfx_draw_rounded_rect_filled(10, 8, 60, 24, 6, state->modified ? COLOR_ACCENT : COLOR_SUCCESS);
fb_draw_string_transparent(22, 12, "Save", 0xFFFFFFFF);

// Zoom Buttons
gfx_draw_rounded_rect_filled(80, 8, 30, 24, 6, 0xFFE2E8F0);
fb_draw_string_transparent(91, 12, "-", 0xFF000000);

gfx_draw_rounded_rect_filled(115, 8, 30, 24, 6, 0xFFE2E8F0);
fb_draw_string_transparent(126, 12, "+", 0xFF000000);

// Style Button
gfx_draw_rounded_rect_filled(155, 8, 70, 24, 6, 0xFFE2E8F0);
const char *s_text = state->style == STYLE_NORMAL ? "Normal" : (state->style == STYLE_BOLD ? "Bold" : "Shadow");
fb_draw_string_transparent(165, 12, s_text, 0xFF000000);

// Search Button
gfx_draw_rounded_rect_filled(235, 8, 70, 24, 6, state->search_active ? COLOR_WARNING : 0xFFE2E8F0);
fb_draw_string_transparent(245, 12, "Search", state->search_active ? 0xFFFFFFFF : 0xFF000000);

// Search Bar Overlay
if (state->search_active) {
    int sb_x = win->width - 160;
    if (sb_x < 315) sb_x = 315;
    gfx_draw_rounded_rect_filled(sb_x, 8, 150, 24, 6, 0xFFFFFFFF);
    gfx_draw_rounded_rect(sb_x, 8, 150, 24, 6, COLOR_ACCENT);
    fb_draw_string_transparent(sb_x + 10, 12, state->search_buffer, 0xFF000000);
    
    if (win->focused && (pit_get_ticks() % 100) < 50) {
        int sw = state->search_length * 8;
        fb_fill_rect(sb_x + 10 + sw, 10, 2, 20, 0xFF000000);
    }
}

    // Saved message popup
    if (state->show_saved_msg) {
        if (pit_get_ticks() - state->saved_msg_timer > 200) {
            state->show_saved_msg = false;
        } else {
            gfx_draw_rounded_rect_filled(win->width / 2 - 60, win->height - 40, 120, 30, 15, 0xDD000000);
            fb_draw_string_transparent(win->width / 2 - 40, win->height - 33, "File Saved", 0xFFFFFFFF);
        }
        win->needs_redraw = true; // keep animating popup
    }

    // Draw context menu if active
    if (state->show_context_menu) {
        int mw = 120;
        int mh = 100;
        int mx = state->context_menu_x;
        int my = state->context_menu_y;
        if (mx + mw > win->width) mx = win->width - mw;
        if (my + mh > win->height) my = win->height - mh;
        
        // Draw Glassmorphism-style background
        gfx_draw_rounded_rect_filled(mx, my, mw, mh, 6, 0xF2F9FAFC);
        
        // Draw items
        fb_draw_string_transparent(mx + 15, my + 15, "Copy", 0xFF000000);
        fb_draw_string_transparent(mx + 15, my + 45, "Paste", 0xFF000000);
        fb_draw_string_transparent(mx + 15, my + 75, "Delete", 0xFF000000);
        
        // Draw simple line dividers
        fb_fill_rect(mx + 5, my + 35, mw - 10, 1, 0xFFE2E8F0);
        fb_fill_rect(mx + 5, my + 65, mw - 10, 1, 0xFFE2E8F0);
        
        // Draw border last
        gfx_draw_rounded_rect(mx, my, mw, mh, 6, 0xFFCBD5E1);
    }
}

static void editor_on_key(struct window *win, char key, uint8_t scancode) {
struct editor_state *state = (struct editor_state *)win->user_data;
extern bool keyboard_key_pressed(uint8_t scancode);
bool shift = keyboard_key_pressed(0x2A) || keyboard_key_pressed(0x36);
bool ctrl = keyboard_key_pressed(0x1D);
int sel_min = state->sel_start;
int sel_max = state->sel_end;

if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }

if (state->search_active) {
    if (key == '\b') {
        if (state->search_length > 0) {
            state->search_length--;
            state->search_buffer[state->search_length] = '\0';
        }
    } else if (key >= 32 && key <= 126) {
        if (state->search_length < 63) {
            state->search_buffer[state->search_length++] = key;
            state->search_buffer[state->search_length] = '\0';
        }
    }
    win->needs_redraw = true;
    return;
}

if (ctrl && (key == 'c' || key == 'C')) {
    if (sel_min != sel_max) {
        int len = sel_max - sel_min;
        if (len > 8190) len = 8190;
        char *clip = kmalloc(len + 1);
        memcpy(clip, &state->text_buffer[sel_min], len);
        clip[len] = '\0';
        wm_set_clipboard(clip);
        kfree(clip);
    }
    return;
}

if (ctrl && (key == 'v' || key == 'V')) {
    const char *clip = wm_get_clipboard();
    if (clip && strlen(clip) > 0) {
        if (sel_min != sel_max) {
            int remove_len = sel_max - sel_min;
            memmove(&state->text_buffer[sel_min], &state->text_buffer[sel_max], state->text_length - sel_max + 1);
            state->text_length -= remove_len;
            state->cursor_pos = sel_min;
            state->sel_start = state->cursor_pos;
            state->sel_end = state->cursor_pos;
        }
        
        int insert_len = strlen(clip);
        if (state->text_length + insert_len < 8191) {
            memmove(&state->text_buffer[state->cursor_pos + insert_len], &state->text_buffer[state->cursor_pos], state->text_length - state->cursor_pos + 1);
            memcpy(&state->text_buffer[state->cursor_pos], clip, insert_len);
            state->cursor_pos += insert_len;
            state->text_length += insert_len;
            state->sel_start = state->cursor_pos;
            state->sel_end = state->cursor_pos;
            state->modified = true;
        }
    }
    editor_recalculate_scroll(win, state);
    win->needs_redraw = true;
    return;
}

if (ctrl && (key == 'a' || key == 'A')) {
    state->sel_start = 0;
    state->sel_end = state->text_length;
    state->cursor_pos = state->text_length;
    win->needs_redraw = true;
    return;
}

if (key == '\b') { // Backspace
    if (sel_min != sel_max) {
        int remove_len = sel_max - sel_min;
        memmove(&state->text_buffer[sel_min], &state->text_buffer[sel_max], state->text_length - sel_max + 1);
        state->text_length -= remove_len;
        state->cursor_pos = sel_min;
        state->sel_start = state->cursor_pos;
        state->sel_end = state->cursor_pos;
        state->modified = true;
    } else if (state->cursor_pos > 0) {
        for (int i = state->cursor_pos - 1; i < state->text_length; i++) {
            state->text_buffer[i] = state->text_buffer[i + 1];
        }
        state->cursor_pos--;
        state->text_length--;
        state->sel_start = state->cursor_pos;
        state->sel_end = state->cursor_pos;
        state->modified = true;
    }
} else if ((key >= 32 && key <= 126) || key == '\n') { // Typing chars
    if (sel_min != sel_max) {
        int remove_len = sel_max - sel_min;
        memmove(&state->text_buffer[sel_min], &state->text_buffer[sel_max], state->text_length - sel_max + 1);
        state->text_length -= remove_len;
        state->cursor_pos = sel_min;
    }

    if (state->text_length < 8191) {
        for (int i = state->text_length; i > state->cursor_pos; i--) {
            state->text_buffer[i] = state->text_buffer[i - 1];
        }
        state->text_buffer[state->cursor_pos] = key;
        state->cursor_pos++;
        state->text_length++;
        state->text_buffer[state->text_length] = '\0';
        state->sel_start = state->cursor_pos;
        state->sel_end = state->cursor_pos;
        state->modified = true;
    }
} else if (key == '\0') {
    if (scancode == 0x4B) { // Left arrow
        if (state->cursor_pos > 0) state->cursor_pos--;
    } else if (scancode == 0x4D) { // Right arrow
        if (state->cursor_pos < state->text_length) state->cursor_pos++;
    } else if (scancode == 0x48) { // Up arrow
        int col = 0;
        int p = state->cursor_pos - 1;
        while (p >= 0 && state->text_buffer[p] != '\n') { col++; p--; }
        if (p >= 0) {
            int prev_start = p - 1;
            while (prev_start >= 0 && state->text_buffer[prev_start] != '\n') { prev_start--; }
            prev_start++;
            int prev_len = p - prev_start;
            state->cursor_pos = prev_start + (col > prev_len ? prev_len : col);
        }
    } else if (scancode == 0x50) { // Down arrow
        int col = 0;
        int p = state->cursor_pos - 1;
        while (p >= 0 && state->text_buffer[p] != '\n') { col++; p--; }
        int next_start = state->cursor_pos;
        while (next_start < state->text_length && state->text_buffer[next_start] != '\n') { next_start++; }
        if (next_start < state->text_length) {
            next_start++;
            int next_end = next_start;
            while (next_end < state->text_length && state->text_buffer[next_end] != '\n') { next_end++; }
            int next_len = next_end - next_start;
            state->cursor_pos = next_start + (col > next_len ? next_len : col);
        }
    } else if (scancode == 0x47) { // Home key
        int p = state->cursor_pos;
        while (p > 0 && state->text_buffer[p - 1] != '\n') p--;
        state->cursor_pos = p;
    } else if (scancode == 0x4F) { // End key
        int p = state->cursor_pos;
        while (p < state->text_length && state->text_buffer[p] != '\n') p++;
        state->cursor_pos = p;
    } else if (scancode == 0x53) { // Delete key
        if (sel_min != sel_max) {
            int remove_len = sel_max - sel_min;
            memmove(&state->text_buffer[sel_min], &state->text_buffer[sel_max], state->text_length - sel_max + 1);
            state->text_length -= remove_len;
            state->cursor_pos = sel_min;
            state->sel_start = state->cursor_pos;
            state->sel_end = state->cursor_pos;
            state->modified = true;
        } else if (state->cursor_pos < state->text_length) {
            for (int i = state->cursor_pos; i < state->text_length; i++) {
                state->text_buffer[i] = state->text_buffer[i + 1];
            }
            state->text_length--;
            state->modified = true;
        }
    }
    
    if (scancode >= 0x47 && scancode <= 0x53) { // Arrow keys etc
        if (shift) {
            state->sel_end = state->cursor_pos;
        } else {
            state->sel_start = state->cursor_pos;
            state->sel_end = state->cursor_pos;
        }
    }
}

editor_recalculate_scroll(win, state);
win->needs_redraw = true;


}

static void editor_on_mouse(struct window *win, int mx, int my, bool left_clicked, bool left_pressed, bool right) {
(void)right;
if (!left_clicked && !left_pressed) return;
struct editor_state *state = (struct editor_state *)win->user_data;

// Toolbar clicks
if (left_clicked && my >= 8 && my <= 32) {
    if (mx >= 10 && mx <= 70) {
        struct vfs_node *file = vfs_open(state->filename);
        if (!file) file = vfs_create(state->filename, VFS_FILE);
        if (file) {
            vfs_write(file, state->text_buffer, state->text_length);
            state->modified = false;
            state->show_saved_msg = true;
            state->saved_msg_timer = pit_get_ticks();
        }
    } else if (mx >= 80 && mx <= 110) {
        if (state->zoom_level > 1) {
            state->zoom_level--;
            editor_recalculate_scroll(win, state);
        }
    } else if (mx >= 115 && mx <= 145) {
        if (state->zoom_level < 4) {
            state->zoom_level++;
            editor_recalculate_scroll(win, state);
        }
    } else if (mx >= 155 && mx <= 225) {
        state->style = (state->style + 1) % 3;
    } else if (mx >= 235 && mx <= 305) {
        state->search_active = !state->search_active;
        if (!state->search_active) state->search_length = 0;
    }
    win->needs_redraw = true;
} 
// Editor Text Area Clicks (Move Cursor)
else if ((left_clicked || left_pressed) && my > TOP_BAR_HEIGHT) {
    // Check if clicking on the scrollbar track
    if (mx >= win->width - 20) {
        int visible_height = win->height - TOP_BAR_HEIGHT;
        if (state->total_height > visible_height) {
            int max_scroll = state->total_height - visible_height;
            // Center the scrollbar thumb on the mouse cursor
            int new_scroll = ((my - TOP_BAR_HEIGHT) * state->total_height) / visible_height - (visible_height / 2);
            if (new_scroll < 0) new_scroll = 0;
            if (new_scroll > max_scroll) new_scroll = max_scroll;
            state->scroll_y = new_scroll;
            win->needs_redraw = true;
        }
        return; // Don't trigger text selection
    }

    int cx = MARGIN_LEFT + 10;
    int cy = TOP_BAR_HEIGHT + MARGIN_TOP - state->scroll_y;
    int char_w = FONT_WIDTH * state->zoom_level;
    int char_h = FONT_HEIGHT * state->zoom_level;
    
    int best_pos = state->text_length;
    
    if (my < cy) {
        best_pos = 0; // dragged above the text
    } else {
        int best_diff = 10000;
        int current_line_cy = cy;
        bool found_line = false;
        int line_end_pos = 0;
        
        for (int i = 0; i <= state->text_length; i++) {
            // If the mouse's Y coordinate falls within this line
            if (my >= cy && my < cy + char_h) {
                found_line = true;
                int diff = mx - cx;
                if (diff < 0) diff = -diff;
                
                // Track closest horizontal character
                if (diff < best_diff) {
                    best_diff = diff;
                    best_pos = i;
                }
            } else if (found_line) {
                // We've passed the line we clicked on
                break; 
            }
            
            line_end_pos = i;
            if (i < state->text_length) {
                if (state->text_buffer[i] == '\n') {
                    cy += char_h;
                    cx = MARGIN_LEFT + 10;
                } else {
                    cx += char_w;
                    if (cx + char_w > win->width - 20) {
                        cy += char_h;
                        cx = MARGIN_LEFT + 10;
                    }
                }
            }
        }
        
        // If we didn't find a line (dragged below text), use the last character position
        if (!found_line) {
            best_pos = state->text_length;
        }
    }
    
    state->cursor_pos = best_pos;
    if (!state->is_selecting) {
        state->sel_start = best_pos;
        state->is_selecting = true;
    }
    state->sel_end = best_pos;
    
    editor_recalculate_scroll(win, state);
    win->needs_redraw = true;
}
}

extern bool mouse_left_pressed(void);
static void editor_on_mouse_wrapper(struct window *win, int mx, int my, bool left, bool right) {
    struct editor_state *state = (struct editor_state *)win->user_data;
    
    if (right && !left) {
        state->show_context_menu = true;
        state->context_menu_x = mx;
        state->context_menu_y = my;
        win->needs_redraw = true;
        return;
    }
    
    bool is_left_down = left || mouse_left_pressed();
    if (is_left_down) {
        if (state->show_context_menu) {
            if (left) { // only handle clicks on context menu, not drags
                int mw = 120;
                int mh = 100;
                int cx = state->context_menu_x;
                int cy = state->context_menu_y;
                if (cx + mw > win->width) cx = win->width - mw;
                if (cy + mh > win->height) cy = win->height - mh;
                
                if (mx >= cx && mx <= cx + mw && my >= cy && my <= cy + mh) {
                    int item_y = my - cy;
                    int sel_min = state->sel_start;
                    int sel_max = state->sel_end;
                    if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }
                    
                    if (item_y < 35) {
                        // Copy
                        if (sel_min != sel_max) {
                            int len = sel_max - sel_min;
                            if (len > 8190) len = 8190;
                            char *clip = kmalloc(len + 1);
                            memcpy(clip, &state->text_buffer[sel_min], len);
                            clip[len] = '\0';
                            wm_set_clipboard(clip);
                            kfree(clip);
                        }
                    } else if (item_y < 65) {
                        // Paste
                        const char *clip = wm_get_clipboard();
                        if (clip && strlen(clip) > 0) {
                            if (sel_min != sel_max) {
                                int remove_len = sel_max - sel_min;
                                memmove(&state->text_buffer[sel_min], &state->text_buffer[sel_max], state->text_length - sel_max + 1);
                                state->text_length -= remove_len;
                                state->cursor_pos = sel_min;
                            }
                            int insert_len = strlen(clip);
                            if (state->text_length + insert_len < 8191) {
                                memmove(&state->text_buffer[state->cursor_pos + insert_len], &state->text_buffer[state->cursor_pos], state->text_length - state->cursor_pos + 1);
                                memcpy(&state->text_buffer[state->cursor_pos], clip, insert_len);
                                state->cursor_pos += insert_len;
                                state->text_length += insert_len;
                                state->modified = true;
                            }
                            state->sel_start = state->cursor_pos;
                            state->sel_end = state->cursor_pos;
                        }
                    } else {
                        // Delete
                        if (sel_min != sel_max) {
                            int remove_len = sel_max - sel_min;
                            memmove(&state->text_buffer[sel_min], &state->text_buffer[sel_max], state->text_length - sel_max + 1);
                            state->text_length -= remove_len;
                            state->cursor_pos = sel_min;
                            state->sel_start = state->cursor_pos;
                            state->sel_end = state->cursor_pos;
                            state->modified = true;
                        }
                    }
                    editor_recalculate_scroll(win, state);
                    state->show_context_menu = false;
                    win->needs_redraw = true;
                    return;
                }
                state->show_context_menu = false;
                win->needs_redraw = true;
            }
        } else {
            editor_on_mouse(win, mx, my, left, mouse_left_pressed(), right);
        }
    } else {
        if (!mouse_left_pressed()) {
            state->is_selecting = false;
        }
    }
}

static void editor_on_close(struct window *win) {
    if (win && win->user_data) {
        kfree(win->user_data);
    }
}

struct window *text_editor_open_file(const char *path) {
    struct window *win = wm_create_window("Notepad+", 100, 100, 500, 450);
    if (!win) return NULL;

    struct editor_state *state = kmalloc(sizeof(struct editor_state));
    if (!state) {
        wm_destroy_window(win);
        return NULL;
    }
    memset(state, 0, sizeof(struct editor_state));
    state->zoom_level = 1;
    state->style = STYLE_NORMAL;
    if (path && path[0] != '\0') {
        strncpy(state->filename, path, sizeof(state->filename) - 1);
    } else {
        strcpy(state->filename, "/documents/notes.txt");
    }

    // Set title based on filename
    const char *fname = strrchr(state->filename, '/');
    fname = fname ? (fname + 1) : state->filename;
    char title_buf[64] = "Notepad+ - ";
    strncat(title_buf, fname, 48);
    strncpy(win->title, title_buf, 63);
    win->title[63] = '\0';

    struct vfs_node *file = vfs_open(state->filename);
    if (file && file->type == VFS_FILE) {
        size_t to_read = file->size;
        if (to_read >= sizeof(state->text_buffer)) {
            to_read = sizeof(state->text_buffer) - 1;
        }
        int rd = vfs_read(file, state->text_buffer, to_read, 0);
        if (rd > 0) {
            state->text_length = rd;
            state->text_buffer[state->text_length] = '\0';
        }
        state->cursor_pos = state->text_length;
    }

    win->user_data = state;
    win->on_draw = editor_draw;
    win->on_key = editor_on_key;
    win->on_mouse = editor_on_mouse_wrapper;
    win->on_close = editor_on_close;

    return win;
}

struct window *text_editor_open(void) {
    return text_editor_open_file(NULL);
}