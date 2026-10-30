/* Zarqaa Blue theme palette and small drawing helpers shared by native apps */
#ifndef GUI_ZB_THEME_H
#define GUI_ZB_THEME_H

#include "zarqaa/types.h"
#include "zarqaa/string.h"
#include "drivers/framebuffer.h"
#include "gui/graphics.h"

#define ZB_BG        0xFF0A1B3D
#define ZB_PANEL     0xFF11295A
#define ZB_PANEL_HI  0xFF1B3D7E
#define ZB_ACCENT    0xFF2F80FF
#define ZB_ACCENT_HI 0xFF6CB4FF
#define ZB_TEXT      0xFFEAF2FF
#define ZB_DIM       0xFF8FA9D4
#define ZB_GOOD      0xFF3DDC97
#define ZB_BAD       0xFFFF5C7A

static inline void zb_text_center(int x, int y, int w, const char *s, color_t c) {
    int tw = (int)strlen(s) * 8;
    fb_draw_string_transparent(x + (w - tw) / 2, y, s, c);
}

static inline bool zb_in(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

static inline void zb_button(int x, int y, int w, int h, const char *label, bool hot, color_t base) {
    gfx_draw_rounded_rect_filled(x, y, w, h, 6, hot ? ZB_ACCENT_HI : base);
    zb_text_center(x, y + (h - 16) / 2, w, label, ZB_TEXT);
}

extern int64_t boot_time;
uint32_t pit_get_seconds(void);
uint64_t pit_get_ticks(void);

static inline uint64_t zb_now(void) { return (uint64_t)boot_time + pit_get_seconds(); }

/* Days since 1970-01-01 -> civil date (Howard Hinnant's algorithm) */
static inline void zb_civil(int64_t z, int *y, int *m, int *d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

static inline int zb_days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static inline int zb_days_in_month(int y, int m) {
    static const int dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return dm[m - 1] + (m == 2 && leap);
}

/* 0 = Sunday */
static inline int zb_weekday(int y, int m, int d) {
    int w = (zb_days_from_civil(y, m, d) + 4) % 7;
    return w < 0 ? w + 7 : w;
}

#endif
