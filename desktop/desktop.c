/*
 * Zarqaa OS 0.1 - QuantumPeak Desktop Environment
 * Creator: Abdelhak Chabbi
 *
 * Faithfully reproduces the quantumpeak_os_simulation.html layout:
 *   - Left sidebar with 11 navigation items
 *   - Right tools panel with 5 utility items
 *   - Bottom dock with 9 application circles
 *   - Central compass widget + system score
 *   - Top header bar with search + clock + utility icons
 *   - Mountain gradient background
 */

#include "gui/dirty_rect.h"
#include "desktop/desktop.h"
#include "desktop/window_manager.h"
#include "desktop/taskbar.h"
#include "desktop/icons.h"
#include "drivers/framebuffer.h"
#include "drivers/usb/usb_hid.h"
#include "drivers/pit.h"
#include "gui/graphics.h"
#include "gui/layer.h"
#include "gui/wallpaper.h"
#include "gui/new_app_icons.h"
#include "desktop/cursor.h"
#include "gui/bitmap_icons.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"
#include "drivers/audio.h"
#include "filesystem/fat.h"
#include "kernel/scheduler.h"

/* Forward declarations for applications */
extern struct window *terminal_open(void);
extern struct window *file_manager_open(void);
extern struct window *text_editor_open(void);
extern struct window *task_manager_open(void);
extern struct window *settings_open(void);
extern struct window *calculator_open(void);
extern struct window *browser_open(void);
extern struct window *power_menu_open(void);
/* WiFi manager removed - no fake WiFi */
extern struct window *paint_open(void);
extern struct window *python_ide_open(void);

/* QuantumPeak color palette (from CSS variables) */
#define QP_BG_DARK    0x070b12
#define QP_BG_PANEL   0x141f30
#define QP_CYAN       0x00f0ff
#define QP_BLUE       0x3a86ff
#define QP_TEXT_MAIN  0xffffff
#define QP_TEXT_MUTED 0x80a0c4
#define QP_TEXT_DIM   0x546e8c
#define QP_BORDER     0x243b55
#define QP_MNT_DARK   0x0c121c
#define QP_MNT_MID    0x0f1723
#define QP_MNT_LIGHT  0x1c2838
#define QP_ACTIVE_BG  0x0c2333
#define QP_ACTIVE_HI  0x184a6b

/* ══════════════════════════════════════════════════════════
 *  HUD Button Data Structures
 * ══════════════════════════════════════════════════════════ */

struct hud_button {
    const uint32_t *bitmap;
    int bw, bh;
    char name[24];
    char desc[24];
    int app_id;
    
    // Magnetic Dock animation state
    float current_x;
    float target_x;
    float current_y;
    float target_y;
    bool is_dragging;
};

/* Left sidebar (macOS Vertical Dock): All 15 System Applications */
static struct hud_button left_buttons[] = {
    {icon_menu_browser,     ICON_MENU_SIZE, ICON_MENU_SIZE, "Browser",      "Web Engine",       APP_HTML_VIEWER,   0,0,0,0,false},
    {icon_menu_terminal,    ICON_MENU_SIZE, ICON_MENU_SIZE, "Terminal",     "Shell & CLI",      APP_TERMINAL,      0,0,0,0,false},
    {icon_menu_files,       ICON_MENU_SIZE, ICON_MENU_SIZE, "Files",        "File Manager",     APP_FILE_MANAGER,  0,0,0,0,false},
    {icon_menu_notepad,     ICON_MENU_SIZE, ICON_MENU_SIZE, "Notepad+",     "Text Editor",      APP_TEXT_EDITOR,   0,0,0,0,false},
    {icon_menu_studio,      ICON_MENU_SIZE, ICON_MENU_SIZE, "Studio",       "Drawing & Paint",  APP_PAINT,         0,0,0,0,false},
    {icon_menu_calculator,  ICON_MENU_SIZE, ICON_MENU_SIZE, "Calculator",   "Math & Logic",     APP_CALCULATOR,    0,0,0,0,false},
    {icon_menu_python_ide,  ICON_MENU_SIZE, ICON_MENU_SIZE, "Python IDE",   "Mini IDE",         APP_PYTHON_IDE,    0,0,0,0,false},
    {icon_menu_task_mgr,    ICON_MENU_SIZE, ICON_MENU_SIZE, "Task Mgr",     "System Monitor",   APP_TASK_MANAGER,  0,0,0,0,false},
    {icon_menu_gallery,     ICON_MENU_SIZE, ICON_MENU_SIZE, "Gallery",      "Image Viewer",     APP_IMAGE_VIEWER,  0,0,0,0,false},
    {icon_menu_settings,    ICON_MENU_SIZE, ICON_MENU_SIZE, "Settings",     "Preferences",      APP_SETTINGS,      0,0,0,0,false},
    {icon_menu_boot_log,    ICON_MENU_SIZE, ICON_MENU_SIZE, "Boot Log",     "System Logs",      APP_BOOT_LOGGER,   0,0,0,0,false},
    {icon_menu_devices,     ICON_MENU_SIZE, ICON_MENU_SIZE, "Devices",      "Hardware & PCI",   APP_DEVICE_MANAGER,0,0,0,0,false},
    {icon_menu_benchmark,   ICON_MENU_SIZE, ICON_MENU_SIZE, "Benchmark",    "Physics Engine",   APP_BENCHMARK,     0,0,0,0,false},
    {icon_menu_snake,       ICON_MENU_SIZE, ICON_MENU_SIZE, "Snake Game",   "Ring 3 Game",      APP_SNAKE,         0,0,0,0,false},
    {new_icon_menu[NEWICON_CLOCK],    ICON_MENU_SIZE, ICON_MENU_SIZE, "Clock",       "Time & Timer",     APP_CLOCK,         0,0,0,0,false},
    {new_icon_menu[NEWICON_CALENDAR], ICON_MENU_SIZE, ICON_MENU_SIZE, "Calendar",    "Month View",       APP_CALENDAR,      0,0,0,0,false},
    {new_icon_menu[NEWICON_SYSINFO],  ICON_MENU_SIZE, ICON_MENU_SIZE, "About",       "System Info",      APP_SYSINFO,       0,0,0,0,false},
    {new_icon_menu[NEWICON_2048],     ICON_MENU_SIZE, ICON_MENU_SIZE, "2048",        "Puzzle Game",      APP_GAME2048,      0,0,0,0,false},
    {new_icon_menu[NEWICON_MINES],    ICON_MENU_SIZE, ICON_MENU_SIZE, "Minesweeper", "Puzzle Game",      APP_MINESWEEPER,   0,0,0,0,false},
    {icon_menu_power,       ICON_MENU_SIZE, ICON_MENU_SIZE, "Power",        "Shutdown / Reset", APP_POWER,         0,0,0,0,false},
};
#define NUM_LEFT 20

/* Bottom dock: 7 productivity apps */
static struct hud_button dock_buttons[] = {
    {icon_dock_browser,     ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Browser",       "Web Engine",          APP_HTML_VIEWER,  0,0,0,0,false},
    {icon_dock_terminal,    ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Terminal",      "Command Line",        APP_TERMINAL,     0,0,0,0,false},
    {icon_dock_studio,      ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Studio",        "Zarqaa Studio",       APP_PAINT,        0,0,0,0,false},
    {icon_dock_notepad,     ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Notepad+",      "Text Editor",         APP_TEXT_EDITOR,  0,0,0,0,false},
    {icon_dock_calculator,  ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Calculator",    "Math Tool",           APP_CALCULATOR,   0,0,0,0,false},
    {icon_dock_gallery,     ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Gallery",       "Image Viewer",        APP_IMAGE_VIEWER, 0,0,0,0,false},
    {icon_dock_power,       ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Power",         "Management",          APP_POWER,        0,0,0,0,false},
};
#define NUM_DOCK 7

/* Right tools: 4 items */
static struct hud_button right_buttons[] = {
    {icon_menu_task_mgr,  ICON_MENU_SIZE, ICON_MENU_SIZE, "Task Mgr",  "Monitor", APP_TASK_MANAGER, 0,0,0,0,false},
    {icon_menu_boot_log,  ICON_MENU_SIZE, ICON_MENU_SIZE, "Boot Log",  "", APP_BOOT_LOGGER,         0,0,0,0,false},
    {icon_menu_devices,   ICON_MENU_SIZE, ICON_MENU_SIZE, "Devices",   "", APP_DEVICE_MANAGER,      0,0,0,0,false},
    {icon_menu_benchmark, ICON_MENU_SIZE, ICON_MENU_SIZE, "Benchmark", "", APP_BENCHMARK,           0,0,0,0,false},
};
#define NUM_RIGHT 4

/* System score (animated on click) */
static int system_score = 0;
static int score_target = 0;
static bool score_animating = false;

/* Applications Menu Popup Toggle (macOS Launchpad style) */
static bool app_menu_open = false;

/* Simple tick counter for animations */
static uint32_t tick_count = 0;

/* Layers */
struct layer *bg_layer;
struct layer *topbar_layer;
struct layer *dock_layer;
struct layer *left_sidebar_layer;
struct layer *right_sidebar_layer;

/* Mouse state snapshot for this frame */
static int frame_mx = 0;
static int frame_my = 0;
static bool frame_left_clicked = false;

/* Previous mouse position for dirty rect updates */
static int old_mx = -1, old_my = -1;
static bool force_redraw = true;
static bool last_max_state = false;

static char to_lower_char(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static const char* desktop_strcasestr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return haystack;
    
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && to_lower_char(*h) == to_lower_char(*n)) {
            h++;
            n++;
        }
        if (*n == '\0') return haystack; // Found
        haystack++;
    }
    return NULL;
}

/* Search Bar State */
#define SEARCH_MAX_LEN 64
static char search_text[SEARCH_MAX_LEN] = {0};
static int search_cursor_pos = 0;
static int search_sel_start = 0;
static int search_sel_end = 0;
static bool search_focused = false;
static bool search_is_selecting = false;

/* ══════════════════════════════════════════════════════════
 *  Initialization
 * ══════════════════════════════════════════════════════════ */

void desktop_init(void) {
    new_app_icons_init();
    layer_init();
    
    int w = fb_get_width();
    int h = fb_get_height();
    
    bg_layer = layer_create(0, 0, w, h, 0);
    fb_set_render_target(bg_layer->buffer, w, h, w * 4);
    
    int copy_w = w < WALLPAPER_WIDTH ? w : WALLPAPER_WIDTH;
    int copy_h = h < WALLPAPER_HEIGHT ? h : WALLPAPER_HEIGHT;
    
    /* Fill beyond wallpaper with dark color */
    for (int y = 0; y < h; y++) {
        uint32_t *dst = bg_layer->buffer + y * w;
        for (int x = 0; x < w; x++) {
            dst[x] = 0xFF0A101D;
        }
    }
    
    /* Copy wallpaper row by row */
    for (int y = 0; y < copy_h; y++) {
        uint32_t *dst = bg_layer->buffer + y * w;
        const uint32_t *src = wallpaper + y * WALLPAPER_WIDTH;
        for (int x = 0; x < copy_w; x++) {
            dst[x] = src[x] | 0xFF000000;
        }
    }
    fb_reset_render_target();
    bg_layer->needs_redraw = false;

    mouse_set_bounds(w, h);
    cursor_init();
    wm_init();
    
    /* Force a full screen redraw on first frame so we overwrite the boot text */
    dirty_rect_add(0, 0, w, h);
    
    /* Audio ready - no startup beep on real hardware */
    audio_init();
}

/* ══════════════════════════════════════════════════════════
 *  Dynamic Wallpaper Engine
 * ══════════════════════════════════════════════════════════ */

static char current_wallpaper_path[128] = "Mountain Peak";

const char *desktop_get_current_wallpaper(void) {
    return current_wallpaper_path;
}

bool desktop_set_wallpaper_pixels(const uint32_t *pixels, int img_w, int img_h) {
    if (!pixels || img_w <= 0 || img_h <= 0 || !bg_layer || !bg_layer->buffer) return false;
    int w = fb_get_width();
    int h = fb_get_height();
    if (w <= 0 || h <= 0) return false;

    /* Scaled blit directly to background layer */
    for (int y = 0; y < h; y++) {
        int src_y = (y * img_h) / h;
        if (src_y >= img_h) src_y = img_h - 1;
        uint32_t *dst = bg_layer->buffer + y * w;
        const uint32_t *src_row = pixels + src_y * img_w;
        for (int x = 0; x < w; x++) {
            int src_x = (x * img_w) / w;
            if (src_x >= img_w) src_x = img_w - 1;
            dst[x] = src_row[src_x] | 0xFF000000;
        }
    }

    bg_layer->needs_redraw = true;
    dirty_rect_add(0, 0, w, h);
    return true;
}

bool desktop_set_wallpaper_file(const char *vfs_path) {
    if (!vfs_path || !vfs_path[0]) return false;
    extern uint32_t *gfx_load_image(const char *path, int *out_w, int *out_h);
    int img_w = 0, img_h = 0;
    uint32_t *pixels = gfx_load_image(vfs_path, &img_w, &img_h);
    if (!pixels) return false;

    bool res = desktop_set_wallpaper_pixels(pixels, img_w, img_h);
    if (res) {
        strncpy(current_wallpaper_path, vfs_path, sizeof(current_wallpaper_path) - 1);
        current_wallpaper_path[sizeof(current_wallpaper_path) - 1] = '\0';
    }
    kfree(pixels);
    return res;
}

/* ══════════════════════════════════════════════════════════
 *  App Launch
 * ══════════════════════════════════════════════════════════ */

/* Frames to block clicks after launching an app — prevents the same click
 * that opened an app from immediately hitting its close button. */
static int launch_click_cooldown = 0;

void desktop_launch_app(int app_id) {
    const char *title = NULL;
    struct window *(*open_func)(void) = NULL;

    switch (app_id) {
extern struct window *quantum_viewer_open(void);
extern void debug_console_open(void);
extern struct window *boot_logger_open(void);
extern struct window *device_manager_open(void);
extern struct window *image_viewer_open(void);
extern struct window *benchmark_open(void);
extern struct window *clock_app_open(void);
extern struct window *calendar_open(void);
extern struct window *sysinfo_open(void);
extern struct window *game2048_open(void);
extern struct window *minesweeper_open(void);
        case APP_TERMINAL:     title = "Terminal";     open_func = terminal_open;     break;
        case APP_FILE_MANAGER: title = "File Manager"; open_func = file_manager_open; break;
        case APP_TEXT_EDITOR:  title = "Text Editor";  open_func = text_editor_open;  break;
        case APP_TASK_MANAGER: title = "Task Manager"; open_func = task_manager_open; break;
        case APP_SETTINGS:     title = "Settings";     open_func = settings_open;     break;
        case APP_CALCULATOR:   title = "ZUI Calculator"; open_func = calculator_open;   break;
        case APP_HTML_VIEWER:  title = "Zarqaa Browser";  open_func = browser_open;  break;
        case APP_QUANTUM_VIEWER: title = "Zarqaa Browser"; open_func = browser_open; break;
        case APP_PYTHON_IDE:   title = "Python IDE";   open_func = python_ide_open;   break;
        case APP_POWER:        title = "Power";        open_func = power_menu_open;   break;
        /* APP_WIFI removed - no fake WiFi */
        case 116:              title = "Zarqaa Studio"; open_func = paint_open;        break;
        case APP_DEBUG_CONSOLE:title = "Debug Console";open_func = (struct window *(*)(void))debug_console_open; break;
        case APP_BOOT_LOGGER:  title = "Boot Logger";  open_func = boot_logger_open;  break;
        case APP_DEVICE_MANAGER:title = "Device Manager";open_func = device_manager_open; break;
        case APP_IMAGE_VIEWER: title = "Zarqaa Gallery"; open_func = image_viewer_open; break;
        case APP_BENCHMARK:    title = "Physics Benchmark"; open_func = benchmark_open; break;
        case APP_CLOCK:        title = "Zarqaa Clock";    open_func = clock_app_open;   break;
        case APP_CALENDAR:     title = "Zarqaa Calendar"; open_func = calendar_open;    break;
        case APP_SYSINFO:      title = "About Zarqaa OS"; open_func = sysinfo_open;     break;
        case APP_GAME2048:     title = "2048";            open_func = game2048_open;    break;
        case APP_MINESWEEPER:  title = "Minesweeper";     open_func = minesweeper_open; break;
        case APP_SNAKE: {
            title = "Terminal";
            open_func = terminal_open;
            extern int process_exec(const char *path, const char *name);
            process_exec("/hd0/snake.elf", "snake");
            break;
        }
    }

    if (title && open_func) {
        kprintf("[Desktop] Requesting launch for app '%s' (ID: %d)\n", title, app_id);
        struct window *win = wm_find_window_by_title(title);
        if (win) {
            kprintf("[Desktop] App '%s' already running, focusing window (ID: %d)\n", title, win->id);
            if (win->minimized) {
                win->minimized = false;
                dirty_rect_add(win->x - 5, win->y - TITLEBAR_HEIGHT - 5, win->width + 10, win->height + TITLEBAR_HEIGHT + 10);
            }
            wm_focus_window(win);
        } else {
            kprintf("[Desktop] App '%s' not running, calling open_func()\n", title);
            open_func();
            /* Block clicks for 8 frames so the launch-click can't close the new window */
            launch_click_cooldown = 8;
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  Hit Testing Helpers
 * ══════════════════════════════════════════════════════════ */

static bool hit_test_circle(int mx, int my, int cx, int cy, int r) {
    int dx = mx - cx; 
    int dy = my - cy;
    return (dx*dx + dy*dy <= r*r);
}

static bool hit_test_rect(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx < x + w && my >= y && my < y + h);
}

/* ══════════════════════════════════════════════════════════
 *  Top Header Bar
 * ══════════════════════════════════════════════════════════ */

static void draw_header(int w) {
    if (!fb_intersects_clip(0, 0, w, fb_get_height())) return;

    bool is_max = wm_has_maximized_window();

    if (search_focused) {
        // Draw fullscreen dim/blur overlay
        gfx_draw_glass_panel(0, 0, w, fb_get_height(), 0, 0x050A15, 0x050A15, 180);
    }

    /* ── Real-time Clock formatting ── */
    char clk[9];
    {
        extern int64_t boot_time;
        extern uint32_t pit_get_seconds(void);
        uint64_t now = (uint64_t)boot_time + pit_get_seconds();
        uint32_t ss = (uint32_t)(now % 60);
        uint32_t mm = (uint32_t)((now / 60) % 60);
        uint32_t hh = (uint32_t)((now / 3600) % 24);

        clk[0] = '0' + (hh / 10);
        clk[1] = '0' + (hh % 10);
        clk[2] = ':';
        clk[3] = '0' + (mm / 10);
        clk[4] = '0' + (mm % 10);
        clk[5] = ':';
        clk[6] = '0' + (ss / 10);
        clk[7] = '0' + (ss % 10);
        clk[8] = '\0';
    }

    /* ── DYNAMIC CLEAN MODE (When any window is maximized) ── */
    if (is_max) {
        // Draw sleek minimal top bar
        fb_fill_rect(0, 0, w, 30, 0xFF08111C);
        fb_draw_line(0, 29, w, 29, 0xFF1B334E);

        // Applications Button (Compact)
        bool hover_app_btn = hit_test_rect(frame_mx, frame_my, 10, 4, 125, 22);
        color_t btn_bg = (app_menu_open || hover_app_btn) ? QP_ACTIVE_BG : QP_BG_PANEL;
        color_t btn_border = (app_menu_open || hover_app_btn) ? QP_CYAN : QP_BORDER;
        gfx_draw_glass_panel(10, 4, 125, 22, 6, btn_bg, btn_border, 180);
        icon_draw_shape(18, 15, 6, ICON_HUD_CUBE, (app_menu_open || hover_app_btn) ? QP_CYAN : QP_TEXT_MAIN);
        fb_draw_string_transparent(30, 7, "Applications", (app_menu_open || hover_app_btn) ? QP_TEXT_MAIN : 0xd1e0f0);

        // Clock in center
        fb_draw_string_transparent((w - 64) / 2, 7, clk, QP_CYAN);

        // Power icon on far right
        icon_draw_shape(w - 24, 15, 7, ICON_HUD_POWER, QP_TEXT_MUTED);
        return;
    }

    /* ── NORMAL WINDOWED MODE (Full Rich Top Bar) ── */

    /* ── Applications Button (macOS Launchpad Style) ── */
    bool hover_app_btn = hit_test_rect(frame_mx, frame_my, 20, 22, 135, 34);
    color_t btn_bg = (app_menu_open || hover_app_btn) ? QP_ACTIVE_BG : QP_BG_PANEL;
    color_t btn_border = (app_menu_open || hover_app_btn) ? QP_CYAN : QP_BORDER;
    gfx_draw_glass_panel(20, 22, 135, 34, 10, btn_bg, btn_border, 180);
    icon_draw_shape(28 + 8, 26 + 8, 8, ICON_HUD_CUBE, (app_menu_open || hover_app_btn) ? QP_CYAN : QP_TEXT_MAIN);
    fb_draw_string_transparent(54, 32, "Applications", (app_menu_open || hover_app_btn) ? QP_TEXT_MAIN : 0xd1e0f0);

    /* Search bar (glass panel, next to Applications button) */
    gfx_draw_glass_panel(165, 22, 210, 34, 10, search_focused ? 0xFF2A3F5C : QP_BG_PANEL, search_focused ? QP_CYAN : QP_BORDER, 150);
    icon_draw_shape(175 + 8, 26 + 8, 8, ICON_HUD_SEARCH, search_focused ? QP_CYAN : QP_TEXT_DIM);
    
    // Draw search text with selection and cursor
    if (strlen(search_text) == 0 && !search_focused) {
        fb_draw_string_transparent(200, 32, "Search...", QP_TEXT_DIM);
    } else {
        int sel_min = search_sel_start;
        int sel_max = search_sel_end;
        if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }
        
        int text_len = strlen(search_text);
        int draw_x = 200;
        int draw_y = 32;
        
        for (int i = 0; i < text_len; i++) {
            char str[2] = { search_text[i], 0 };
            bool is_selected = (i >= sel_min && i < sel_max);
            
            if (is_selected) {
                fb_fill_rect(draw_x, draw_y - 2, 8, 20, 0xFF0078D7); // Windows Blue Highlight
                fb_draw_string_transparent(draw_x, draw_y, str, 0xFFFFFFFF);
            } else {
                fb_draw_string_transparent(draw_x, draw_y, str, QP_TEXT_MAIN);
            }
            draw_x += 8; // Approx width of font char
        }
        
        // Draw blinking cursor
        if (search_focused && sel_min == sel_max && (tick_count % 30 < 15)) {
            int cursor_x = 200 + (search_cursor_pos * 8);
            fb_fill_rect(cursor_x, draw_y - 2, 2, 16, QP_CYAN);
        }
    }
    
    // Draw search results dropdown if active
    if (search_focused && strlen(search_text) > 0) {
        // Find matching apps
        struct hud_button *matches[20];
        int match_count = 0;
        
        // Search dock
        for (int i = 0; i < NUM_DOCK; i++) {
            if (dock_buttons[i].app_id != -1 && 
                (desktop_strcasestr(dock_buttons[i].name, search_text) || desktop_strcasestr(dock_buttons[i].desc, search_text))) {
                matches[match_count++] = &dock_buttons[i];
                if (match_count >= 20) break;
            }
        }
        // Search left sidebar
        for (int i = 0; i < NUM_LEFT && match_count < 20; i++) {
            if (left_buttons[i].app_id != -1 && 
                (desktop_strcasestr(left_buttons[i].name, search_text) || desktop_strcasestr(left_buttons[i].desc, search_text))) {
                
                // Avoid duplicates
                bool dup = false;
                for (int j = 0; j < match_count; j++) {
                    if (matches[j]->app_id == left_buttons[i].app_id) { dup = true; break; }
                }
                if (!dup) matches[match_count++] = &left_buttons[i];
            }
        }
        
        if (match_count > 0) {
            int dropdown_h = match_count * 40;
            gfx_draw_glass_panel(30, 65, 280, dropdown_h, 8, QP_BG_PANEL, QP_BORDER, 200);
            for (int i = 0; i < match_count; i++) {
                int item_y = 65 + i * 40;
                bool hover = hit_test_rect(frame_mx, frame_my, 30, item_y, 280, 40);
                if (hover) {
                    fb_fill_rect(32, item_y + 2, 276, 36, QP_ACTIVE_BG);
                }
                fb_draw_string_transparent(40, item_y + 12, matches[i]->name, hover ? QP_CYAN : QP_TEXT_MAIN);
                fb_draw_string_transparent(160, item_y + 12, matches[i]->desc, QP_TEXT_DIM);
            }
        } else {
            gfx_draw_glass_panel(30, 65, 280, 40, 8, QP_BG_PANEL, QP_BORDER, 200);
            fb_draw_string_transparent(40, 65 + 12, "No results found", QP_TEXT_DIM);
        }
    }

    /* Right side utilities */
    int rx = w - 380;
    
    // Volume Controls
    int vol = audio_get_volume();
    bool muted = audio_is_muted();
    
    // Vol Down (-)
    bool hover_vdown = hit_test_circle(frame_mx, frame_my, rx + 15, 39, 15);
    gfx_draw_circle(rx + 15, 39, 14, hover_vdown ? QP_CYAN : QP_BORDER);
    fb_draw_string_transparent(rx + 11, 35, "-", hover_vdown ? QP_TEXT_MAIN : QP_TEXT_MUTED);
    
    // Vol Level
    char vbuf[4];
    itoa(vol, vbuf, 10);
    fb_draw_string_transparent(rx + 33, 35, muted ? " M" : vbuf, muted ? 0xFA5C5C : QP_CYAN);
    
    // Vol Up (+)
    bool hover_vup = hit_test_circle(frame_mx, frame_my, rx + 65, 39, 15);
    gfx_draw_circle(rx + 65, 39, 14, hover_vup ? QP_CYAN : QP_BORDER);
    fb_draw_string_transparent(rx + 61, 35, "+", hover_vup ? QP_TEXT_MAIN : QP_TEXT_MUTED);
    
    // Speaker Mute Toggle
    int cx = rx + 105;
    int cy = 39;
    bool hover_mute = hit_test_circle(frame_mx, frame_my, cx, cy, 15);
    color_t c = muted ? 0xFA5C5C : (hover_mute ? QP_TEXT_MAIN : QP_TEXT_MUTED);
    gfx_draw_circle(cx, cy, 14, hover_mute ? QP_CYAN : QP_BORDER);
    fb_fill_rect(cx - 5, cy - 3, 4, 6, c);
    gfx_draw_line_aa(cx - 1, cy - 3, cx + 3, cy - 6, c);
    gfx_draw_line_aa(cx - 1, cy + 3, cx + 3, cy + 6, c);
    gfx_draw_line_aa(cx + 3, cy - 6, cx + 3, cy + 6, c);
    if (muted) {
        gfx_draw_line_aa(cx + 5, cy - 4, cx + 11, cy + 4, c);
        gfx_draw_line_aa(cx + 5, cy + 4, cx + 11, cy - 4, c);
    } else {
        gfx_draw_circle(cx + 1, cy, 5, c); // simple arcs
    }


    /* ── Real-time Clock in the center of the topbar ── */
    {
        extern int64_t boot_time;
        uint64_t now = (uint64_t)boot_time + (uint64_t)pit_get_seconds();
        int ss = (int)(now % 60);
        int mm = (int)((now / 60) % 60);
        int hh = (int)((now / 3600) % 24);

        /* Format: HH:MM:SS */
        char clk[12];
        clk[0]  = '0' + (hh / 10);
        clk[1]  = '0' + (hh % 10);
        clk[2]  = ':';
        clk[3]  = '0' + (mm / 10);
        clk[4]  = '0' + (mm % 10);
        clk[5]  = ':';
        clk[6]  = '0' + (ss / 10);
        clk[7]  = '0' + (ss % 10);
        clk[8]  = '\0';

        /* Draw a small glass pill in the center */
        int clk_w = 72;  /* 8 chars * 8 px + 8 px padding */
        int clk_x = (w - clk_w) / 2;
        int clk_y = 20;
        gfx_draw_glass_panel(clk_x, clk_y, clk_w, 26, 13, QP_BG_PANEL, QP_BORDER, 160);
        fb_draw_string_transparent(clk_x + 4, clk_y + 8, clk, QP_CYAN);
    }
}

/* ══════════════════════════════════════════════════════════
 *  Vertical macOS-style Dock (Applications Menu Popup)
 * ══════════════════════════════════════════════════════════ */

static void draw_left_sidebar(void) {
    if (!app_menu_open) return; // Only draw when Applications icon is clicked!

    int w = fb_get_width();
    int h = fb_get_height();
    (void)w;
    
    int dock_w = 220;
    int dock_h = h - 85;
    if (dock_h < 500) dock_h = 500;
    
    if (!fb_intersects_clip(5, 60, dock_w + 15, dock_h + 30)) return;

    int sx = 14;
    int sy = 95;
    int gap = (dock_h - 45) / NUM_LEFT;
    if (gap > 44) gap = 44;
    if (gap < 35) gap = 35;

    // Draw macOS Glassmorphism Dock Panel Popup
    gfx_draw_glass_panel(sx - 4, 62, dock_w, NUM_LEFT * gap + 42, 14, QP_BG_PANEL, QP_CYAN, 210);
    
    // Header title in popup
    icon_draw_shape(sx + 10, 75, 7, ICON_HUD_CUBE, QP_CYAN);
    fb_draw_string_transparent(sx + 24, 71, "All Applications", QP_TEXT_MAIN);
    
    // Close button [×]
    bool hover_close = hit_test_rect(frame_mx, frame_my, sx + dock_w - 28, 67, 20, 20);
    fb_draw_string_transparent(sx + dock_w - 22, 70, "x", hover_close ? 0xFA5C5C : QP_TEXT_MUTED);

    // Separator line
    fb_fill_rect(sx + 6, 90, dock_w - 20, 1, QP_BORDER);

    for (int i = 0; i < NUM_LEFT; i++) {
        int ix = sx;
        int iy = sy + i * gap;
        int item_w = dock_w - 12;
        int item_h = gap - 3;
        int icon_cx = ix + 18;
        int icon_cy = iy + item_h / 2;
        bool hover = hit_test_rect(frame_mx, frame_my, ix, iy, item_w, item_h);

        // Check if application window is currently running
        bool is_running = false;
        if (left_buttons[i].name[0]) {
            if (wm_find_window_by_title(left_buttons[i].name) != NULL) {
                is_running = true;
            } else if (strcmp(left_buttons[i].name, "Files") == 0 && wm_find_window_by_title("File Manager")) {
                is_running = true;
            } else if (strcmp(left_buttons[i].name, "Browser") == 0 && wm_find_window_by_title("Zarqaa Browser")) {
                is_running = true;
            } else if (strcmp(left_buttons[i].name, "Notepad+") == 0 && wm_find_window_by_title("Text Editor")) {
                is_running = true;
            } else if (strcmp(left_buttons[i].name, "Studio") == 0 && wm_find_window_by_title("Zarqaa Studio")) {
                is_running = true;
            } else if (strcmp(left_buttons[i].name, "Gallery") == 0 && wm_find_window_by_title("Zarqaa Gallery")) {
                is_running = true;
            } else if (strcmp(left_buttons[i].name, "Calculator") == 0 && wm_find_window_by_title("ZUI Calculator")) {
                is_running = true;
            } else if (strcmp(left_buttons[i].name, "Benchmark") == 0 && wm_find_window_by_title("Physics Benchmark")) {
                is_running = true;
            }
        }

        // Draw hover effect
        if (hover) {
            gfx_draw_glass_panel(ix, iy, item_w, item_h, 8, QP_ACTIVE_BG, QP_CYAN, 220);
        }

        // Running Indicator Dot (macOS Style)
        if (is_running) {
            gfx_draw_circle_filled(ix + 4, icon_cy, 3, QP_CYAN);
            gfx_draw_circle_aa(ix + 4, icon_cy, 4, QP_TEXT_MAIN);
        }

        // Squircle Icon Badge (Smooth Anti-Aliased)
        gfx_draw_glass_panel(icon_cx - 14, icon_cy - 14, 28, 28, 7, hover ? 0x1f3b5c : 0x142236, hover ? QP_CYAN : QP_BORDER, 220);

        color_t title_color = hover ? QP_TEXT_MAIN : (is_running ? 0xffffff : 0xd1e0f0);

        if (left_buttons[i].bitmap) {
            gfx_draw_bitmap_rgba_centered(icon_cx, icon_cy, left_buttons[i].bitmap, left_buttons[i].bw, left_buttons[i].bh);
        } else {
            icon_draw_shape(icon_cx, icon_cy, 9, ICON_HUD_MONITOR, hover ? QP_CYAN : title_color);
        }

        // App Name & Subtitle (Cleanly Aligned)
        if (gap >= 39 && left_buttons[i].desc[0]) {
            fb_draw_string_transparent(ix + 38, iy + (item_h / 2) - 10, left_buttons[i].name, title_color);
            fb_draw_string_transparent(ix + 38, iy + (item_h / 2) + 2, left_buttons[i].desc, QP_TEXT_DIM);
        } else {
            fb_draw_string_transparent(ix + 38, iy + (item_h / 2) - 6, left_buttons[i].name, title_color);
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  Right Tools Panel
 * ══════════════════════════════════════════════════════════ */

static void draw_right_panel(int w) {
    if (!fb_intersects_clip(w - 260, 100, 260, 500)) return;

    int sx = w - 240;
    int sy = 100;
    int gap = 56;

    gfx_draw_dashed_line(w - 256, 90, w - 256, 90 + NUM_RIGHT * gap, QP_BORDER, 4, 4);
    fb_fill_rect(w - 258, 150, 4, 60, QP_CYAN);

    for (int i = 0; i < NUM_RIGHT; i++) {
        int iy = sy + i * gap;
        int icon_cx = w - 50;
        int icon_cy = iy + 20;
        bool hover = hit_test_rect(frame_mx, frame_my, sx, iy, w - sx, 48);

        int label_len = strlen(right_buttons[i].name);
        fb_draw_string_transparent(icon_cx - 30 - label_len * 8, iy + 12,
                                   right_buttons[i].name,
                                   hover ? QP_TEXT_MAIN : 0xd1e0f0);

        gfx_draw_circle(icon_cx, icon_cy, 20, hover ? QP_TEXT_MAIN : QP_BORDER);
        if (right_buttons[i].bitmap) {
            gfx_draw_bitmap_rgba_centered(icon_cx, icon_cy, right_buttons[i].bitmap, right_buttons[i].bw, right_buttons[i].bh);
        } else {
            icon_draw_shape(icon_cx, icon_cy, 12, ICON_HUD_MONITOR, hover ? QP_TEXT_MAIN : 0xd1e0f0);
        }

    }
}

/* ══════════════════════════════════════════════════════════
 *  Central Compass + System Score
 * ══════════════════════════════════════════════════════════ */

static void draw_central_hud(int w, int h) {
    if (!fb_intersects_clip(w/2 - 200, h/2 - 120, 400, 240)) return;

    int cx = w / 2;
    int cy = h / 2 - 40;

    int ring_offset = (tick_count / 2) % 360;
    (void)ring_offset;

    gfx_draw_circle(cx, cy, 120, QP_BORDER);
    gfx_draw_circle(cx, cy, 100, 0x162538);
    
    gfx_draw_circle(cx, cy, 48, QP_CYAN);
    gfx_draw_circle(cx, cy, 47, QP_CYAN);
    gfx_draw_circle_filled(cx, cy, 44, QP_BG_DARK);

    for (int dy = -15; dy <= 15; dy++) {
        int half_w = (15 - (dy < 0 ? -dy : dy)) / 2;
        for (int dx = -half_w; dx <= half_w; dx++) {
            fb_putpixel(cx + dx, cy + dy, QP_TEXT_MAIN);
        }
    }

    int live_cpu = scheduler_get_cpu_usage();
    int live_score = 100 - live_cpu;
    if (live_score < 5) live_score = 5;
    if (live_score > 99) live_score = 99;

    int sx = cx - 180;
    int sy = cy;
    gfx_draw_progress_ring(sx, sy, 28, live_score, QP_BORDER, QP_CYAN);

    char score_buf[8];
    itoa(live_score, score_buf, 10);
    int score_len = strlen(score_buf);
    fb_draw_string_transparent(sx - score_len * 4, sy - 6, score_buf, QP_TEXT_MAIN);
    fb_draw_string_transparent(sx - 24, sy + 38, "System", QP_TEXT_MUTED);
    fb_draw_string_transparent(sx - 20, sy + 52, "Score", QP_TEXT_MUTED);

    int rx = cx + 160;
    fb_draw_string_transparent(rx - 24, sy - 20, "SYSTEM", QP_TEXT_MAIN);
    fb_draw_string_transparent(rx - 24, sy - 4,  "UPDATE", QP_TEXT_MAIN);
    gfx_draw_glass_panel(rx - 40, sy + 16, 80, 24, 12, QP_BG_PANEL, QP_BORDER, 150);
    fb_draw_string_transparent(rx - 34, sy + 22, "BACKUP", QP_TEXT_MUTED);
    gfx_draw_circle_filled(rx - 34, sy + 56, 3, QP_CYAN);
    
    char cpu_hud_str[16];
    itoa(live_cpu, cpu_hud_str, 10);
    strcat(cpu_hud_str, "% CPU");
    fb_draw_string_transparent(rx - 24, sy + 50, cpu_hud_str, QP_CYAN);

    fb_draw_line(cx - 130, cy, cx - 200, cy, QP_BORDER);
    fb_draw_line(cx - 200, cy, cx - 200, cy + 80, QP_BORDER);
    gfx_draw_circle_filled(cx - 200, cy + 80, 2, QP_CYAN);
    
    fb_draw_line(cx + 130, cy - 20, cx + 200, cy - 20, QP_BORDER);
    gfx_draw_circle_filled(cx + 200, cy - 20, 2, QP_CYAN);

    if (score_animating && system_score < score_target) {
        system_score++;
        if (system_score >= score_target) score_animating = false;
    }
}

/* ══════════════════════════════════════════════════════════
 *  Bottom Dock
 * ══════════════════════════════════════════════════════════ */

static void recalc_dock_targets(void) {
    int w = fb_get_width();
    int spacing = 85;
    int total_w = (NUM_DOCK - 1) * spacing;
    int start_x = w / 2 - total_w / 2;
    for (int i = 0; i < NUM_DOCK; i++) {
        dock_buttons[i].target_x = start_x + i * spacing;
        if (dock_buttons[i].current_x < 1.0f) { // First time setup
            dock_buttons[i].current_x = dock_buttons[i].target_x;
        }
    }
}

/* Called ONCE per frame - updates animation state and adds dirty rects */
static void update_dock_animation(int w, int h) {
    (void)w;
    (void)h;
    recalc_dock_targets();
    int dock_y = h - 100;
    for (int i = 0; i < NUM_DOCK; i++) {
        if (!dock_buttons[i].is_dragging) {
            float diff = dock_buttons[i].target_x - dock_buttons[i].current_x;
            if (diff < -0.5f || diff > 0.5f) {
                /* Add dirty rect for old position */
                int old_cx = (int)dock_buttons[i].current_x;
                dirty_rect_add(old_cx - 50, dock_y - 50, 100, 110);
                dock_buttons[i].current_x += diff * 0.25f;
                /* Add dirty rect for new position */
                int new_cx = (int)dock_buttons[i].current_x;
                dirty_rect_add(new_cx - 50, dock_y - 50, 100, 110);
            } else {
                /* Snap to target */
                dock_buttons[i].current_x = dock_buttons[i].target_x;
            }
        }
    }
}

bool desktop_is_app_running(int app_id, bool *out_minimized) {
    const char *title = NULL;
    switch (app_id) {
        case APP_TERMINAL:     title = "Terminal";     break;
        case APP_FILE_MANAGER: title = "File Manager"; break;
        case APP_TEXT_EDITOR:  title = "Text Editor";  break;
        case APP_TASK_MANAGER: title = "Task Manager"; break;
        case APP_SETTINGS:     title = "Settings";     break;
        case APP_CALCULATOR:   title = "ZUI Calculator"; break;
        case APP_HTML_VIEWER:  title = "Zarqaa Browser";  break;
        case APP_QUANTUM_VIEWER: title = "Zarqaa Browser"; break;
        case APP_PYTHON_IDE:   title = "Python IDE";   break;
        case APP_POWER:        title = "Power";        break;
        case APP_DEBUG_CONSOLE:title = "Debug Console"; break;
        case APP_BOOT_LOGGER:  title = "Boot Logger";  break;
        case APP_DEVICE_MANAGER:title = "Device Manager"; break;
        case APP_IMAGE_VIEWER: title = "Zarqaa Gallery"; break;
        case APP_BENCHMARK:    title = "Physics Benchmark"; break;
        case APP_SNAKE:        title = "Snake";        break;
        case APP_CLOCK:        title = "Zarqaa Clock";    break;
        case APP_CALENDAR:     title = "Zarqaa Calendar"; break;
        case APP_SYSINFO:      title = "About Zarqaa OS"; break;
        case APP_GAME2048:     title = "2048";            break;
        case APP_MINESWEEPER:  title = "Minesweeper";     break;
    }
    if (!title) return false;
    struct window *win = wm_find_window_by_title(title);
    if (win) {
        if (out_minimized) *out_minimized = win->minimized;
        return true;
    }
    return false;
}

static void draw_dock(int w, int h) {
    (void)w;
    int dock_y = h - 100;
    int circle_r = 30;
    /* recalc_dock_targets already called by update_dock_animation */
    recalc_dock_targets();

    // Draw unified continuous dock background panel
    int spacing = 85;
    int total_w = (NUM_DOCK - 1) * spacing;
    int dock_bg_w = total_w + 90;
    int dock_bg_x = w / 2 - dock_bg_w / 2;
    int dock_bg_y = dock_y - 35;
    int dock_bg_h = 70;
    
    // Draw modern semi-transparent glass panel background (alpha=200)
    gfx_draw_glass_panel(dock_bg_x, dock_bg_y, dock_bg_w, dock_bg_h, 24, 0x0D1826, 0x1E354F, 200);

    // Draw normal icons first
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < NUM_DOCK; i++) {
            if (pass == 0 && dock_buttons[i].is_dragging) continue;
            if (pass == 1 && !dock_buttons[i].is_dragging) continue;

            int cx = (int)dock_buttons[i].current_x;
            int cy = dock_buttons[i].is_dragging ? (int)dock_buttons[i].current_y : dock_y;
            
            bool hover = dock_buttons[i].is_dragging || hit_test_circle(frame_mx, frame_my, cx, cy, circle_r + 5);
            bool is_active = (i == 0);

            // Highlight badge for hover or active
            if (is_active || hover) {
                // We use alpha=150 for the highlight badge
                gfx_draw_glass_panel(cx - 27, cy - 27, 54, 54, 14, 
                                     is_active ? 0x14283D : 0x1E354F, 
                                     QP_CYAN, 150);
            }

            if (dock_buttons[i].bitmap) {
                gfx_draw_bitmap_rgba_centered(cx, cy, dock_buttons[i].bitmap, dock_buttons[i].bw, dock_buttons[i].bh);
            } else {
                icon_draw_shape(cx, cy, 18, ICON_HUD_APPS, (is_active || hover) ? QP_TEXT_MAIN : QP_TEXT_MUTED);
            }
            
            bool is_minimized = false;
            if (desktop_is_app_running(dock_buttons[i].app_id, &is_minimized)) {
                // Draw a glowing indicator below the icon
                gfx_draw_circle_filled(cx, cy + 24, 3, is_minimized ? 0xFF00FFFF : 0xFF00FF00); // cyan if minimized, green if open
            }

        if (hover || is_active) {
            int tl = strlen(dock_buttons[i].name);
            fb_draw_string_transparent(cx - tl * 4, cy + circle_r + 12,
                                       dock_buttons[i].name, QP_TEXT_MAIN);
            if (dock_buttons[i].desc[0]) {
                int dl = strlen(dock_buttons[i].desc);
                fb_draw_string_transparent(cx - dl * 4, cy + circle_r + 28,
                                           dock_buttons[i].desc, QP_TEXT_MUTED);
            }
        }
        } // End of pass loop
    }
}

/* ══════════════════════════════════════════════════════════
 *  Desktop Icons (Floating on desktop)
 * ══════════════════════════════════════════════════════════ */

static void draw_desktop_icons(int w, int h) {
    (void)w;
    (void)h;
    // Desktop Icons space (if needed in future)
    
    char usb_label[12];
    if (fat_is_mounted(usb_label)) {
        int cx = 320;
        int cy = 120;
        if (!fb_intersects_clip(cx - 60, cy - 60, 120, 120)) return;
        
        bool hover = hit_test_circle(frame_mx, frame_my, cx, cy, 35);
        
        if (hover) {
            gfx_draw_circle_filled(cx, cy, 30, QP_ACTIVE_BG);
            gfx_draw_circle(cx, cy, 30, QP_CYAN);
        }
        
        // USB/Drive icon
        icon_draw_shape(cx, cy, 20, ICON_HUD_DATABASE, hover ? QP_CYAN : QP_TEXT_MAIN);
        
        int len = strlen(usb_label);
        fb_draw_string_transparent(cx - (len * 4), cy + 32, usb_label, hover ? QP_TEXT_MAIN : QP_TEXT_MUTED);
    }
}

/* ══════════════════════════════════════════════════════════
 *  Main Update Loop
 * ══════════════════════════════════════════════════════════ */

void desktop_update(void) {
    frame_left_clicked = mouse_left_clicked();
    frame_mx = mouse_get_x();
    frame_my = mouse_get_y();

    int w = fb_get_width();
    int h = fb_get_height();

    update_dock_animation(w, h);

    tick_count++;

    /* Refresh topbar clock and central HUD every ~30 ticks */
    if (tick_count % 30 == 0) {
        int clk_w = 72;
        int clk_x = (w - clk_w) / 2;
        dirty_rect_add(clk_x - 4, 16, clk_w + 8, 34);
        if (!wm_has_maximized_window()) {
            dirty_rect_add(w / 2 - 220, h / 2 - 120, 440, 160);
        }
    }
    
    // Handle keyboard for search bar BEFORE wm_update consumes it
    if (search_focused) {
        struct key_event ev;
        while (keyboard_poll_event(&ev)) {
            if (ev.is_release) continue;

            int len = strlen(search_text);
            int sel_min = search_sel_start;
            int sel_max = search_sel_end;
            if (sel_min > sel_max) { int t = sel_min; sel_min = sel_max; sel_max = t; }

            if (ev.ctrl && (ev.c == 'a' || ev.c == 'A' || ev.scancode == 0x1E)) {
                // Ctrl+A: Select All
                search_sel_start = 0;
                search_sel_end = len;
                search_cursor_pos = len;
                force_redraw = true;
            } else if (ev.c == 27) { // ESC: Close / Unfocus search
                search_focused = false;
                force_redraw = true;
            } else if (ev.c == '\b') { // Backspace
                if (sel_min != sel_max) {
                    memmove(&search_text[sel_min], &search_text[sel_max], len - sel_max + 1);
                    search_cursor_pos = sel_min;
                } else if (search_cursor_pos > 0) {
                    memmove(&search_text[search_cursor_pos - 1], &search_text[search_cursor_pos], len - search_cursor_pos + 1);
                    search_cursor_pos--;
                }
                search_sel_start = search_cursor_pos;
                search_sel_end = search_cursor_pos;
                force_redraw = true;
            } else if (ev.scancode == 0x53) { // Delete key
                if (sel_min != sel_max) {
                    memmove(&search_text[sel_min], &search_text[sel_max], len - sel_max + 1);
                    search_cursor_pos = sel_min;
                } else if (search_cursor_pos < len) {
                    memmove(&search_text[search_cursor_pos], &search_text[search_cursor_pos + 1], len - search_cursor_pos);
                }
                search_sel_start = search_cursor_pos;
                search_sel_end = search_cursor_pos;
                force_redraw = true;
            } else if (ev.scancode == 0x4B) { // Left arrow
                if (search_cursor_pos > 0) search_cursor_pos--;
                if (ev.shift) {
                    search_sel_end = search_cursor_pos;
                } else {
                    search_sel_start = search_cursor_pos;
                    search_sel_end = search_cursor_pos;
                }
                force_redraw = true;
            } else if (ev.scancode == 0x4D) { // Right arrow
                if (search_cursor_pos < len) search_cursor_pos++;
                if (ev.shift) {
                    search_sel_end = search_cursor_pos;
                } else {
                    search_sel_start = search_cursor_pos;
                    search_sel_end = search_cursor_pos;
                }
                force_redraw = true;
            } else if (ev.scancode == 0x47) { // Home key
                search_cursor_pos = 0;
                if (ev.shift) {
                    search_sel_end = 0;
                } else {
                    search_sel_start = 0;
                    search_sel_end = 0;
                }
                force_redraw = true;
            } else if (ev.scancode == 0x4F) { // End key
                search_cursor_pos = len;
                if (ev.shift) {
                    search_sel_end = len;
                } else {
                    search_sel_start = len;
                    search_sel_end = len;
                }
                force_redraw = true;
            } else if (ev.c >= 32 && ev.c <= 126) { // Printable chars
                int remove_len = sel_max - sel_min;
                if (len - remove_len + 1 < SEARCH_MAX_LEN) {
                    memmove(&search_text[sel_min + 1], &search_text[sel_max], len - sel_max + 1);
                    search_text[sel_min] = ev.c;
                    search_cursor_pos = sel_min + 1;
                    search_sel_start = search_cursor_pos;
                    search_sel_end = search_cursor_pos;
                    force_redraw = true;
                }
            }
        }
    }
    
    /* Window manager gets first priority — windows block clicks to
     * desktop elements behind them, which is standard OS behaviour.
     * If we just launched an app, suppress the click for a few frames
     * so the launch-click can't immediately close the new window. */
    bool click_to_pass = frame_left_clicked;
    if (launch_click_cooldown > 0) {
        launch_click_cooldown--;
        click_to_pass = false; // suppress click
    }
    bool wm_handled = wm_update(click_to_pass);
    if (wm_handled) {
        if (frame_left_clicked) {
            search_focused = false; // Unfocus search when clicking a window
            force_redraw = true;
            frame_left_clicked = false; // Consume the click so the desktop doesn't use it
        }
    }
    
    static int old_hover_dock_idx = -1;

    // Handle Search Bar dragging for text selection
    if (search_is_selecting) {
        if (mouse_left_pressed()) {
            if (frame_mx >= 70 && frame_mx < 300) {
                int char_idx = (frame_mx - 70 + 4) / 8; // +4 for rounding
                int len = strlen(search_text);
                if (char_idx < 0) char_idx = 0;
                if (char_idx > len) char_idx = len;
                
                if (search_sel_end != char_idx) {
                    search_sel_end = char_idx;
                    search_cursor_pos = char_idx;
                    force_redraw = true;
                }
            }
        } else {
            search_is_selecting = false;
        }
    }

    // Handle Dock dragging (Magnetic UI)
    static int drag_item_idx = -1;
    static int drag_start_x = 0;
    static int drag_start_y = 0;
    static bool drag_moved = false;

    if (!wm_handled && frame_left_clicked) {
        int dock_y = h - 100;
        for (int i = 0; i < NUM_DOCK; i++) {
            if (hit_test_circle(frame_mx, frame_my, dock_buttons[i].current_x, dock_y, 35)) {
                drag_item_idx = i;
                drag_start_x = frame_mx;
                drag_start_y = frame_my;
                drag_moved = false;
                break;
            }
        }
    }

    if (drag_item_idx != -1) {
        if (mouse_left_pressed()) {
            int dx = frame_mx - drag_start_x;
            int dy = frame_my - drag_start_y;
            if (!drag_moved && (dx*dx + dy*dy > 25)) { // 5 pixels threshold
                drag_moved = true;
                dock_buttons[drag_item_idx].is_dragging = true;
            }
            if (drag_moved) {
                dock_buttons[drag_item_idx].current_x = frame_mx;
                dock_buttons[drag_item_idx].current_y = frame_my;
                force_redraw = true;

                // Check for swaps
                for (int i = 0; i < NUM_DOCK; i++) {
                    if (i != drag_item_idx && !dock_buttons[i].is_dragging) {
                        int diff = frame_mx - (int)dock_buttons[i].target_x;
                        if (diff > -40 && diff < 40) { // Crossed over
                            // Swap them
                            struct hud_button temp = dock_buttons[i];
                            dock_buttons[i] = dock_buttons[drag_item_idx];
                            dock_buttons[drag_item_idx] = temp;
                            drag_item_idx = i; 
                            
                            // Recalculate targets immediately
                            int dock_spacing = 85;
                            int start_x = w / 2 - ((NUM_DOCK - 1) * dock_spacing) / 2;
                            for (int j = 0; j < NUM_DOCK; j++) {
                                dock_buttons[j].target_x = start_x + j * dock_spacing;
                            }
                            break;
                        }
                    }
                }
            }
        } else { // Mouse released
            if (drag_moved) {
                dock_buttons[drag_item_idx].is_dragging = false;
            } else { // It was just a click
                audio_play_click();
                if (dock_buttons[drag_item_idx].app_id != -1) {
                    desktop_launch_app(dock_buttons[drag_item_idx].app_id);
                }
                frame_left_clicked = false; // consume
            }
            drag_item_idx = -1;
            drag_moved = false;
            force_redraw = true;
        }
    }
    
    if (force_redraw || (search_focused && tick_count % 15 == 0)) { // Add cursor blink redraw
        dirty_rect_add(0, 0, w, h);
        force_redraw = false;
        old_mx = frame_mx;
        old_my = frame_my;
    } else {
        if (frame_mx != old_mx || frame_my != old_my) {
            int min_x = old_mx < frame_mx ? old_mx : frame_mx;
            int min_y = old_my < frame_my ? old_my : frame_my;
            int max_x = (old_mx + 28 > frame_mx + 28) ? old_mx + 28 : frame_mx + 28;
            int max_y = (old_my + 28 > frame_my + 28) ? old_my + 28 : frame_my + 28;
            dirty_rect_add(min_x, min_y, max_x - min_x, max_y - min_y);
            
            // Only check desktop hovers if the mouse is not over a window
            int hover_idx = -1;
            int left_hover_idx = -1;
            int right_hover_idx = -1;
            int vol_hover_idx = -1;
            bool usb_hover = false;

            int dock_y = h - 100;
            int spacing = 85;
            int total_w = (NUM_DOCK - 1) * spacing;
            int start_x = w / 2 - total_w / 2;
            int circle_r = 30;

            int dock_h_left = h - 85;
            if (dock_h_left < 500) dock_h_left = 500;
            int sx_left = 14;
            int sy_left = 95;
            int gap_left = (dock_h_left - 45) / NUM_LEFT;
            if (gap_left > 44) gap_left = 44;
            if (gap_left < 35) gap_left = 35;
            int item_w_left = 220 - 12;
            int item_h_left = gap_left - 3;

            int sx_right = w - 200;
            int sy_right = 100;
            int gap_right = 56;
            
            int rx_vol = w - 380;
            
            char usb_label[12];
            bool has_usb = fat_is_mounted(usb_label);
            int cx_usb = 320, cy_usb = 120;

            if (!wm_handled) {
                // Check dock hover
                for (int i = 0; i < NUM_DOCK; i++) {
                    int cx = start_x + i * spacing;
                    if (hit_test_circle(frame_mx, frame_my, cx, dock_y, circle_r + 5)) {
                        hover_idx = i;
                        break;
                    }
                }

                // Check Left Sidebar (macOS Vertical Dock) hover
                if (app_menu_open) {
                    for (int i = 0; i < NUM_LEFT; i++) {
                        if (hit_test_rect(frame_mx, frame_my, sx_left, sy_left + i * gap_left, item_w_left, item_h_left)) {
                            left_hover_idx = i;
                            break;
                        }
                    }
                }

                // Check Right Panel hover
                for (int i = 0; i < NUM_RIGHT; i++) {
                    if (hit_test_rect(frame_mx, frame_my, sx_right, sy_right + i * gap_right, 200, 48)) {
                        right_hover_idx = i;
                        break;
                    }
                }
                
                // Check Volume Hover
                if (hit_test_circle(frame_mx, frame_my, rx_vol + 15, 39, 15)) vol_hover_idx = 0;
                else if (hit_test_circle(frame_mx, frame_my, rx_vol + 65, 39, 15)) vol_hover_idx = 1;
                else if (hit_test_circle(frame_mx, frame_my, rx_vol + 105, 39, 15)) vol_hover_idx = 2;

                // Check USB desktop icon hover
                if (has_usb) {
                    usb_hover = hit_test_circle(frame_mx, frame_my, cx_usb, cy_usb, 35);
                }
            }  /* end if (!wm_handled) */

            if (hover_idx != old_hover_dock_idx) {
                if (old_hover_dock_idx != -1) {
                    int ocx = start_x + old_hover_dock_idx * spacing;
                    dirty_rect_add(ocx - 80, dock_y - 40, 160, 140);
                }
                if (hover_idx != -1) {
                    int ncx = start_x + hover_idx * spacing;
                    dirty_rect_add(ncx - 80, dock_y - 40, 160, 140);
                }
                old_hover_dock_idx = hover_idx;
            }

            static int old_left_hover_idx = -1;
            if (left_hover_idx != old_left_hover_idx) {
                dirty_rect_add(sx_left - 10, 60, 240, NUM_LEFT * gap_left + 50);
                old_left_hover_idx = left_hover_idx;
            }

            static int old_right_hover_idx = -1;
            if (right_hover_idx != old_right_hover_idx) {
                dirty_rect_add(sx_right - 60, 80, 270, NUM_RIGHT * gap_right + 30);
                old_right_hover_idx = right_hover_idx;
            }

            static int old_vol_hover_idx = -1;
            if (vol_hover_idx != old_vol_hover_idx) {
                dirty_rect_add(rx_vol, 20, 140, 40);
                old_vol_hover_idx = vol_hover_idx;
            }

            if (has_usb) {
                static bool old_usb_hover = false;
                if (usb_hover != old_usb_hover) {
                    dirty_rect_add(cx_usb - 60, cy_usb - 60, 120, 120);
                    old_usb_hover = usb_hover;
                }
            }

            old_mx = frame_mx;
            old_my = frame_my;
        }

        if (score_animating) {
            dirty_rect_add(w/2 - 200, h/2 - 120, 200, 120);
        }
    }  /* ── end of else (hover-only path) ── */

    bool is_max = wm_has_maximized_window();

    /* Track maximized state transitions to force full redraw */
    if (is_max != last_max_state) {
        last_max_state = is_max;
        force_redraw = true;
        dirty_rect_add(0, 0, w, h);
    }

    /* ── CLICK HANDLING: always runs, never skipped by force_redraw ── */
    if (frame_left_clicked) {
        dirty_rect_add(0, 0, w, 70);
        dirty_rect_add(0, 20, 260, 600);         /* Left sidebar */

        // 1. Applications Button Click (Toggle Launchpad Popup)
        bool clicked_app_btn = is_max ? hit_test_rect(frame_mx, frame_my, 10, 4, 125, 22)
                                      : hit_test_rect(frame_mx, frame_my, 20, 22, 135, 34);
        if (clicked_app_btn) {
            audio_play_click();
            app_menu_open = !app_menu_open;
            force_redraw = true;
        }

        // 2. Handle Applications Popup Menu interactions
        if (app_menu_open && !clicked_app_btn) {
            int sx_left = 14, sy_left = 95;
            int gap_left = (h - 85 - 45) / NUM_LEFT;
            if (gap_left > 44) gap_left = 44;
            if (gap_left < 35) gap_left = 35;
            int item_w_left = 220 - 12;
            int item_h_left = gap_left - 3;

            // Close button [x]
            if (hit_test_rect(frame_mx, frame_my, sx_left + 220 - 28, 67, 20, 20)) {
                audio_play_click();
                app_menu_open = false;
                force_redraw = true;
            } else {
                bool item_clicked = false;
                for (int i = 0; i < NUM_LEFT; i++) {
                    if (hit_test_rect(frame_mx, frame_my, sx_left, sy_left + i * gap_left, item_w_left, item_h_left)) {
                        audio_play_click();
                        if (left_buttons[i].app_id != -1) desktop_launch_app(left_buttons[i].app_id);
                        app_menu_open = false;
                        item_clicked = true;
                        force_redraw = true;
                        break;
                    }
                }
                // If clicked outside popup, close it!
                if (!item_clicked && !hit_test_rect(frame_mx, frame_my, 10, 60, 230, NUM_LEFT * gap_left + 50)) {
                    app_menu_open = false;
                    force_redraw = true;
                }
            }
        }

        /* Elements only clickable in Normal Mode (not when maximized) */
        if (!is_max) {
            dirty_rect_add(w - 260, 80, 260, 350);   /* Right panel */

            // 3. Handle Search Bar click
            bool clicked_search = hit_test_rect(frame_mx, frame_my, 165, 22, 210, 34);
            if (clicked_search) {
                search_focused = true;
                search_is_selecting = true;
                int char_idx = (frame_mx - 200 + 4) / 8;
                int len = strlen(search_text);
                if (char_idx < 0) char_idx = 0;
                if (char_idx > len) char_idx = len;
                search_sel_start = char_idx;
                search_sel_end = char_idx;
                search_cursor_pos = char_idx;
                force_redraw = true;
            } else if (search_focused) {
                bool clicked_dropdown = false;
                if (strlen(search_text) > 0) {
                    if (hit_test_rect(frame_mx, frame_my, 165, 60, 280, 20 * 40)) {
                        clicked_dropdown = true;
                        struct hud_button *matches[20];
                        int match_count = 0;
                        for (int i = 0; i < NUM_DOCK; i++) {
                            if (dock_buttons[i].app_id != -1 && (desktop_strcasestr(dock_buttons[i].name, search_text) || desktop_strcasestr(dock_buttons[i].desc, search_text))) {
                                matches[match_count++] = &dock_buttons[i];
                                if (match_count >= 20) break;
                            }
                        }
                        for (int i = 0; i < NUM_LEFT && match_count < 20; i++) {
                            if (left_buttons[i].app_id != -1 && (desktop_strcasestr(left_buttons[i].name, search_text) || desktop_strcasestr(left_buttons[i].desc, search_text))) {
                                bool dup = false;
                                for (int j = 0; j < match_count; j++) if (matches[j]->app_id == left_buttons[i].app_id) { dup = true; break; }
                                if (!dup) matches[match_count++] = &left_buttons[i];
                            }
                        }
                        int click_idx = (frame_my - 60) / 40;
                        if (click_idx >= 0 && click_idx < match_count) {
                            audio_play_click();
                            desktop_launch_app(matches[click_idx]->app_id);
                            search_focused = false;
                            search_text[0] = '\0';
                            force_redraw = true;
                        }
                    }
                }
                if (!clicked_dropdown) {
                    search_focused = false;
                    force_redraw = true;
                }
            }

            dirty_rect_add(0, h - 140, w, 140);

            char usb_label[12];
            if (fat_is_mounted(usb_label)) {
                dirty_rect_add(320 - 60, 120 - 60, 120, 120);
            }

            /* Right Panel clicks */
            {
                int sx = w - 240, sy = 100, gap = 56;
                for (int i = 0; i < NUM_RIGHT; i++) {
                    if (hit_test_rect(frame_mx, frame_my, sx, sy + i * gap, 200, 48)) {
                        audio_play_click();
                        if (right_buttons[i].app_id != -1) desktop_launch_app(right_buttons[i].app_id);
                        force_redraw = true;
                        break;
                    }
                }
            }

            /* Volume buttons */
            {
                int rx_vol = w - 380;
                if (hit_test_circle(frame_mx, frame_my, rx_vol + 15, 39, 15)) {
                    audio_set_volume(audio_get_volume() - 10);
                    dirty_rect_add(rx_vol, 20, 140, 40);
                    audio_play_click();
                } else if (hit_test_circle(frame_mx, frame_my, rx_vol + 65, 39, 15)) {
                    audio_set_volume(audio_get_volume() + 10);
                    dirty_rect_add(rx_vol, 20, 140, 40);
                    audio_play_click();
                } else if (hit_test_circle(frame_mx, frame_my, rx_vol + 105, 39, 15)) {
                    audio_toggle_mute();
                    dirty_rect_add(rx_vol, 20, 140, 40);
                    audio_play_click();
                }
            }

            /* USB drive icon */
            if (fat_is_mounted(usb_label)) {
                if (hit_test_circle(frame_mx, frame_my, 320, 120, 35)) {
                    audio_play_click();
                    extern struct window *file_manager_open_path(const char *path);
                    file_manager_open_path("/hd0");
                }
            }
        }
    } /* end if (frame_left_clicked) */

    int count = dirty_rect_get_count();
    if (count == 0) {
        frame_left_clicked = false;
        return;
    }

    int min_x = w, min_y = h, max_x = 0, max_y = 0;
    for (int i = 0; i < count; i++) {
        rect_t *r = dirty_rect_get(i);
        if (!r) continue;
        if (r->x < min_x) min_x = r->x;
        if (r->y < min_y) min_y = r->y;
        if (r->x + r->width > max_x) max_x = r->x + r->width;
        if (r->y + r->height > max_y) max_y = r->y + r->height;
    }
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > w) max_x = w;
    if (max_y > h) max_y = h;

    int bw = max_x - min_x;
    int bh = max_y - min_y;

    if (bw > 0 && bh > 0) {
        fb_set_clip(min_x, min_y, bw, bh);
        rect_t bound_rect = { .x = min_x, .y = min_y, .width = bw, .height = bh };
        
        layer_render_all(&bound_rect);
        
        // Hide background desktop widgets when a window is maximized
        if (!is_max) {
            draw_right_panel(w);
            draw_central_hud(w, h);
            draw_dock(w, h);
            draw_desktop_icons(w, h);
        }
        
        wm_render();
        
        draw_left_sidebar(); // Draw vertical Applications popup on top of windows!
        draw_header(w);      // Draw header and search overlay ON TOP of everything
        cursor_draw(frame_mx, frame_my);

        fb_reset_clip();
        fb_swap();
    }
    
    dirty_rect_clear();

    /* Consume the click at the end of the frame */
    frame_left_clicked = false;
}
