/*
 * Zarqaa OS 0.1 - Dirty Rectangle Manager
 * Creator: Abdelhak Chabbi
 */

#include "gui/dirty_rect.h"
#include "drivers/framebuffer.h"

static rect_t dirty_rects[MAX_DIRTY_RECTS];
static int dirty_rect_count = 0;

rect_t fb_clip_rect;

void dirty_rect_init(void) {
    dirty_rect_count = 0;
    fb_reset_clip();
}

void dirty_rect_add(int x, int y, int w, int h) {
    /* Clip to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb_get_width()) w = fb_get_width() - x;
    if (y + h > fb_get_height()) h = fb_get_height() - y;
    
    if (w <= 0 || h <= 0) return;

    /* If we exceed capacity, just invalidate the whole screen once */
    if (dirty_rect_count >= MAX_DIRTY_RECTS) {
        dirty_rect_count = 1;
        dirty_rects[0].x = 0;
        dirty_rects[0].y = 0;
        dirty_rects[0].width = fb_get_width();
        dirty_rects[0].height = fb_get_height();
        return;
    }

    /* Basic optimization: if this rect is already inside the first (whole screen) rect, ignore */
    if (dirty_rect_count == 1 && 
        dirty_rects[0].x == 0 && dirty_rects[0].y == 0 &&
        dirty_rects[0].width == fb_get_width() && dirty_rects[0].height == fb_get_height()) {
        return;
    }

    /* Check if we can merge with an existing dirty rectangle */
    for (int i = 0; i < dirty_rect_count; i++) {
        rect_t *r = &dirty_rects[i];
        if (!(x + w + 16 < r->x || x > r->x + r->width + 16 ||
              y + h + 16 < r->y || y > r->y + r->height + 16)) {
            int nx1 = x < r->x ? x : r->x;
            int ny1 = y < r->y ? y : r->y;
            int nx2 = (x + w > r->x + r->width) ? (x + w) : (r->x + r->width);
            int ny2 = (y + h > r->y + r->height) ? (y + h) : (r->y + r->height);
            r->x = nx1;
            r->y = ny1;
            r->width = nx2 - nx1;
            r->height = ny2 - ny1;
            return;
        }
    }

    if (dirty_rect_count >= MAX_DIRTY_RECTS) {
        dirty_rect_count = 1;
        dirty_rects[0].x = 0;
        dirty_rects[0].y = 0;
        dirty_rects[0].width = fb_get_width();
        dirty_rects[0].height = fb_get_height();
        return;
    }

    dirty_rects[dirty_rect_count].x = x;
    dirty_rects[dirty_rect_count].y = y;
    dirty_rects[dirty_rect_count].width = w;
    dirty_rects[dirty_rect_count].height = h;
    dirty_rect_count++;
}

void dirty_rect_clear(void) {
    dirty_rect_count = 0;
}

int dirty_rect_get_count(void) {
    return dirty_rect_count;
}

rect_t *dirty_rect_get(int index) {
    if (index < 0 || index >= dirty_rect_count) return NULL;
    return &dirty_rects[index];
}

void fb_set_clip(int x, int y, int w, int h) {
    fb_clip_rect.x = x;
    fb_clip_rect.y = y;
    fb_clip_rect.width = w;
    fb_clip_rect.height = h;
}

void fb_reset_clip(void) {
    fb_clip_rect.x = 0;
    fb_clip_rect.y = 0;
    fb_clip_rect.width = fb_get_width();
    fb_clip_rect.height = fb_get_height();
}

bool fb_is_in_clip(int x, int y) {
    return (x >= fb_clip_rect.x && x < fb_clip_rect.x + fb_clip_rect.width &&
            y >= fb_clip_rect.y && y < fb_clip_rect.y + fb_clip_rect.height);
}

bool fb_intersects_clip(int x, int y, int w, int h) {
    if (fb_clip_rect.width == 0 || fb_clip_rect.height == 0) return false;
    if (x + w <= fb_clip_rect.x) return false;
    if (x >= fb_clip_rect.x + fb_clip_rect.width) return false;
    if (y + h <= fb_clip_rect.y) return false;
    if (y >= fb_clip_rect.y + fb_clip_rect.height) return false;
    return true;
}
