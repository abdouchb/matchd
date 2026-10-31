/*
 * Zarqaa OS 0.1 - Dirty Rectangle Manager
 * Creator: Abdelhak Chabbi
 */

#ifndef GUI_DIRTY_RECT_H
#define GUI_DIRTY_RECT_H

#include "zarqaa/types.h"

#define MAX_DIRTY_RECTS 256

void dirty_rect_init(void);
void dirty_rect_add(int x, int y, int w, int h);
void dirty_rect_clear(void);
int dirty_rect_get_count(void);
rect_t *dirty_rect_get(int index);

/* Global clipping rectangle used by the framebuffer */
extern rect_t fb_clip_rect;
void fb_set_clip(int x, int y, int w, int h);
void fb_reset_clip(void);
bool fb_is_in_clip(int x, int y);
bool fb_intersects_clip(int x, int y, int w, int h);

#endif /* GUI_DIRTY_RECT_H */
