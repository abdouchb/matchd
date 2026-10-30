/*
 * Zarqaa OS 0.1 - Calculator App (ZUI Proof of Concept)
 * Creator: Abdelhak Chabbi
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "zarqaa/string.h"
#include "libzui/zui_window.h"
#include "libzui/zui_button.h"
#include "libzui/zui_label.h"

struct calc_state {
    zui_window_t *zui_win;
    zui_label_t *display;
    int operand1;
    int operand2;
    char op;
    bool new_number;
};

static void calc_process_char(struct calc_state *state, char c) {
    if (c >= '0' && c <= '9') {
        if (state->new_number) {
            char buf[2] = {c, 0};
            strcpy(state->display->text, buf);
            state->new_number = false;
        } else {
            if (strlen(state->display->text) < 15) {
                if (strcmp(state->display->text, "0") == 0) {
                    char buf[2] = {c, 0};
                    strcpy(state->display->text, buf);
                } else {
                    char buf[2] = {c, 0};
                    strcat(state->display->text, buf);
                }
            }
        }
    } else if (c == 'c' || c == 'C') {
        strcpy(state->display->text, "0");
        state->operand1 = 0;
        state->operand2 = 0;
        state->op = 0;
        state->new_number = true;
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        state->operand1 = atoi(state->display->text);
        state->op = c;
        state->new_number = true;
    } else if (c == '=' || c == '\n' || c == '\r') {
        state->operand2 = atoi(state->display->text);
        int result = 0;
        if (state->op == '+') result = state->operand1 + state->operand2;
        if (state->op == '-') result = state->operand1 - state->operand2;
        if (state->op == '*') result = state->operand1 * state->operand2;
        if (state->op == '/') {
            if (state->operand2 != 0) result = state->operand1 / state->operand2;
            else result = 0; // Quick div0 fix
        }
        itoa(result, state->display->text, 10);
        state->new_number = true;
        state->op = 0;
    }
    
    state->display->base.needs_redraw = true;
}

static void calc_on_button_click(zui_button_t *btn) {
    zui_widget_t *win_base = btn->base.parent->parent; // parent=pane, parent=window
    struct calc_state *state = (struct calc_state *)win_base->user_data;
    if (!state || !state->display) return;
    
    calc_process_char(state, btn->text[0]);
    win_base->needs_redraw = true;
}

static void calc_on_key(struct window *win, char key, uint8_t scancode) {
    (void)scancode;
    struct calc_state *state = (struct calc_state *)win->user_data;
    if (!state) return;
    
    if (key != 0) {
        calc_process_char(state, key);
        win->needs_redraw = true;
    }
}

static void calc_on_draw(struct window *win) {
    struct calc_state *state = (struct calc_state *)win->user_data;
    
    // Fill window background
    fb_fill_rect(0, 0, win->width, win->height, 0xFF0D1925);
    
    // Let ZUI draw all its widgets
    zui_widget_draw(&state->zui_win->base, 0, 0);
}

static void calc_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)mx;
    (void)my;
    (void)right;
    struct calc_state *state = (struct calc_state *)win->user_data;
    
    // Pass absolute window coordinates to ZUI
    if (zui_widget_handle_mouse(&state->zui_win->base, win->x, win->y, left)) {
        win->needs_redraw = true;
    }
}

static void calc_on_close(struct window *win) {
    struct calc_state *state = (struct calc_state *)win->user_data;
    if (state) {
        zui_widget_destroy(&state->zui_win->base);
        kfree(state);
    }
}

struct window *calculator_open(void) {
    struct window *win = wm_create_window("ZUI Calculator", 200, 150, 300, 400);
    if (!win) return NULL;

    struct calc_state *state = kcalloc(1, sizeof(struct calc_state));
    
    // Create ZUI Window (Acts as the root container)
    state->zui_win = zui_window_create(0, 0, 300, 400, "");
    state->zui_win->base.user_data = state;
    
    // Display Label
    state->display = zui_label_create(20, 20, "0");
    state->display->base.fg_color = COLOR_ACCENT;
    zui_widget_add_child(state->zui_win->content_pane, &state->display->base);
    
    // Buttons Grid
    int start_x = 20, start_y = 60;
    int bw = 60, bh = 60, pad = 10;
    
    const char *labels[] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };
    
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            zui_button_t *btn = zui_button_create(
                start_x + col * (bw + pad),
                start_y + row * (bh + pad),
                bw, bh, labels[row * 4 + col]
            );
            btn->on_click = calc_on_button_click;
            zui_widget_add_child(state->zui_win->content_pane, &btn->base);
        }
    }

    win->user_data = state;
    win->on_draw = calc_on_draw;
    win->on_mouse = calc_on_mouse;
    win->on_key = calc_on_key;
    win->on_close = calc_on_close;
    
    return win;
}
