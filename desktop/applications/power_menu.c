/*
 * Zarqaa OS 0.1 - Power & Lock Menu
 * Creator: Abdelhak Chabbi & AI
 */

#include "desktop/window_manager.h"
#include "drivers/framebuffer.h"
#include "gui/graphics.h"
#include "kernel/heap.h"
#include "zarqaa/types.h"
#include "zarqaa/string.h"
#include "drivers/pit.h"
#include "kernel/acpi.h"

enum power_action {
    ACTION_NONE = 0,
    ACTION_RESTART,
    ACTION_SHUTDOWN
};

struct power_state {
    bool is_locked;
    enum power_action pending_action;
    uint32_t last_tick;
    int current_countdown;
};

static void execute_power_action(enum power_action action) {
    if (action == ACTION_RESTART) {
        outb(0x64, 0xFE); // Reset via Keyboard Controller
        while(1) hlt();
    } else if (action == ACTION_SHUTDOWN) {
        acpi_power_off();
    }
}

static void power_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    if (!left) return;
    struct power_state *state = (struct power_state *)win->user_data;

    int w = win->width;
    int h = win->height;

    if (state->is_locked) {
        // Unlock Button (center bottom)
        int btn_w = 200;
        int btn_h = 50;
        int btn_x = (w - btn_w) / 2;
        int btn_y = h / 2 + 100;
        
        if (mx >= btn_x && mx <= btn_x + btn_w && my >= btn_y && my <= btn_y + btn_h) {
            wm_destroy_window(win);
        }
    } else if (state->pending_action != ACTION_NONE) {
        // Confirmation Screen
        int cx = w / 2;
        int cy = h / 2;
        int btn_w = 120;
        int btn_h = 40;
        
        // Confirm Button
        if (mx >= cx - 140 && mx <= cx - 140 + btn_w && my >= cy + 50 && my <= cy + 50 + btn_h) {
            execute_power_action(state->pending_action);
        }
        // Cancel Button
        if (mx >= cx + 20 && mx <= cx + 20 + btn_w && my >= cy + 50 && my <= cy + 50 + btn_h) {
            state->pending_action = ACTION_NONE;
            win->needs_redraw = true;
        }
    } else {
        // Power Options
        int cx = w / 2;
        int cy = h / 2;
        
        int btn_w = 160;
        int btn_h = 160;
        int spacing = 40;
        
        int start_x = cx - (btn_w * 3 + spacing * 2) / 2;
        
        // 1. Lock Button
        if (mx >= start_x && mx <= start_x + btn_w && my >= cy - btn_h/2 && my <= cy + btn_h/2) {
            state->is_locked = true;
            win->needs_redraw = true;
            return;
        }
        
        start_x += btn_w + spacing;
        
        // 2. Restart Button
        if (mx >= start_x && mx <= start_x + btn_w && my >= cy - btn_h/2 && my <= cy + btn_h/2) {
            state->pending_action = ACTION_RESTART;
            state->current_countdown = 5;
            state->last_tick = pit_get_ticks();
            win->needs_redraw = true;
            return;
        }
        
        start_x += btn_w + spacing;
        
        // 3. Shutdown Button
        if (mx >= start_x && mx <= start_x + btn_w && my >= cy - btn_h/2 && my <= cy + btn_h/2) {
            state->pending_action = ACTION_SHUTDOWN;
            state->current_countdown = 5;
            state->last_tick = pit_get_ticks();
            win->needs_redraw = true;
            return;
        }
        
        // Cancel / Click anywhere else to close Power Menu
        wm_destroy_window(win);
    }
}

static void power_on_draw(struct window *win) {
    struct power_state *state = (struct power_state *)win->user_data;
    int w = win->width;
    int h = win->height;

    // Semi-transparent dark overlay (Since we can't easily read backbuffer without tearing,
    // we just draw a very dark solid holographic color for now).
    fb_fill_rect(0, 0, w, h, 0xFF050A10);
    
    // Grid pattern
    for (int y = 0; y < h; y += 40) {
        fb_draw_line(0, y, w, y, 0xFF0A1420);
    }
    for (int x = 0; x < w; x += 40) {
        fb_draw_line(x, 0, x, h, 0xFF0A1420);
    }

    if (state->pending_action != ACTION_NONE) {
        // Update countdown
        if (pit_get_ticks() - state->last_tick >= 100) { // Approx 1s if 100Hz
            state->current_countdown--;
            state->last_tick = pit_get_ticks();
            if (state->current_countdown <= 0) {
                execute_power_action(state->pending_action);
            }
        }
        win->needs_redraw = true; // Continuous update
        
        int cx = w / 2;
        int cy = h / 2;
        
        // Draw dialog box
        gfx_draw_rounded_rect_filled(cx - 200, cy - 100, 400, 200, 15, 0xFF181825);
        gfx_draw_rounded_rect(cx - 200, cy - 100, 400, 200, 15, 0xFFF38BA8); // Red border
        
        const char *action_str = (state->pending_action == ACTION_RESTART) ? "Restart" : "Shutdown";
        char msg[64];
        strcpy(msg, "Are you sure you want to ");
        strcat(msg, action_str);
        strcat(msg, "?");
        
        fb_draw_string_transparent(cx - (strlen(msg)*8)/2, cy - 50, msg, 0xFFCDD6F4);
        
        char count_msg[64];
        strcpy(count_msg, "Executing automatically in ");
        char num[4];
        itoa(state->current_countdown, num, 10);
        strcat(count_msg, num);
        strcat(count_msg, " seconds...");
        fb_draw_string_transparent(cx - (strlen(count_msg)*8)/2, cy - 20, count_msg, 0xFFF9E2AF);
        
        // Buttons
        int btn_w = 120;
        int btn_h = 40;
        
        // Confirm Button
        gfx_draw_rounded_rect_filled(cx - 140, cy + 50, btn_w, btn_h, 8, 0xFF313244);
        gfx_draw_rounded_rect(cx - 140, cy + 50, btn_w, btn_h, 8, 0xFFF38BA8);
        fb_draw_string_transparent(cx - 140 + 32, cy + 50 + 12, "Confirm", 0xFFF38BA8);
        
        // Cancel Button
        gfx_draw_rounded_rect_filled(cx + 20, cy + 50, btn_w, btn_h, 8, 0xFF313244);
        gfx_draw_rounded_rect(cx + 20, cy + 50, btn_w, btn_h, 8, 0xFF89B4FA);
        fb_draw_string_transparent(cx + 20 + 36, cy + 50 + 12, "Cancel", 0xFF89B4FA);
        
    } else if (state->is_locked) {
        // Lock Screen UI
        int cx = w / 2;
        int cy = h / 2;
        
        gfx_draw_circle_filled(cx, cy - 80, 50, 0xFF112233);
        gfx_draw_circle(cx, cy - 80, 50, 0xFFCDD6F4);
        // Draw a simple lock icon
        fb_fill_rect(cx - 20, cy - 80, 40, 30, 0xFFCDD6F4);
        gfx_draw_circle(cx, cy - 90, 15, 0xFFCDD6F4);
        
        int text_w = 13 * 8; // "System Locked"
        fb_draw_string_transparent(cx - text_w/2, cy + 10, "System Locked", 0xFFCDD6F4);
        
        // Unlock Button
        int btn_w = 200;
        int btn_h = 50;
        int btn_x = cx - btn_w / 2;
        int btn_y = cy + 100;
        
        gfx_draw_rounded_rect_filled(btn_x, btn_y, btn_w, btn_h, 25, 0xFF313244);
        gfx_draw_rounded_rect(btn_x, btn_y, btn_w, btn_h, 25, 0xFF89B4FA);
        fb_draw_string_transparent(cx - 28, btn_y + 17, "Unlock", 0xFF89B4FA);
        
    } else {
        // Power Menu UI
        int cx = w / 2;
        int cy = h / 2;
        
        int text_w = 20 * 8; // "Select Power Option:"
        fb_draw_string_transparent(cx - text_w/2, cy - 150, "Select Power Option:", 0xFFCDD6F4);
        
        int btn_w = 160;
        int btn_h = 160;
        int spacing = 40;
        
        int start_x = cx - (btn_w * 3 + spacing * 2) / 2;
        
        // 1. Lock Button (Blue)
        gfx_draw_rounded_rect_filled(start_x, cy - btn_h/2, btn_w, btn_h, 20, 0xFF181825);
        gfx_draw_rounded_rect(start_x, cy - btn_h/2, btn_w, btn_h, 20, 0xFF89B4FA);
        gfx_draw_circle_filled(start_x + btn_w/2, cy - 20, 30, 0xFF313244);
        fb_draw_string_transparent(start_x + btn_w/2 - 16, cy + 40, "Lock", 0xFF89B4FA);
        
        start_x += btn_w + spacing;
        
        // 2. Restart Button (Yellow)
        gfx_draw_rounded_rect_filled(start_x, cy - btn_h/2, btn_w, btn_h, 20, 0xFF181825);
        gfx_draw_rounded_rect(start_x, cy - btn_h/2, btn_w, btn_h, 20, 0xFFF9E2AF);
        gfx_draw_circle_filled(start_x + btn_w/2, cy - 20, 30, 0xFF313244);
        fb_draw_string_transparent(start_x + btn_w/2 - 28, cy + 40, "Restart", 0xFFF9E2AF);
        
        start_x += btn_w + spacing;
        
        // 3. Shutdown Button (Red)
        gfx_draw_rounded_rect_filled(start_x, cy - btn_h/2, btn_w, btn_h, 20, 0xFF181825);
        gfx_draw_rounded_rect(start_x, cy - btn_h/2, btn_w, btn_h, 20, 0xFFF38BA8);
        gfx_draw_circle_filled(start_x + btn_w/2, cy - 20, 30, 0xFF313244);
        fb_draw_string_transparent(start_x + btn_w/2 - 32, cy + 40, "Shutdown", 0xFFF38BA8);
    }
}

static void power_on_close(struct window *win) {
    if (win->user_data) kfree(win->user_data);
}

struct window *power_menu_open(void) {
    // Open a full-screen frameless window
    struct window *win = wm_create_window("Power", 0, 0, fb_get_width(), fb_get_height());
    if (!win) return NULL;

    win->frameless = true; // This hides the titlebar and borders
    
    struct power_state *state = kcalloc(1, sizeof(struct power_state));
    state->is_locked = false;
    state->pending_action = ACTION_NONE;
    
    win->user_data = state;
    win->on_draw = power_on_draw;
    win->on_mouse = power_on_mouse;
    win->on_close = power_on_close;

    return win;
}
