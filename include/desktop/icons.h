/*
 * Zarqaa OS 0.1 - Icons Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DESKTOP_ICONS_H
#define DESKTOP_ICONS_H

#include "zarqaa/types.h"

#define ICON_FOLDER      0
#define ICON_FILE        1
#define ICON_TERMINAL    2
#define ICON_TEXT_EDITOR 3
#define ICON_SETTINGS    4
#define ICON_COMPUTER    5
#define ICON_TRASH       6
#define ICON_APP         7
#define ICON_CALCULATOR  25
#define ICON_HUD_SYSTEM   8
#define ICON_HUD_UPDATE   9
#define ICON_HUD_SECURITY 10
#define ICON_HUD_MONITOR  11
#define ICON_HUD_POWER    12
#define ICON_HUD_APPS     13
#define ICON_HUD_CUBE         14
#define ICON_HUD_CHIP         15
#define ICON_HUD_DATABASE     16
#define ICON_HUD_WIFI         17
#define ICON_HUD_USER         18
#define ICON_HUD_OPTIMIZATION 19
#define ICON_HUD_CLEANING     20
#define ICON_HUD_PERSONALIZE  21
#define ICON_HUD_PRIORITY     22
#define ICON_HUD_SEARCH       23
#define ICON_HUD_BELL         24
#define ICON_PAINT            26

#define ICON_SIZE 32

void icon_draw(int x, int y, int icon_id);
void icon_draw_futuristic(int cx, int cy, int radius, int icon_id, bool active);
void icon_draw_bitmap(int x, int y, const uint32_t *bitmap, int w, int h);
void icon_draw_shape(int cx, int cy, int r, int icon_id, color_t color);

#endif /* DESKTOP_ICONS_H */
