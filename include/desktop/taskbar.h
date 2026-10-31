/*
 * Zarqaa OS 0.1 - Taskbar Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DESKTOP_TASKBAR_H
#define DESKTOP_TASKBAR_H

#include "zarqaa/types.h"

#define TOPBAR_HEIGHT 30
#define DOCK_HEIGHT 50
#define DOCK_ICON_SIZE 32
#define DOCK_ICON_SPACING 60

void taskbar_init(void);
void taskbar_draw(void);
void taskbar_handle_mouse(int mx, int my, bool clicked);

#endif /* DESKTOP_TASKBAR_H */
