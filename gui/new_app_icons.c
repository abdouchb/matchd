/* Procedurally generated icons for the Zarqaa Blue native apps */
#include "gui/new_app_icons.h"
#include "gui/zb_theme.h"

uint32_t new_icon_dock[NEWICON_COUNT][1600];
uint32_t new_icon_menu[NEWICON_COUNT][576];

static void px(uint32_t *b, int s, int x, int y, uint32_t c) {
    if (x >= 0 && y >= 0 && x < s && y < s) b[y * s + x] = c;
}

static void rect(uint32_t *b, int s, int x, int y, int w, int h, uint32_t c) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) px(b, s, x + i, y + j, c);
}

static void ring(uint32_t *b, int s, int cx, int cy, int r, int t, uint32_t c) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            int d = x * x + y * y;
            if (d <= r * r && d >= (r - t) * (r - t)) px(b, s, cx + x, cy + y, c);
        }
}

static void line(uint32_t *b, int s, int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        px(b, s, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Rounded blue gradient tile */
static void tile(uint32_t *b, int s) {
    int r = s / 5;
    for (int y = 0; y < s; y++) {
        int rr = 0x1E + (0x10 - 0x1E) * y / s, gg = 0x6A + (0x3A - 0x6A) * y / s, bb = 0xE8 + (0x9C - 0xE8) * y / s;
        for (int x = 0; x < s; x++) {
            int cx = x < r ? r - x : (x >= s - r ? x - (s - r - 1) : 0);
            int cy = y < r ? r - y : (y >= s - r ? y - (s - r - 1) : 0);
            if (cx * cx + cy * cy > r * r) { b[y * s + x] = 0; continue; }
            b[y * s + x] = 0xFF000000 | (rr << 16) | (gg << 8) | bb;
        }
    }
}

static void glyph(uint32_t *b, int s, int kind) {
    int u = s / 8;               /* unit */
    uint32_t W = 0xFFFFFFFF, L = 0xFFBFE0FF;
    int c = s / 2;
    switch (kind) {
    case NEWICON_CLOCK:
        ring(b, s, c, c, 3 * u, u > 3 ? 3 : 2, W);
        line(b, s, c, c, c, c - 2 * u, W);
        line(b, s, c, c, c + u + u / 2, c, L);
        break;
    case NEWICON_CALENDAR:
        rect(b, s, u * 2, u * 2, u * 4, u * 4, W);
        rect(b, s, u * 2, u * 2, u * 4, u, 0xFFFF6B8A);
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 3; i++)
                rect(b, s, u * 2 + u / 2 + i * (u + u / 4), u * 4 + j * (u + u / 4), u / 2 + 1, u / 2 + 1, 0xFF2F80FF);
        break;
    case NEWICON_SYSINFO:
        ring(b, s, c, c, 3 * u, u > 3 ? 3 : 2, W);
        rect(b, s, c - u / 4, c - u, u / 2 + 1, 2 * u, W);
        rect(b, s, c - u / 4, c - 2 * u + u / 2, u / 2 + 1, u / 2 + 1, W);
        break;
    case NEWICON_2048:
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 2; i++)
                rect(b, s, u * 2 + i * (u * 2 + 1), u * 2 + j * (u * 2 + 1), u * 2, u * 2, (i + j) & 1 ? L : W);
        break;
    case NEWICON_MINES:
        ring(b, s, c, c, 2 * u, 2 * u, W);
        line(b, s, c - 3 * u, c, c + 3 * u, c, W);
        line(b, s, c, c - 3 * u, c, c + 3 * u, W);
        line(b, s, c - 2 * u, c - 2 * u, c + 2 * u, c + 2 * u, W);
        line(b, s, c - 2 * u, c + 2 * u, c + 2 * u, c - 2 * u, W);
        break;
    }
}

void new_app_icons_init(void) {
    for (int k = 0; k < NEWICON_COUNT; k++) {
        tile(new_icon_dock[k], 40);
        glyph(new_icon_dock[k], 40, k);
        tile(new_icon_menu[k], 24);
        glyph(new_icon_menu[k], 24, k);
    }
}
