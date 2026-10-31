/*
 * Zarqaa OS 0.1 - Mouse Cursor Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DESKTOP_CURSOR_H
#define DESKTOP_CURSOR_H

#include "zarqaa/types.h"

#define CURSOR_WIDTH 12
#define CURSOR_HEIGHT 19

void cursor_init(void);
void cursor_draw(int x, int y);
void cursor_save_background(int x, int y);
void cursor_restore_background(void);

#endif /* DESKTOP_CURSOR_H */
