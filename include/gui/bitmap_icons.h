/* Auto-generated Zarqaa OS Icon Bitmaps */
#ifndef GUI_BITMAP_ICONS_H
#define GUI_BITMAP_ICONS_H

#include "zarqaa/types.h"

#define ICON_DOCK_SIZE 40
#define ICON_MENU_SIZE 24

extern const uint32_t icon_dock_browser[1600];
extern const uint32_t icon_menu_browser[576];
extern const uint32_t icon_dock_terminal[1600];
extern const uint32_t icon_menu_terminal[576];
extern const uint32_t icon_dock_files[1600];
extern const uint32_t icon_menu_files[576];
extern const uint32_t icon_dock_notepad[1600];
extern const uint32_t icon_menu_notepad[576];
extern const uint32_t icon_dock_studio[1600];
extern const uint32_t icon_menu_studio[576];
extern const uint32_t icon_dock_calculator[1600];
extern const uint32_t icon_menu_calculator[576];
extern const uint32_t icon_dock_python_ide[1600];
extern const uint32_t icon_menu_python_ide[576];
extern const uint32_t icon_dock_task_mgr[1600];
extern const uint32_t icon_menu_task_mgr[576];
extern const uint32_t icon_dock_gallery[1600];
extern const uint32_t icon_menu_gallery[576];
extern const uint32_t icon_dock_settings[1600];
extern const uint32_t icon_menu_settings[576];
extern const uint32_t icon_dock_boot_log[1600];
extern const uint32_t icon_menu_boot_log[576];
extern const uint32_t icon_dock_devices[1600];
extern const uint32_t icon_menu_devices[576];
extern const uint32_t icon_dock_benchmark[1600];
extern const uint32_t icon_menu_benchmark[576];
extern const uint32_t icon_dock_snake[1600];
extern const uint32_t icon_menu_snake[576];
extern const uint32_t icon_dock_power[1600];
extern const uint32_t icon_menu_power[576];

void gfx_draw_bitmap_rgba(int x, int y, const uint32_t *data, int w, int h);
void gfx_draw_bitmap_rgba_centered(int cx, int cy, const uint32_t *data, int w, int h);
const uint32_t *icon_get_dock_bitmap(int app_id);
const uint32_t *icon_get_menu_bitmap(int app_id);

#endif /* GUI_BITMAP_ICONS_H */
