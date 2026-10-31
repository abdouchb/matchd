/*
 * Zarqaa OS 0.1 - Search Application (Spotlight / HUD)
 * Creator: Abdelhak Chabbi & AI
 */

#include "desktop/window_manager.h"
#include "desktop/desktop.h"
#include "kernel/heap.h"
#include "gui/graphics.h"
#include "drivers/gpu_renderer.h"
#include "drivers/font.h"

// Colors are provided by include/zarqaa/types.h
#include "zarqaa/string.h"
#include "filesystem/vfs.h"

#define MAX_SEARCH_LEN 64
#define MAX_RESULTS 10

struct search_result {
    char name[64];
    int type; // 0 = App, 1 = File
    int app_id;
};

struct search_state {
    char query[MAX_SEARCH_LEN];
    int query_len;
    struct search_result results[MAX_RESULTS];
    int num_results;
    int selected_index;
};

// Known Apps
static struct { const char *name; int id; } known_apps[] = {
    {"Terminal", APP_TERMINAL},
    {"File Manager", APP_FILE_MANAGER},
    {"Text Editor", APP_TEXT_EDITOR},
    {"Settings", APP_SETTINGS},
    {"Task Manager", APP_TASK_MANAGER},
    {"ZUI Calculator", APP_CALCULATOR},
    {"Browser Pro", APP_BROWSER_PRO},
    {"Device Manager", 15}, // APP_DEVICE_MANAGER
    {"Paint", 116},
    {"Clock", 19},
    {"Calendar", 20},
    {"About Zarqaa OS", 21},
    {"2048", 22},
    {"Minesweeper", 23},
    {NULL, 0}
};

static void update_search_results(struct search_state *state) {
    state->num_results = 0;
    if (state->query_len == 0) return;

    // Search Apps
    for (int i = 0; known_apps[i].name != NULL; i++) {
        // Simple case-insensitive prefix/substring match
        // For simplicity, we just check if it contains the query (case sensitive for now or manual tolower)
        const char *name = known_apps[i].name;
        bool match = false;
        
        // Simple substring check
        for (int j = 0; name[j]; j++) {
            bool current_match = true;
            for (int k = 0; k < state->query_len; k++) {
                char c1 = name[j + k];
                char c2 = state->query[k];
                if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
                if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
                if (c1 != c2) {
                    current_match = false;
                    break;
                }
            }
            if (current_match) {
                match = true;
                break;
            }
        }

        if (match && state->num_results < MAX_RESULTS) {
            strcpy(state->results[state->num_results].name, known_apps[i].name);
            state->results[state->num_results].type = 0;
            state->results[state->num_results].app_id = known_apps[i].id;
            state->num_results++;
        }
    }
    
    if (state->selected_index >= state->num_results) {
        state->selected_index = state->num_results > 0 ? 0 : -1;
    }
}

static void search_on_key(struct window *win, char c, uint8_t scancode) {
    (void)scancode;
    struct search_state *state = (struct search_state *)win->user_data;
    
    if (c == '\b') { // Backspace
        if (state->query_len > 0) {
            state->query_len--;
            state->query[state->query_len] = '\0';
            update_search_results(state);
        }
    } else if (c == '\n' || c == '\r') { // Enter
        if (state->selected_index >= 0 && state->selected_index < state->num_results) {
            if (state->results[state->selected_index].type == 0) {
                desktop_launch_app(state->results[state->selected_index].app_id);
                wm_destroy_window(win); // Close search after launch
            }
        }
    } else if (c >= 32 && c <= 126) {
        if (state->query_len < MAX_SEARCH_LEN - 1) {
            state->query[state->query_len++] = c;
            state->query[state->query_len] = '\0';
            update_search_results(state);
        }
    } else if (c == 0) {
        if (scancode == 0x48) { // Up Arrow
            if (state->selected_index > 0) state->selected_index--;
        } else if (scancode == 0x50) { // Down Arrow
            if (state->selected_index < state->num_results - 1) state->selected_index++;
        }
    }
}

static void search_draw(struct window *win) {
    if (!win) return;
    struct search_state *state = (struct search_state *)win->user_data;

    int cx = win->x;
    int cy = win->y;
    int cw = win->width;
    int ch = win->height;

    // Background (Blurry / Dark transparent look)
    gfx_draw_rounded_rect_filled(cx, cy, cw, ch, 0, 0x1A1A1A); // Dark gray
    
    // Search Bar
    gfx_draw_rounded_rect_filled(cx + 20, cy + 20, cw - 40, 50, 0, 0x333333);
    gfx_draw_string_transparent(cx + 35, cy + 35, state->query_len > 0 ? state->query : "Search apps or files...", state->query_len > 0 ? COLOR_WHITE : 0x777777);

    // Blinking cursor
    // static int blink = 0; blink++;
    // if (blink % 20 < 10) gfx_draw_rounded_rect_filled(cx + 35 + state->query_len * 8, cy + 35, 2, 16, 0, COLOR_WHITE);

    // Results
    int ry = cy + 90;
    for (int i = 0; i < state->num_results; i++) {
        if (i == state->selected_index) {
            gfx_draw_rounded_rect_filled(cx + 20, ry, cw - 40, 40, 0, 0x0055AA); // Highlight
        }
        
        char *prefix = state->results[i].type == 0 ? "APP  | " : "FILE | ";
        gfx_draw_string_transparent(cx + 35, ry + 12, prefix, 0xAAAAAA);
        gfx_draw_string_transparent(cx + 90, ry + 12, state->results[i].name, COLOR_WHITE);
        
        ry += 40;
    }
}

static void search_close(struct window *win) {
    if (win) {
        if (win->user_data) {
            kfree(win->user_data);
            win->user_data = NULL;
        }
    }
}

struct window *search_app_open(void) {
    // HUD style: Center of screen
    int sw = 800; // Assuming screen res or we can pass it
    // For now hardcode HUD dimensions
    int hud_w = 500;
    int hud_h = 400;
    
    struct window *win = wm_create_window("Search", (sw - hud_w)/2, 100, hud_w, hud_h); 
    if (!win) return NULL;

    struct search_state *state = (struct search_state *)kmalloc(sizeof(struct search_state));
    memset(state, 0, sizeof(struct search_state));
    state->selected_index = -1;

    win->user_data = state;
    win->on_draw = search_draw;
    win->on_key = search_on_key;
    win->on_close = search_close;

    return win;
}
