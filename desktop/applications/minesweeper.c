/*
 * Zarqaa OS - Minesweeper (click: reveal, right click or F: flag)
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/zb_theme.h"

#define N 9
#define MINES 10
#define CS 32
#define GX 14
#define GY 70

enum { ST_READY, ST_PLAY, ST_WON, ST_LOST };

struct mines {
    uint8_t mine[N][N], shown[N][N], flag[N][N];
    int state, flags;
    uint64_t t0, t_end;
    uint32_t rng;
    int hr, hc;                    /* hovered cell, for the F-key flag */
};

static uint32_t rnd(struct mines *m) {
    m->rng ^= m->rng << 13;
    m->rng ^= m->rng >> 17;
    m->rng ^= m->rng << 5;
    return m->rng;
}

static void reset(struct mines *m) {
    memset(m->mine, 0, sizeof(m->mine));
    memset(m->shown, 0, sizeof(m->shown));
    memset(m->flag, 0, sizeof(m->flag));
    m->state = ST_READY;
    m->flags = 0;
}

static int around(struct mines *m, int r, int c) {
    int n = 0;
    for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++) {
            int rr = r + dr, cc = c + dc;
            if ((dr || dc) && rr >= 0 && rr < N && cc >= 0 && cc < N) n += m->mine[rr][cc];
        }
    return n;
}

static void place(struct mines *m, int sr, int sc) {
    for (int placed = 0; placed < MINES;) {
        int r = (int)(rnd(m) % N), c = (int)(rnd(m) % N);
        if (m->mine[r][c] || (r >= sr - 1 && r <= sr + 1 && c >= sc - 1 && c <= sc + 1)) continue;
        m->mine[r][c] = 1;
        placed++;
    }
}

static void reveal(struct mines *m, int r, int c) {
    if (r < 0 || r >= N || c < 0 || c >= N || m->shown[r][c] || m->flag[r][c]) return;
    m->shown[r][c] = 1;
    if (m->mine[r][c]) return;
    if (around(m, r, c) == 0)
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) reveal(m, r + dr, c + dc);
}

static void check_win(struct mines *m) {
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            if (!m->mine[r][c] && !m->shown[r][c]) return;
    m->state = ST_WON;
    m->t_end = pit_get_ticks();
}

static void toggle_flag(struct mines *m, int r, int c) {
    if (r < 0 || r >= N || c < 0 || c >= N || m->shown[r][c]) return;
    m->flag[r][c] ^= 1;
    m->flags += m->flag[r][c] ? 1 : -1;
}

static const color_t num_col[9] = {0, 0xFF6CB4FF, 0xFF3DDC97, 0xFFFF5C7A, 0xFFB48CFF, 0xFFFFB347, 0xFF4FE3E3, 0xFFFFFFFF, 0xFFAAAAAA};

static void m_on_draw(struct window *win) {
    struct mines *m = (struct mines *)win->user_data;
    fb_fill_rect(0, 0, win->width, win->height, ZB_BG);
    char buf[16];
    snprintf(buf, sizeof(buf), "Mines %02d", MINES - m->flags);
    fb_draw_string_transparent(16, 24, buf, ZB_TEXT);
    uint64_t end = m->state == ST_PLAY ? pit_get_ticks() : m->t_end;
    int secs = m->state == ST_READY ? 0 : (int)((end - m->t0) / 1000);
    snprintf(buf, sizeof(buf), "Time %03d", secs);
    fb_draw_string_transparent(win->width - 90, 24, buf, ZB_TEXT);
    const char *face = m->state == ST_WON ? "Won!" : m->state == ST_LOST ? "Lost" : "New";
    zb_button(win->width / 2 - 30, 14, 60, 30, face, false, m->state == ST_LOST ? ZB_BAD : m->state == ST_WON ? ZB_GOOD : ZB_ACCENT);

    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            int x = GX + c * CS, y = GY + r * CS;
            bool open = m->shown[r][c];
            gfx_draw_rounded_rect_filled(x + 1, y + 1, CS - 2, CS - 2, 5, open ? ZB_PANEL : ZB_PANEL_HI);
            if (open && m->mine[r][c]) {
                gfx_draw_circle_filled(x + CS / 2, y + CS / 2, 7, ZB_BAD);
            } else if (open) {
                int n = around(m, r, c);
                if (n) { buf[0] = '0' + n; buf[1] = 0; fb_draw_string_transparent(x + CS / 2 - 4, y + CS / 2 - 8, buf, num_col[n]); }
            } else if (m->flag[r][c]) {
                fb_fill_rect(x + 12, y + 8, 3, 16, ZB_TEXT);
                fb_fill_rect(x + 15, y + 8, 9, 7, ZB_ACCENT_HI);
            } else if (m->state == ST_LOST && m->mine[r][c]) {
                gfx_draw_circle_filled(x + CS / 2, y + CS / 2, 5, ZB_BAD);
            }
        }
}

static void m_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    struct mines *m = (struct mines *)win->user_data;
    m->hr = (my - GY) / CS;
    m->hc = (mx - GX) / CS;
    if (mx < GX || my < GY) m->hr = m->hc = -1;
    if (!left && !right) return;
    if (left && zb_in(mx, my, win->width / 2 - 30, 14, 60, 30)) { reset(m); return; }
    if (m->state == ST_WON || m->state == ST_LOST) return;
    int c = (mx - GX) / CS, r = (my - GY) / CS;
    if (mx < GX || my < GY || r >= N || c >= N) return;
    if (right) {
        toggle_flag(m, r, c);
        return;
    }
    if (m->state == ST_READY) {
        m->rng = (uint32_t)pit_get_ticks() | 1;
        place(m, r, c);
        m->state = ST_PLAY;
        m->t0 = pit_get_ticks();
    }
    reveal(m, r, c);
    if (m->mine[r][c] && m->shown[r][c]) { m->state = ST_LOST; m->t_end = pit_get_ticks(); }
    else check_win(m);
}

static void m_on_key(struct window *win, char key, uint8_t sc) {
    (void)sc;
    struct mines *m = (struct mines *)win->user_data;
    if ((key == 'f' || key == 'F' || key == ' ') && (m->state == ST_READY || m->state == ST_PLAY))
        toggle_flag(m, m->hr, m->hc);
    else if (key == 'n' || key == 'N') reset(m);
}

static void m_on_tick(struct window *win) {
    struct mines *m = (struct mines *)win->user_data;
    static uint64_t last;
    uint64_t now = pit_get_ticks();
    if (m->state == ST_PLAY && now - last >= 500) { last = now; win->needs_redraw = true; }
}

static void m_on_close(struct window *win) {
    kfree(win->user_data);
    win->user_data = NULL;
}

struct window *minesweeper_open(void) {
    struct window *win = wm_create_window("Minesweeper", 320, 90, GX * 2 + N * CS, GY + N * CS + 14);
    if (!win) return NULL;
    struct mines *m = kcalloc(1, sizeof(*m));
    reset(m);
    win->user_data = m;
    win->on_draw = m_on_draw;
    win->on_mouse = m_on_mouse;
    win->on_tick = m_on_tick;
    win->on_key = m_on_key;
    win->on_close = m_on_close;
    return win;
}
