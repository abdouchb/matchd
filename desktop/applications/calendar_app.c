/*
 * Zarqaa OS - Calendar app: month view with today highlight
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/zb_theme.h"

struct cal_state {
    int year, month;
    int today_y, today_m, today_d;
    int hover;
};

static const char *month_names[12] = {"January", "February", "March", "April", "May", "June", "July",
                                      "August", "September", "October", "November", "December"};
static const char *dow[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

#define CELL_W 48
#define CELL_H 40
#define GRID_X 14
#define GRID_Y 100

static void cal_step(struct cal_state *s, int delta) {
    s->month += delta;
    if (s->month < 1) { s->month = 12; s->year--; }
    if (s->month > 12) { s->month = 1; s->year++; }
}

static void cal_on_draw(struct window *win) {
    struct cal_state *s = (struct cal_state *)win->user_data;
    int w = win->width;
    fb_fill_rect(0, 0, w, win->height, ZB_BG);

    gfx_draw_rounded_rect_filled(10, 10, w - 20, 44, 10, ZB_PANEL);
    zb_button(16, 16, 32, 32, "<", s->hover == 1, ZB_PANEL_HI);
    zb_button(w - 48, 16, 32, 32, ">", s->hover == 2, ZB_PANEL_HI);
    char buf[40];
    snprintf(buf, sizeof(buf), "%s %d", month_names[s->month - 1], s->year);
    zb_text_center(0, 24, w, buf, ZB_TEXT);

    for (int i = 0; i < 7; i++)
        zb_text_center(GRID_X + i * CELL_W, 72, CELL_W, dow[i], i == 0 ? ZB_BAD : ZB_ACCENT_HI);

    int first = zb_weekday(s->year, s->month, 1);
    int days = zb_days_in_month(s->year, s->month);
    for (int d = 1; d <= days; d++) {
        int idx = first + d - 1;
        int cx = GRID_X + (idx % 7) * CELL_W, cy = GRID_Y + (idx / 7) * CELL_H;
        bool today = d == s->today_d && s->month == s->today_m && s->year == s->today_y;
        if (today) gfx_draw_rounded_rect_filled(cx + 4, cy + 3, CELL_W - 8, CELL_H - 6, 8, ZB_ACCENT);
        snprintf(buf, sizeof(buf), "%d", d);
        zb_text_center(cx, cy + 12, CELL_W, buf, today ? 0xFFFFFFFF : (idx % 7 == 0 ? ZB_BAD : ZB_TEXT));
    }
    zb_text_center(0, win->height - 24, w, "Arrows: change month   T: today", ZB_DIM);
}

static void cal_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    struct cal_state *s = (struct cal_state *)win->user_data;
    int h = zb_in(mx, my, 16, 16, 32, 32) ? 1 : zb_in(mx, my, win->width - 48, 16, 32, 32) ? 2 : 0;
    if (h != s->hover) win->needs_redraw = true;
    s->hover = h;
    if (left && s->hover) cal_step(s, s->hover == 1 ? -1 : 1);
}

static void cal_on_key(struct window *win, char key, uint8_t scancode) {
    struct cal_state *s = (struct cal_state *)win->user_data;
    if (scancode == 0x4B || scancode == 0x48) cal_step(s, -1);
    else if (scancode == 0x4D || scancode == 0x50) cal_step(s, 1);
    else if (key == 't' || key == 'T') { s->year = s->today_y; s->month = s->today_m; }
}

static void cal_on_close(struct window *win) {
    kfree(win->user_data);
    win->user_data = NULL;
}

struct window *calendar_open(void) {
    struct window *win = wm_create_window("Zarqaa Calendar", 300, 110, 350, 400);
    if (!win) return NULL;
    struct cal_state *s = kcalloc(1, sizeof(*s));
    zb_civil((int64_t)(zb_now() / 86400), &s->today_y, &s->today_m, &s->today_d);
    s->year = s->today_y;
    s->month = s->today_m;
    win->user_data = s;
    win->on_draw = cal_on_draw;
    win->on_mouse = cal_on_mouse;
    win->on_key = cal_on_key;
    win->on_close = cal_on_close;
    return win;
}
