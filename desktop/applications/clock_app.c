/*
 * Zarqaa OS - Clock app: analog/digital clock, stopwatch and countdown timer
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/zb_theme.h"

#define TAB_CLOCK 0
#define TAB_STOPWATCH 1
#define TAB_TIMER 2

struct clock_state {
    int tab;
    bool sw_running;
    uint64_t sw_start;      /* tick at last start */
    uint64_t sw_accum;      /* ms accumulated while stopped */
    bool tm_running;
    uint64_t tm_end;        /* tick when countdown ends */
    uint64_t tm_left;       /* ms left while stopped */
    bool tm_done;
    int hover;
};

static const int sin60[16] = {0, 105, 208, 309, 407, 500, 588, 669, 743, 809, 866, 914, 951, 978, 995, 1000};

static int isin(int step) {            /* step = 1/60 turn */
    step %= 60;
    if (step < 0) step += 60;
    if (step <= 15) return sin60[step];
    if (step <= 30) return sin60[30 - step];
    if (step <= 45) return -sin60[step - 30];
    return -sin60[60 - step];
}
static int icos(int step) { return isin(step + 15); }

static void hand(int cx, int cy, int step, int len, color_t c) {
    int x = cx + isin(step) * len / 1000;
    int y = cy - icos(step) * len / 1000;
    fb_draw_line(cx, cy, x, y, c);
    fb_draw_line(cx + 1, cy, x + 1, y, c);
}

static void fmt_ms(char *buf, uint64_t ms) {
    snprintf(buf, 24, "%02d:%02d.%02d", (int)(ms / 60000), (int)((ms / 1000) % 60), (int)((ms % 1000) / 10));
}

static uint64_t sw_ms(struct clock_state *s) {
    return s->sw_accum + (s->sw_running ? pit_get_ticks() - s->sw_start : 0);
}

static uint64_t tm_ms(struct clock_state *s) {
    if (!s->tm_running) return s->tm_left;
    uint64_t now = pit_get_ticks();
    return now >= s->tm_end ? 0 : s->tm_end - now;
}

static const char *tab_names[3] = {"Clock", "Stopwatch", "Timer"};

static void clock_on_draw(struct window *win) {
    struct clock_state *s = (struct clock_state *)win->user_data;
    int w = win->width;
    fb_fill_rect(0, 0, w, win->height, ZB_BG);

    for (int i = 0; i < 3; i++) {
        int tx = 12 + i * 112;
        gfx_draw_rounded_rect_filled(tx, 10, 104, 28, 8, s->tab == i ? ZB_ACCENT : (s->hover == 10 + i ? ZB_PANEL_HI : ZB_PANEL));
        zb_text_center(tx, 16, 104, tab_names[i], ZB_TEXT);
    }

    char buf[32];
    if (s->tab == TAB_CLOCK) {
        uint64_t now = zb_now();
        int hh = (int)((now / 3600) % 24), mm = (int)((now / 60) % 60), ss = (int)(now % 60);
        int cx = w / 2, cy = 150, r = 90;
        gfx_draw_circle_filled(cx, cy, r + 6, ZB_PANEL_HI);
        gfx_draw_circle_filled(cx, cy, r + 2, ZB_PANEL);
        for (int i = 0; i < 12; i++) {
            int st = i * 5;
            int x0 = cx + isin(st) * (r - 10) / 1000, y0 = cy - icos(st) * (r - 10) / 1000;
            int x1 = cx + isin(st) * (r - 1) / 1000, y1 = cy - icos(st) * (r - 1) / 1000;
            fb_draw_line(x0, y0, x1, y1, ZB_ACCENT_HI);
        }
        hand(cx, cy, (hh % 12) * 5 + mm / 12, r * 5 / 10, ZB_TEXT);
        hand(cx, cy, mm, r * 75 / 100, ZB_ACCENT_HI);
        hand(cx, cy, ss, r * 85 / 100, ZB_BAD);
        gfx_draw_circle_filled(cx, cy, 4, ZB_TEXT);

        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);
        zb_text_center(0, 262, w, buf, ZB_TEXT);
        int y, m, d;
        zb_civil((int64_t)(now / 86400), &y, &m, &d);
        static const char *wd[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        snprintf(buf, sizeof(buf), "%s, %04d-%02d-%02d", wd[zb_weekday(y, m, d)], y, m, d);
        zb_text_center(0, 290, w, buf, ZB_DIM);
    } else if (s->tab == TAB_STOPWATCH) {
        fmt_ms(buf, sw_ms(s));
        gfx_draw_rounded_rect_filled(30, 70, w - 60, 100, 14, ZB_PANEL);
        zb_text_center(30, 112, w - 60, buf, ZB_ACCENT_HI);
        zb_button(30, 200, 120, 40, s->sw_running ? "Stop" : "Start", s->hover == 1, s->sw_running ? ZB_BAD : ZB_ACCENT);
        zb_button(w - 150, 200, 120, 40, "Reset", s->hover == 2, ZB_PANEL_HI);
    } else {
        fmt_ms(buf, tm_ms(s));
        gfx_draw_rounded_rect_filled(30, 70, w - 60, 100, 14, ZB_PANEL);
        zb_text_center(30, 112, w - 60, buf, s->tm_done ? ZB_BAD : ZB_ACCENT_HI);
        if (s->tm_done) zb_text_center(30, 140, w - 60, "Time's up!", ZB_BAD);
        zb_button(30, 190, 70, 30, "+1m", s->hover == 3, ZB_PANEL_HI);
        zb_button(w / 2 - 35, 190, 70, 30, "+10s", s->hover == 4, ZB_PANEL_HI);
        zb_button(w - 100, 190, 70, 30, "Clear", s->hover == 5, ZB_PANEL_HI);
        zb_button(30, 240, w - 60, 40, s->tm_running ? "Pause" : "Start", s->hover == 6, s->tm_running ? ZB_BAD : ZB_ACCENT);
    }
}

static int hit(struct clock_state *s, int mx, int my, int w) {
    for (int i = 0; i < 3; i++)
        if (zb_in(mx, my, 12 + i * 112, 10, 104, 28)) return 10 + i;
    if (s->tab == TAB_STOPWATCH) {
        if (zb_in(mx, my, 30, 200, 120, 40)) return 1;
        if (zb_in(mx, my, w - 150, 200, 120, 40)) return 2;
    } else if (s->tab == TAB_TIMER) {
        if (zb_in(mx, my, 30, 190, 70, 30)) return 3;
        if (zb_in(mx, my, w / 2 - 35, 190, 70, 30)) return 4;
        if (zb_in(mx, my, w - 100, 190, 70, 30)) return 5;
        if (zb_in(mx, my, 30, 240, w - 60, 40)) return 6;
    }
    return 0;
}

static void clock_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    struct clock_state *s = (struct clock_state *)win->user_data;
    int h = hit(s, mx, my, win->width);
    if (h != s->hover) win->needs_redraw = true;
    s->hover = h;
    if (!left || !s->hover) return;
    uint64_t now = pit_get_ticks();
    switch (s->hover) {
    case 10: case 11: case 12: s->tab = s->hover - 10; break;
    case 1:
        if (s->sw_running) { s->sw_accum += now - s->sw_start; s->sw_running = false; }
        else { s->sw_start = now; s->sw_running = true; }
        break;
    case 2: s->sw_running = false; s->sw_accum = 0; break;
    case 3: case 4:
        s->tm_done = false;
        if (s->tm_running) s->tm_end += s->hover == 3 ? 60000 : 10000;
        else s->tm_left += s->hover == 3 ? 60000 : 10000;
        break;
    case 5: s->tm_running = false; s->tm_left = 0; s->tm_done = false; break;
    case 6:
        if (s->tm_running) { s->tm_left = tm_ms(s); s->tm_running = false; }
        else if (s->tm_left) { s->tm_end = now + s->tm_left; s->tm_running = true; s->tm_done = false; }
        break;
    }
}

static void clock_on_tick(struct window *win) {
    struct clock_state *s = (struct clock_state *)win->user_data;
    uint64_t now = pit_get_ticks();
    static uint64_t last;
    if (s->tm_running && tm_ms(s) == 0) { s->tm_running = false; s->tm_left = 0; s->tm_done = true; win->needs_redraw = true; }
    /* Redraw ~10 Hz while something is animating */
    if (now - last >= 100 && (s->tab == TAB_CLOCK || s->sw_running || s->tm_running)) {
        last = now;
        win->needs_redraw = true;
    }
}

static void clock_on_key(struct window *win, char key, uint8_t scancode) {
    (void)scancode;
    struct clock_state *s = (struct clock_state *)win->user_data;
    if (key == '1') s->tab = TAB_CLOCK;
    else if (key == '2') s->tab = TAB_STOPWATCH;
    else if (key == '3') s->tab = TAB_TIMER;
}

static void clock_on_close(struct window *win) {
    kfree(win->user_data);
    win->user_data = NULL;
}

struct window *clock_app_open(void) {
    struct window *win = wm_create_window("Zarqaa Clock", 260, 120, 360, 330);
    if (!win) return NULL;
    win->user_data = kcalloc(1, sizeof(struct clock_state));
    win->on_draw = clock_on_draw;
    win->on_mouse = clock_on_mouse;
    win->on_key = clock_on_key;
    win->on_tick = clock_on_tick;
    win->on_close = clock_on_close;
    return win;
}
