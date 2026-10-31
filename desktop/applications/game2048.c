/*
 * Zarqaa OS - 2048 puzzle game (arrow keys / WASD)
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/zb_theme.h"

struct g2048 {
    int cell[4][4];
    int score, best;
    bool over;
    uint32_t rng;
};

static uint32_t rnd(struct g2048 *g) {
    g->rng ^= g->rng << 13;
    g->rng ^= g->rng >> 17;
    g->rng ^= g->rng << 5;
    return g->rng;
}

static void spawn(struct g2048 *g) {
    int free_n = 0;
    for (int i = 0; i < 16; i++) if (!g->cell[i / 4][i % 4]) free_n++;
    if (!free_n) return;
    int pick = (int)(rnd(g) % (uint32_t)free_n);
    for (int i = 0; i < 16; i++) {
        if (g->cell[i / 4][i % 4]) continue;
        if (pick-- == 0) { g->cell[i / 4][i % 4] = (rnd(g) % 10) ? 2 : 4; return; }
    }
}

static void reset(struct g2048 *g) {
    memset(g->cell, 0, sizeof(g->cell));
    g->score = 0;
    g->over = false;
    spawn(g);
    spawn(g);
}

/* Slide one line toward index 0; returns true if anything changed */
static bool slide(struct g2048 *g, int *line) {
    int out[4] = {0, 0, 0, 0}, n = 0;
    bool merged = false;
    for (int i = 0; i < 4; i++) {
        if (!line[i]) continue;
        if (n && out[n - 1] == line[i] && !merged) { out[n - 1] *= 2; g->score += out[n - 1]; merged = true; }
        else { out[n++] = line[i]; merged = false; }
    }
    bool changed = false;
    for (int i = 0; i < 4; i++) { if (line[i] != out[i]) changed = true; line[i] = out[i]; }
    return changed;
}

static bool can_move(struct g2048 *g) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            if (!g->cell[r][c]) return true;
            if (c < 3 && g->cell[r][c] == g->cell[r][c + 1]) return true;
            if (r < 3 && g->cell[r][c] == g->cell[r + 1][c]) return true;
        }
    return false;
}

static void move(struct g2048 *g, int dir) { /* 0 left 1 right 2 up 3 down */
    bool changed = false;
    for (int k = 0; k < 4; k++) {
        int line[4];
        for (int i = 0; i < 4; i++) {
            int j = (dir & 1) ? 3 - i : i;
            line[i] = (dir < 2) ? g->cell[k][j] : g->cell[j][k];
        }
        if (slide(g, line)) changed = true;
        for (int i = 0; i < 4; i++) {
            int j = (dir & 1) ? 3 - i : i;
            if (dir < 2) g->cell[k][j] = line[i]; else g->cell[j][k] = line[i];
        }
    }
    if (changed) spawn(g);
    if (g->score > g->best) g->best = g->score;
    if (!can_move(g)) g->over = true;
}

static color_t tile_color(int v) {
    switch (v) {
    case 0: return 0xFF1B3D7E;
    case 2: return 0xFF4A8DFF;
    case 4: return 0xFF3A7BEF;
    case 8: return 0xFF2F9BEA;
    case 16: return 0xFF22B0DC;
    case 32: return 0xFF1BC4C4;
    case 64: return 0xFF19D2A0;
    case 128: return 0xFF7FD957;
    case 256: return 0xFFD9D94A;
    case 512: return 0xFFF2B83C;
    case 1024: return 0xFFF2873C;
    default: return 0xFFFF5C7A;
    }
}

#define TS 76
#define GAP 8
#define BX 14
#define BY 80

static void g_on_draw(struct window *win) {
    struct g2048 *g = (struct g2048 *)win->user_data;
    fb_fill_rect(0, 0, win->width, win->height, ZB_BG);
    char buf[32];
    fb_draw_string_transparent(16, 16, "2048", ZB_ACCENT_HI);
    snprintf(buf, sizeof(buf), "Score %d", g->score);
    fb_draw_string_transparent(16, 40, buf, ZB_TEXT);
    snprintf(buf, sizeof(buf), "Best %d", g->best);
    fb_draw_string_transparent(150, 40, buf, ZB_DIM);
    zb_button(win->width - 90, 12, 76, 28, "New", false, ZB_ACCENT);

    gfx_draw_rounded_rect_filled(BX - 6, BY - 6, 4 * TS + 3 * GAP + 12, 4 * TS + 3 * GAP + 12, 10, ZB_PANEL);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            int v = g->cell[r][c], x = BX + c * (TS + GAP), y = BY + r * (TS + GAP);
            gfx_draw_rounded_rect_filled(x, y, TS, TS, 8, tile_color(v));
            if (v) { snprintf(buf, sizeof(buf), "%d", v); zb_text_center(x, y + TS / 2 - 8, TS, buf, 0xFFFFFFFF); }
        }
    if (g->over) {
        gfx_draw_rounded_rect_filled(win->width / 2 - 70, BY + 130, 140, 40, 10, ZB_BAD);
        zb_text_center(win->width / 2 - 70, BY + 142, 140, "Game over", 0xFFFFFFFF);
    }
    zb_text_center(0, win->height - 22, win->width, "Arrows / WASD to move", ZB_DIM);
}

static void g_on_key(struct window *win, char key, uint8_t sc) {
    struct g2048 *g = (struct g2048 *)win->user_data;
    int dir = -1;
    if (sc == 0x4B || key == 'a' || key == 'A') dir = 0;
    else if (sc == 0x4D || key == 'd' || key == 'D') dir = 1;
    else if (sc == 0x48 || key == 'w' || key == 'W') dir = 2;
    else if (sc == 0x50 || key == 's' || key == 'S') dir = 3;
    else if (key == 'n' || key == 'N') reset(g);
    if (dir >= 0 && !g->over) move(g, dir);
}

static void g_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    if (left && zb_in(mx, my, win->width - 90, 12, 76, 28)) reset((struct g2048 *)win->user_data);
}

static void g_on_close(struct window *win) {
    kfree(win->user_data);
    win->user_data = NULL;
}

struct window *game2048_open(void) {
    struct window *win = wm_create_window("2048", 340, 100, 360, 440);
    if (!win) return NULL;
    struct g2048 *g = kcalloc(1, sizeof(*g));
    g->rng = (uint32_t)pit_get_ticks() | 1;
    reset(g);
    win->user_data = g;
    win->on_draw = g_on_draw;
    win->on_key = g_on_key;
    win->on_mouse = g_on_mouse;
    win->on_close = g_on_close;
    return win;
}
