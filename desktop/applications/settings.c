/*
 * Zarqaa OS 0.1 - Settings Application (Creative Modern Redesign)
 * Creator: Abdelhak Chabbi
 *
 * A modern, tabbed settings application with a beautiful 
 * dark glassmorphic UI, inspired by GNOME and customized for Zarqaa.
 */

#include "desktop/window_manager.h"
#include "desktop/desktop.h"
#include "kernel/heap.h"
#include "gui/graphics.h"
#include "gui/wallpaper_thumbnails.h"
#include "drivers/framebuffer.h"
#include "zarqaa/string.h"

#define TAB_WIFI 0
#define TAB_NETWORK 1
#define TAB_BLUETOOTH 2
#define TAB_DISPLAYS 3
#define TAB_SOUND 4
#define TAB_POWER 5
#define TAB_APPEARANCE 6
#define TAB_APPS 7
#define TAB_ABOUT 8
#define NUM_TABS 9

struct settings_state {
    int active_tab;
    int hover_tab;
    
    /* Displays state */
    bool adjust_tv;
    bool hover_adjust_tv;
    int scale_index; // 0=100%, 1=200%, 2=300%, 3=400%
    int hover_scale; // -1 to 3
    bool fractional_scale;
    bool hover_fractional;
    bool night_light;
    bool hover_night_light;

    /* Appearance state */
    int selected_wallpaper_idx;
    int hover_wallpaper_idx;
    bool hover_apply_btn;
    bool applied_toast;
    int toast_ticks;
};

// Colors from QuantumPeak + GNOME inspired Dark Mode
#define BG_MAIN        0xFF0D141C
#define BG_SIDEBAR     0xFF121B26
#define BG_PANEL       0xFF1A2636
#define BORDER_PANEL   0xFF2A3A4C
#define COLOR_TEXT     0xFFFFFFFF
#define COLOR_MUTED    0xFF8098B3
#define COLOR_CYAN     0xFF00E5FF
#define COLOR_SETTINGS_ACCENT 0xFF1E3A5F

static void draw_toggle(int x, int y, bool state, bool hover) {
    color_t bg = state ? COLOR_CYAN : 0xFF2A3A4C;
    if (hover) bg = gfx_alpha_blend(0xFFFFFFFF, bg, 30);
    
    gfx_draw_rounded_rect_filled(x, y, 40, 20, 10, bg);
    int knob_x = state ? x + 22 : x + 2;
    gfx_draw_circle_filled(knob_x + 8, y + 10, 8, COLOR_TEXT);
}

/* Helper to render scaled thumbnail directly to screen */
static void draw_thumb_scaled(int sx, int sy, int sw, int sh, const uint32_t *pixels, int tw, int th) {
    if (!pixels || tw <= 0 || th <= 0 || sw <= 0 || sh <= 0) return;
    int scr_w = fb_get_width();
    int scr_h = fb_get_height();

    for (int y = 0; y < sh; y++) {
        int py = sy + y;
        if (py < 0 || py >= scr_h) continue;
        int src_y = (y * th) / sh;
        if (src_y >= th) src_y = th - 1;
        const uint32_t *src_row = pixels + src_y * tw;

        for (int x = 0; x < sw; x++) {
            int px = sx + x;
            if (px < 0 || px >= scr_w) continue;
            int src_x = (x * tw) / sw;
            if (src_x >= tw) src_x = tw - 1;
            fb_putpixel(px, py, src_row[src_x]);
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  Appearance & Wallpaper Tab
 * ══════════════════════════════════════════════════════════ */

static void draw_appearance_tab(struct window *win, struct settings_state *state, int cx, int cy, int cw, int ch) {
    (void)win;
    (void)ch;

    // Header Title
    fb_draw_string_transparent(cx + 20, cy + 12, "Appearance & Wallpaper", COLOR_TEXT);
    fb_draw_string_transparent(cx + 20, cy + 28, "Live interactive desktop simulator & background gallery", COLOR_MUTED);

    // 1. Live Desktop Simulator Monitor
    int mon_w = 230;
    int mon_h = 130;
    int mon_x = cx + (cw - mon_w) / 2;
    int mon_y = cy + 46;

    // Monitor Outer Glass Bezel
    gfx_draw_glass_panel(mon_x - 6, mon_y - 6, mon_w + 12, mon_h + 12, 10, 0xFF14202E, COLOR_CYAN, 240);
    
    // Monitor Screen Area
    int scr_x = mon_x;
    int scr_y = mon_y;
    
    int preview_idx = state->selected_wallpaper_idx;
    if (state->hover_wallpaper_idx >= 0 && state->hover_wallpaper_idx < NUM_WALLPAPERS) {
        preview_idx = state->hover_wallpaper_idx;
    }
    if (preview_idx < 0 || preview_idx >= NUM_WALLPAPERS) preview_idx = 0;

    const struct wallpaper_info *info = wallpaper_get_info(preview_idx);
    if (info && info->thumb_pixels) {
        draw_thumb_scaled(scr_x, scr_y, mon_w, mon_h, info->thumb_pixels, info->thumb_w, info->thumb_h);
    } else {
        fb_fill_rect(scr_x, scr_y, mon_w, mon_h, 0xFF0D141C);
    }

    // Mini Desktop Elements inside Simulator:
    // Mini Top Bar
    gfx_draw_glass_panel(scr_x, scr_y, mon_w, 11, 0, 0xFF0A101D, 0xFF1E354F, 180);
    fb_draw_string_transparent(scr_x + 6, scr_y + 2, "ZarqaaOS", 0xFF00E5FF);
    fb_draw_string_transparent(scr_x + mon_w - 40, scr_y + 2, "18:30", COLOR_TEXT);

    // Mini Active Window
    int win_w = 90;
    int win_h = 55;
    int win_x = scr_x + (mon_w - win_w) / 2;
    int win_y = scr_y + 25;
    gfx_draw_glass_panel(win_x, win_y, win_w, win_h, 4, 0xFF142236, COLOR_CYAN, 210);
    fb_fill_rect(win_x + 4, win_y + 3, win_w - 8, 6, 0xFF00E5FF);
    fb_fill_rect(win_x + 8, win_y + 14, 40, 2, 0xFF8098B3);
    fb_fill_rect(win_x + 8, win_y + 20, 60, 2, 0xFF506883);
    fb_fill_rect(win_x + 8, win_y + 26, 50, 2, 0xFF506883);

    // Mini Bottom Dock
    int dock_w = 70;
    int dock_h = 10;
    int dock_x = scr_x + (mon_w - dock_w) / 2;
    int dock_y = scr_y + mon_h - 14;
    gfx_draw_glass_panel(dock_x, dock_y, dock_w, dock_h, 5, 0xFF0D1826, 0xFF2A3A4C, 220);
    for (int d = 0; d < 5; d++) {
        color_t dc = (d == 0) ? 0xFF00E5FF : ((d == 1) ? 0xFF5CFA92 : ((d == 2) ? 0xFFFA925C : 0xFFFA5C5C));
        gfx_draw_circle_filled(dock_x + 8 + d * 13, dock_y + 5, 2, dc);
    }

    // Monitor Stand & Neck
    fb_fill_rect(mon_x + (mon_w / 2) - 10, mon_y + mon_h + 6, 20, 8, 0xFF1A2636);
    gfx_draw_rounded_rect_filled(mon_x + (mon_w / 2) - 28, mon_y + mon_h + 13, 56, 4, 2, 0xFF2A3A4C);

    // Live Badge & Wallpaper Name
    int badge_y = mon_y + mon_h + 20;
    gfx_draw_circle_filled(cx + (cw / 2) - 80, badge_y + 4, 3, 0xFF5CFA92);
    char title_str[64] = "LIVE: ";
    if (info) strncat(title_str, info->title, 40);
    fb_draw_string_transparent(cx + (cw / 2) - 70, badge_y, title_str, COLOR_CYAN);

    // 2. Apply as Wallpaper Button
    int btn_w = 180;
    int btn_h = 24;
    int btn_x = cx + (cw - btn_w) / 2;
    int btn_y = badge_y + 18;

    color_t btn_bg = state->applied_toast ? 0xFF1F5C3E : (state->hover_apply_btn ? 0xFF00A0C0 : COLOR_CYAN);
    color_t btn_text = state->applied_toast ? 0xFF5CFA92 : (state->hover_apply_btn ? 0xFFFFFFFF : 0xFF000000);

    gfx_draw_rounded_rect_filled(btn_x, btn_y, btn_w, btn_h, 12, btn_bg);
    gfx_draw_rounded_rect(btn_x, btn_y, btn_w, btn_h, 12, state->applied_toast ? 0xFF5CFA92 : COLOR_TEXT);

    const char *btn_label = state->applied_toast ? "[ Applied to Desktop! ]" : "[ Apply as Wallpaper ]";
    int label_len = strlen(btn_label);
    fb_draw_string_transparent(btn_x + (btn_w - label_len * 8) / 2, btn_y + 6, btn_label, btn_text);

    // 3. Wallpaper Thumbnails Grid (2 rows of 5 cards)
    int grid_y = btn_y + 32;
    int card_w = 100;
    int card_h = 68;
    int gap_x = 8;
    int gap_y = 6;
    int total_grid_w = 5 * card_w + 4 * gap_x; // 524
    int grid_x = cx + (cw - total_grid_w) / 2;

    for (int i = 0; i < NUM_WALLPAPERS; i++) {
        int row = i / 5;
        int col = i % 5;
        int x = grid_x + col * (card_w + gap_x);
        int y = grid_y + row * (card_h + gap_y);

        bool is_selected = (state->selected_wallpaper_idx == i);
        bool is_hover = (state->hover_wallpaper_idx == i);

        // Card Outer Panel
        color_t border_col = is_selected ? COLOR_CYAN : (is_hover ? 0xFF5CFA92 : BORDER_PANEL);
        gfx_draw_glass_panel(x, y, card_w, card_h, 6, is_hover ? 0xFF1F2F42 : BG_PANEL, border_col, 220);

        // Thumbnail Image
        const struct wallpaper_info *item = wallpaper_get_info(i);
        if (item && item->thumb_pixels) {
            draw_thumb_scaled(x + 3, y + 3, card_w - 6, 44, item->thumb_pixels, item->thumb_w, item->thumb_h);
        }

        // Title Label
        if (item) {
            color_t title_color = is_selected ? COLOR_CYAN : (is_hover ? COLOR_TEXT : COLOR_MUTED);
            fb_draw_string_transparent(x + 5, y + 51, item->title, title_color);
        }

        // Checkmark badge for selected wallpaper
        if (is_selected) {
            gfx_draw_circle_filled(x + card_w - 10, y + 10, 6, COLOR_CYAN);
            fb_draw_string_transparent(x + card_w - 13, y + 6, "v", 0xFF000000);
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  Displays Tab
 * ══════════════════════════════════════════════════════════ */

static void draw_displays_tab(struct window *win, struct settings_state *state, int cx, int cy, int cw, int ch) {
    (void)win;
    (void)ch;
    // Title
    fb_draw_string_transparent(cx + (cw/2) - 40, cy + 20, "Displays", COLOR_TEXT);
    
    int px = cx + 40;
    int py = cy + 60;
    int pw = cw - 80;
    
    // Top Panel
    gfx_draw_rounded_rect_filled(px, py, pw, 280, 12, BG_PANEL);
    gfx_draw_rounded_rect(px, py, pw, 280, 12, BORDER_PANEL);
    
    // 1. Orientation
    fb_draw_string_transparent(px + 20, py + 20, "Orientation", COLOR_TEXT);
    fb_draw_string_transparent(px + pw - 100, py + 20, "Landscape v", COLOR_CYAN);
    fb_draw_line(px + 20, py + 45, px + pw - 20, py + 45, BORDER_PANEL);
    
    // 2. Resolution
    fb_draw_string_transparent(px + 20, py + 60, "Resolution", COLOR_TEXT);
    fb_draw_string_transparent(px + pw - 150, py + 60, "1920 x 1080 (16:9) v", COLOR_CYAN);
    fb_draw_line(px + 20, py + 85, px + pw - 20, py + 85, BORDER_PANEL);
    
    // 3. Refresh Rate
    fb_draw_string_transparent(px + 20, py + 100, "Refresh Rate", COLOR_TEXT);
    fb_draw_string_transparent(px + pw - 90, py + 100, "60.00 Hz v", COLOR_CYAN);
    fb_draw_line(px + 20, py + 125, px + pw - 20, py + 125, BORDER_PANEL);
    
    // 4. Adjust for TV
    fb_draw_string_transparent(px + 20, py + 140, "Adjust for TV", COLOR_TEXT);
    draw_toggle(px + pw - 60, py + 135, state->adjust_tv, state->hover_adjust_tv);
    fb_draw_line(px + 20, py + 165, px + pw - 20, py + 165, BORDER_PANEL);
    
    // 5. Scale (Segmented control)
    fb_draw_string_transparent(px + 20, py + 180, "Scale", COLOR_TEXT);
    int seg_w = 200;
    int seg_x = px + pw - seg_w - 20;
    int seg_y = py + 175;
    
    gfx_draw_rounded_rect_filled(seg_x, seg_y, seg_w, 26, 6, 0xFF121B26);
    const char *scales[] = {"100%", "200%", "300%", "400%"};
    
    for (int i=0; i<4; i++) {
        int item_x = seg_x + i*(seg_w/4);
        if (state->scale_index == i) {
            gfx_draw_rounded_rect_filled(item_x, seg_y, seg_w/4, 26, 6, COLOR_CYAN);
            fb_draw_string_transparent(item_x + 8, seg_y + 5, scales[i], 0xFF000000);
        } else {
            if (state->hover_scale == i) {
                gfx_draw_rounded_rect_filled(item_x, seg_y, seg_w/4, 26, 6, 0xFF2A3A4C);
            }
            fb_draw_string_transparent(item_x + 8, seg_y + 5, scales[i], COLOR_TEXT);
        }
    }
    
    fb_draw_line(px + 20, py + 215, px + pw - 20, py + 215, BORDER_PANEL);
    
    // 6. Fractional Scaling
    fb_draw_string_transparent(px + 20, py + 230, "Fractional Scaling", COLOR_TEXT);
    fb_draw_string_transparent(px + 20, py + 250, "May increase power usage or reduce sharpness.", COLOR_MUTED);
    draw_toggle(px + pw - 60, py + 235, state->fractional_scale, state->hover_fractional);
    
    // Night Light Panel
    py += 300;
    color_t nl_bg = state->hover_night_light ? 0xFF2A3A4C : BG_PANEL;
    gfx_draw_rounded_rect_filled(px, py, pw, 50, 12, nl_bg);
    gfx_draw_rounded_rect(px, py, pw, 50, 12, BORDER_PANEL);
    fb_draw_string_transparent(px + 20, py + 17, "Night Light", COLOR_TEXT);
    fb_draw_string_transparent(px + pw - 70, py + 17, state->night_light ? "On >" : "Off >", COLOR_MUTED);
}

static void draw_placeholder_tab(struct window *win, const char *title, int cx, int cy, int cw, int ch) {
    (void)win;
    (void)ch;
    fb_draw_string_transparent(cx + (cw/2) - (strlen(title)*4), cy + 20, title, COLOR_TEXT);
    
    int px = cx + 40;
    int py = cy + 60;
    int pw = cw - 80;
    
    gfx_draw_rounded_rect_filled(px, py, pw, 100, 12, BG_PANEL);
    gfx_draw_rounded_rect(px, py, pw, 100, 12, BORDER_PANEL);
    
    fb_draw_string_transparent(px + 20, py + 40, "Coming soon to ZarqaaOS...", COLOR_MUTED);
}

static void settings_on_draw(struct window *win) {
    struct settings_state *state = (struct settings_state *)win->user_data;
    
    /* Main Background */
    fb_fill_rect(0, 0, win->width, win->height, BG_MAIN);

    /* Sidebar Layout */
    int sidebar_w = 180;
    gfx_draw_glass_panel(0, 0, sidebar_w, win->height, 0, BG_SIDEBAR, BG_SIDEBAR, 240);

    /* Sidebar Title */
    fb_draw_string_transparent(20, 20, "Settings", COLOR_TEXT);

    /* Tabs */
    const char *tabs[] = {
        "Wi-Fi", "Network", "Bluetooth", "Displays", 
        "Sound", "Power", "Appearance", "Apps", "About"
    };
    
    int tab_y = 60;
    for (int i = 0; i < NUM_TABS; i++) {
        color_t fg = COLOR_TEXT;
        
        if (state->active_tab == i) {
            gfx_draw_rounded_rect_filled(10, tab_y - 8, sidebar_w - 20, 32, 16, COLOR_CYAN);
            fg = 0xFF000000; // Black text on cyan
        } else if (state->hover_tab == i) {
            gfx_draw_rounded_rect_filled(10, tab_y - 8, sidebar_w - 20, 32, 16, 0xFF2A3A4C);
        }
        
        fb_draw_string_transparent(30, tab_y, tabs[i], fg);
        tab_y += 40;
    }

    /* Content Area */
    int cx = sidebar_w;
    int cy = 0;
    int cw = win->width - sidebar_w;
    int ch = win->height;

    switch (state->active_tab) {
        case TAB_APPEARANCE: draw_appearance_tab(win, state, cx, cy, cw, ch); break;
        case TAB_DISPLAYS:   draw_displays_tab(win, state, cx, cy, cw, ch); break;
        case TAB_ABOUT:      draw_placeholder_tab(win, "About ZarqaaOS", cx, cy, cw, ch); break;
        default:             draw_placeholder_tab(win, tabs[state->active_tab], cx, cy, cw, ch); break;
    }
}

static void settings_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    struct settings_state *state = (struct settings_state *)win->user_data;
    bool needs_redraw = false;

    int sidebar_w = 180;
    
    /* Check Tab Hover/Click */
    int new_hover_tab = -1;
    if (mx >= 10 && mx <= sidebar_w - 10) {
        for (int i = 0; i < NUM_TABS; i++) {
            int ty = 60 + i * 40 - 8;
            if (my >= ty && my <= ty + 32) {
                new_hover_tab = i;
                if (left && state->active_tab != i) {
                    state->active_tab = i;
                    needs_redraw = true;
                }
                break;
            }
        }
    }
    
    if (state->hover_tab != new_hover_tab) {
        state->hover_tab = new_hover_tab;
        needs_redraw = true;
    }

    /* Check Interactions inside Content Area */
    int cx = sidebar_w;
    int cw = win->width - sidebar_w;
    
    if (state->active_tab == TAB_APPEARANCE) {
        // 1. Apply button click / hover
        int btn_w = 180;
        int btn_h = 24;
        int btn_x = cx + (cw - btn_w) / 2;
        int btn_y = 46 + 130 + 20 + 18; // badge_y + 18

        bool hover_btn = (mx >= btn_x && mx <= btn_x + btn_w && my >= btn_y && my <= btn_y + btn_h);
        if (state->hover_apply_btn != hover_btn) {
            state->hover_apply_btn = hover_btn;
            needs_redraw = true;
        }
        if (hover_btn && left) {
            const struct wallpaper_info *info = wallpaper_get_info(state->selected_wallpaper_idx);
            if (info) {
                desktop_set_wallpaper_file(info->path);
                state->applied_toast = true;
                needs_redraw = true;
            }
        }

        // 2. Thumbnail Cards Grid click / hover
        int grid_y = btn_y + 32;
        int card_w = 100;
        int card_h = 68;
        int gap_x = 8;
        int gap_y = 6;
        int total_grid_w = 5 * card_w + 4 * gap_x;
        int grid_x = cx + (cw - total_grid_w) / 2;

        int new_hover_wp = -1;
        for (int i = 0; i < NUM_WALLPAPERS; i++) {
            int row = i / 5;
            int col = i % 5;
            int x = grid_x + col * (card_w + gap_x);
            int y = grid_y + row * (card_h + gap_y);

            if (mx >= x && mx <= x + card_w && my >= y && my <= y + card_h) {
                new_hover_wp = i;
                if (left && state->selected_wallpaper_idx != i) {
                    state->selected_wallpaper_idx = i;
                    state->applied_toast = false;
                    needs_redraw = true;
                }
                break;
            }
        }

        if (state->hover_wallpaper_idx != new_hover_wp) {
            state->hover_wallpaper_idx = new_hover_wp;
            needs_redraw = true;
        }
    } else if (state->active_tab == TAB_DISPLAYS) {
        int px = cx + 40;
        int py = 60;
        int pw = cw - 80;
        
        // Adjust for TV toggle
        int tog1_x = px + pw - 60;
        int tog1_y = py + 135;
        bool hover_t1 = (mx >= tog1_x && mx <= tog1_x + 40 && my >= tog1_y && my <= tog1_y + 20);
        if (state->hover_adjust_tv != hover_t1) { state->hover_adjust_tv = hover_t1; needs_redraw = true; }
        if (hover_t1 && left) { state->adjust_tv = !state->adjust_tv; needs_redraw = true; }
        
        // Scale segments
        int seg_w = 200;
        int seg_x = px + pw - seg_w - 20;
        int seg_y = py + 175;
        int new_hover_scale = -1;
        
        if (my >= seg_y && my <= seg_y + 26) {
            for (int i=0; i<4; i++) {
                int item_x = seg_x + i*(seg_w/4);
                if (mx >= item_x && mx < item_x + (seg_w/4)) {
                    new_hover_scale = i;
                    if (left && state->scale_index != i) {
                        state->scale_index = i;
                        needs_redraw = true;
                    }
                }
            }
        }
        if (state->hover_scale != new_hover_scale) { state->hover_scale = new_hover_scale; needs_redraw = true; }
        
        // Fractional Scaling toggle
        int tog2_x = px + pw - 60;
        int tog2_y = py + 235;
        bool hover_t2 = (mx >= tog2_x && mx <= tog2_x + 40 && my >= tog2_y && my <= tog2_y + 20);
        if (state->hover_fractional != hover_t2) { state->hover_fractional = hover_t2; needs_redraw = true; }
        if (hover_t2 && left) { state->fractional_scale = !state->fractional_scale; needs_redraw = true; }
        
        // Night Light Panel
        int nl_y = py + 300;
        bool hover_nl = (mx >= px && mx <= px + pw && my >= nl_y && my <= nl_y + 50);
        if (state->hover_night_light != hover_nl) { state->hover_night_light = hover_nl; needs_redraw = true; }
        if (hover_nl && left) { state->night_light = !state->night_light; needs_redraw = true; }
    }

    if (needs_redraw) {
        win->needs_redraw = true;
    }
}

static void settings_on_close(struct window *win) {
    if (win->user_data) kfree(win->user_data);
}

struct window *settings_open(void) {
    struct window *win = wm_create_window("Settings", 150, 80, 750, 520);
    if (!win) return NULL;

    struct settings_state *state = kcalloc(1, sizeof(struct settings_state));
    state->active_tab = TAB_APPEARANCE; // Open with Appearance tab so user immediately sees the wallpapers & preview!
    state->hover_tab = -1;
    state->scale_index = 1; // 200% default
    state->selected_wallpaper_idx = 0;
    state->hover_wallpaper_idx = -1;

    win->user_data = state;
    win->on_draw = settings_on_draw;
    win->on_mouse = settings_on_mouse;
    win->on_close = settings_on_close;
    
    return win;
}
