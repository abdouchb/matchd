/*
 * Zarqaa OS 0.1 - Desktop Environment Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DESKTOP_H
#define DESKTOP_H

#include "zarqaa/types.h"

void desktop_init(void);
void desktop_update(void);
void desktop_launch_app(int app_id);
bool desktop_set_wallpaper_pixels(const uint32_t *pixels, int w, int h);
bool desktop_set_wallpaper_file(const char *vfs_path);
const char *desktop_get_current_wallpaper(void);

#define APP_FILE_MANAGER 0
#define APP_TERMINAL     1
#define APP_TEXT_EDITOR  2
#define APP_SETTINGS     3
#define APP_TASK_MANAGER 4
#define APP_POWER        5
#define APP_WIFI         6
#define APP_CALCULATOR   7
#define APP_HTML_VIEWER  8
#define APP_QUANTUM_VIEWER 9
#define APP_PYTHON_IDE   10
#define APP_PAINT        116
#define APP_DEBUG_CONSOLE 12
#define APP_BOOT_LOGGER  14
#define APP_DEVICE_MANAGER 15
#define APP_IMAGE_VIEWER 16
#define APP_BENCHMARK    17
#define APP_SNAKE        18
#define APP_CLOCK        19
#define APP_CALENDAR     20
#define APP_SYSINFO      21
#define APP_GAME2048     22
#define APP_MINESWEEPER  23

#endif /* DESKTOP_H */
