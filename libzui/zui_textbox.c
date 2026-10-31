#include "libzui/zui_textbox.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"
#include "drivers/usb/usb_hid.h"
#include "gui/graphics.h"
#include "drivers/gpu_renderer.h"
#include "drivers/font.h"
#include "kernel/clipboard.h"

static void zui_textbox_draw(zui_widget_t *base, int abs_x, int abs_y) {
    zui_textbox_t *tb = (zui_textbox_t *)base;
    
    // Draw background and border
    gpu_renderer_fill_rect(abs_x, abs_y, base->width, base->height, 0xFFFFFFFF); // White BG
    gfx_draw_rect(abs_x, abs_y, base->width, base->height, tb->is_focused ? 0xFF0078D7 : 0xFF808080); // Border
    
    int text_len = strlen(tb->text);
    
    int sel_min = tb->selection_start;
    int sel_max = tb->selection_end;
    if (sel_min > sel_max) {
        int tmp = sel_min;
        sel_min = sel_max;
        sel_max = tmp;
    }
    
    // Draw text with selection highlighting
    int draw_x = abs_x + 5;
    int draw_y = abs_y + (base->height - 16) / 2;
    
    for (int i = 0; i < text_len; i++) {
        char str[2] = { tb->text[i], 0 };
        bool is_selected = (i >= sel_min && i < sel_max);
        
        if (is_selected) {
            gpu_renderer_fill_rect(draw_x, draw_y - 2, 8, 20, 0xFF0078D7); // Blue highlight
            gfx_draw_string_transparent(draw_x, draw_y, str, 0xFFFFFFFF); // White text
        } else {
            gfx_draw_string_transparent(draw_x, draw_y, str, 0xFF000000); // Black text
        }
        draw_x += 8;
    }
    
    // Draw cursor
    if (tb->is_focused && sel_min == sel_max) {
        int cursor_x = abs_x + 5 + (tb->cursor_pos * 8);
        gpu_renderer_fill_rect(cursor_x, draw_y - 2, 2, 20, 0xFF000000);
    }
}

static void zui_textbox_on_mouse(zui_widget_t *base, int local_x, int local_y, bool is_click) {
    zui_textbox_t *tb = (zui_textbox_t *)base;
    (void)local_y; // Not used yet
    
    int char_idx = (local_x - 5) / 8;
    int text_len = strlen(tb->text);
    if (char_idx < 0) char_idx = 0;
    if (char_idx > text_len) char_idx = text_len;
    
    if (is_click) {
        tb->is_focused = true;
        tb->selection_start = char_idx;
        tb->selection_end = char_idx;
        tb->cursor_pos = char_idx;
        tb->is_selecting = true;
        base->needs_redraw = true;
    } else if (tb->is_selecting) {
        if (mouse_left_pressed()) {
            tb->selection_end = char_idx;
            tb->cursor_pos = char_idx;
            base->needs_redraw = true;
        } else {
            tb->is_selecting = false;
        }
    }
}

zui_textbox_t *zui_textbox_create(int x, int y, int width, int height, const char *initial_text, int max_length) {
    zui_textbox_t *tb = kmalloc(sizeof(zui_textbox_t));
    if (!tb) return NULL;
    
    zui_widget_init(&tb->base, ZUI_WIDGET_TEXTBOX, x, y, width, height);
    tb->base.draw = zui_textbox_draw;
    tb->base.on_mouse = zui_textbox_on_mouse;
    
    tb->max_length = max_length;
    tb->text = kmalloc(max_length + 1);
    tb->text[0] = '\0';
    
    if (initial_text) {
        strncpy(tb->text, initial_text, max_length);
        tb->text[max_length] = '\0';
    }
    
    tb->cursor_pos = strlen(tb->text);
    tb->selection_start = tb->cursor_pos;
    tb->selection_end = tb->cursor_pos;
    tb->is_focused = false;
    tb->is_selecting = false;
    
    return tb;
}

void zui_textbox_set_text(zui_textbox_t *tb, const char *text) {
    if (!tb || !text) return;
    strncpy(tb->text, text, tb->max_length);
    tb->text[tb->max_length] = '\0';
    tb->cursor_pos = strlen(tb->text);
    tb->selection_start = tb->cursor_pos;
    tb->selection_end = tb->cursor_pos;
    tb->base.needs_redraw = true;
}

const char *zui_textbox_get_text(zui_textbox_t *tb) {
    return tb ? tb->text : NULL;
}

bool zui_textbox_handle_key(zui_textbox_t *tb, char c, uint16_t scancode) {
    (void)scancode;
    if (!tb || !tb->is_focused) return false;
    
    // Simple Ctrl+C / Ctrl+V / Ctrl+X detection (Assuming Ctrl is pressed if scancode is special or we use a modifier check)
    // For now, if character is ASCII 3 (Ctrl+C), ASCII 22 (Ctrl+V), ASCII 24 (Ctrl+X)
    if (c == 3) { // Ctrl+C
        int sel_min = tb->selection_start;
        int sel_max = tb->selection_end;
        if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }
        if (sel_min != sel_max) {
            char *copied = kmalloc(sel_max - sel_min + 1);
            if (copied) {
                strncpy(copied, tb->text + sel_min, sel_max - sel_min);
                copied[sel_max - sel_min] = '\0';
                clipboard_set_text(copied);
                kfree(copied);
            }
        }
        return true;
    } else if (c == 22) { // Ctrl+V
        const char *clip = clipboard_get_text();
        if (clip) {
            // Delete selection if any
            int sel_min = tb->selection_start;
            int sel_max = tb->selection_end;
            if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }
            
            int clip_len = strlen(clip);
            int current_len = strlen(tb->text);
            
            // Move text after selection
            memmove(tb->text + sel_min + clip_len, tb->text + sel_max, current_len - sel_max + 1);
            memcpy(tb->text + sel_min, clip, clip_len);
            
            tb->cursor_pos = sel_min + clip_len;
            tb->selection_start = tb->cursor_pos;
            tb->selection_end = tb->cursor_pos;
            tb->base.needs_redraw = true;
        }
        return true;
    }
    
    // Normal typing and backspace
    if (c >= 32 && c <= 126) {
        int sel_min = tb->selection_start;
        int sel_max = tb->selection_end;
        if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }
        
        int current_len = strlen(tb->text);
        int remove_len = sel_max - sel_min;
        
        if (current_len - remove_len + 1 < tb->max_length) {
            // Move text to make room
            memmove(tb->text + sel_min + 1, tb->text + sel_max, current_len - sel_max + 1);
            tb->text[sel_min] = c;
            tb->cursor_pos = sel_min + 1;
            tb->selection_start = tb->cursor_pos;
            tb->selection_end = tb->cursor_pos;
            tb->base.needs_redraw = true;
            return true;
        }
    } else if (c == '\b') { // Backspace
        int sel_min = tb->selection_start;
        int sel_max = tb->selection_end;
        if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }
        
        int current_len = strlen(tb->text);
        
        if (sel_min != sel_max) {
            // Delete selection
            memmove(tb->text + sel_min, tb->text + sel_max, current_len - sel_max + 1);
            tb->cursor_pos = sel_min;
        } else if (tb->cursor_pos > 0) {
            // Delete character before cursor
            memmove(tb->text + tb->cursor_pos - 1, tb->text + tb->cursor_pos, current_len - tb->cursor_pos + 1);
            tb->cursor_pos--;
        }
        
        tb->selection_start = tb->cursor_pos;
        tb->selection_end = tb->cursor_pos;
        tb->base.needs_redraw = true;
        return true;
    }
    
    return false;
}
