#include "desktop/taskbar.h"
#include "desktop/desktop.h"
#include "desktop/icons.h"
#include "drivers/framebuffer.h"
#include "gui/graphics.h"
#include "drivers/pit.h"
#include "drivers/network.h"

extern int64_t boot_time;

static int dock_x;
static int dock_y;
static int dock_w;

static int hover_index = -1;

void taskbar_init(void) {
    int screen_w = fb_get_width();
    int screen_h = fb_get_height();
    dock_w = (6 * DOCK_ICON_SPACING) + 20;
    dock_x = (screen_w - dock_w) / 2;
    dock_y = screen_h - DOCK_HEIGHT - 10;
}

void taskbar_draw(void) {
    int screen_w = fb_get_width();

    // Draw Top Bar
    fb_fill_rect(0, 0, screen_w, TOPBAR_HEIGHT, COLOR_TOPBAR);
    fb_draw_string_transparent(10, 7, "? Zarqaa OS", COLOR_ACCENT);

    // Calculate time
    uint64_t current_time = boot_time + pit_get_seconds();
    int seconds = current_time % 60;
    int minutes = (current_time / 60) % 60;
    int hours = (current_time / 3600) % 24;

    char time_str[16];
    time_str[0] = '0' + (hours / 10);
    time_str[1] = '0' + (hours % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (minutes / 10);
    time_str[4] = '0' + (minutes % 10);
    time_str[5] = ':';
    time_str[6] = '0' + (seconds / 10);
    time_str[7] = '0' + (seconds % 10);
    time_str[8] = '\0';

    fb_draw_string_transparent(screen_w - 120, 7, time_str, COLOR_TEXT_PRIMARY);

    // Draw Power Button in top bar
    gfx_draw_rounded_rect_filled(screen_w - 40, 4, 30, 22, 5, COLOR_ERROR);
    fb_draw_string_transparent(screen_w - 32, 7, "PWR", COLOR_WHITE);

    // Draw Dock
    gfx_draw_rounded_rect_filled(dock_x, dock_y, dock_w, DOCK_HEIGHT, 24, COLOR_DOCK);
    gfx_draw_rounded_rect(dock_x, dock_y, dock_w, DOCK_HEIGHT, 24, 0xFF132D4A);

    int icons[6] = {ICON_FOLDER, ICON_TERMINAL, ICON_TEXT_EDITOR, ICON_CALCULATOR, ICON_SETTINGS, ICON_HUD_MONITOR};

    for (int i = 0; i < 6; i++) {
        int ix = dock_x + 10 + (i * DOCK_ICON_SPACING) + (DOCK_ICON_SPACING - DOCK_ICON_SIZE) / 2;
        int iy = dock_y + (DOCK_HEIGHT - DOCK_ICON_SIZE) / 2;

        if (i == hover_index) {
            gfx_draw_rounded_rect_filled(ix - 5, iy - 5, DOCK_ICON_SIZE + 10, DOCK_ICON_SIZE + 10, 12, COLOR_BUTTON_HOVER);
        }

        icon_draw(ix, iy, icons[i]);
    }
}

void taskbar_handle_mouse(int mx, int my, bool clicked) {
    hover_index = -1;
    
    // Check Top Bar Power Button
    int screen_w = fb_get_width();
    if (clicked && mx >= screen_w - 40 && mx <= screen_w - 10 && my >= 4 && my <= 26) {
        desktop_launch_app(APP_POWER);
        return;
    }

    if (mx >= dock_x && mx < dock_x + dock_w && my >= dock_y && my < dock_y + DOCK_HEIGHT) {
        for (int i = 0; i < 6; i++) {
            int ix = dock_x + 10 + (i * DOCK_ICON_SPACING) + (DOCK_ICON_SPACING - DOCK_ICON_SIZE) / 2;
            int iy = dock_y + (DOCK_HEIGHT - DOCK_ICON_SIZE) / 2;

            if (mx >= ix && mx <= ix + DOCK_ICON_SIZE && my >= iy && my <= iy + DOCK_ICON_SIZE) {
                hover_index = i;
                if (clicked) {
                    switch (i) {
                        case 0: desktop_launch_app(APP_FILE_MANAGER); break;
                        case 1: desktop_launch_app(APP_TERMINAL); break;
                        case 2: desktop_launch_app(APP_TEXT_EDITOR); break;
                        case 3: desktop_launch_app(APP_CALCULATOR); break;
                        case 4: desktop_launch_app(APP_SETTINGS); break;
                        case 5: desktop_launch_app(APP_TASK_MANAGER); break;
                    }
                }
                return;
            }
        }
    }
}
