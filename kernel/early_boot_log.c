/*
 * Zarqaa OS 0.1 - Early Boot Screen Logger (Implementation)
 * Creator: Abdelhak Chabbi
 *
 * Draws boot progress directly to the raw Limine framebuffer.
 * Does NOT depend on heap, kmalloc, desktop, or any subsystem.
 * Only requires: fb_init() to have been called.
 */

#include "kernel/early_boot_log.h"
#include "drivers/framebuffer.h"
#include "drivers/font.h"
#include "drivers/pit.h"
#include "gui/dirty_rect.h"
#include "zarqaa/string.h"

/* ------------------------------------------------------------------ */
/* Colour palette                                                       */
/* ------------------------------------------------------------------ */
#define COL_BG       0xFF0A0E1A   /* Very dark navy                   */
#define COL_PANEL    0xFF111827   /* Slightly lighter panel           */
#define COL_BORDER   0xFF1E3050   /* Subtle blue border               */
#define COL_WHITE    0xFFEAEFF8   /* Soft white text                  */
#define COL_MUTED    0xFF607090   /* Muted grey                       */
#define COL_GREEN    0xFF00E676   /* Bright green  [  OK  ]           */
#define COL_YELLOW   0xFFFFD740   /* Amber         [ WARN ]           */
#define COL_RED      0xFFFF5252   /* Bright red    [ FAIL ]           */
#define COL_CYAN     0xFF40C4FF   /* Sky blue      step header        */
#define COL_LOGO_A   0xFF3D7FFF   /* Logo gradient A                  */
#define COL_LOGO_B   0xFF00E5FF   /* Logo gradient B                  */

/* ------------------------------------------------------------------ */
/* Layout constants (all in pixels)                                    */
/* ------------------------------------------------------------------ */
#define MARGIN_X       24
#define MARGIN_Y       24
#define LINE_H         18        /* pixels per log line               */
#define CHAR_W         8
#define CHAR_H         16
#define MAX_LINES      36        /* max visible log lines             */
#define BADGE_W        54        /* width of the "[  OK  ]" badge     */
#define LOGO_H         50        /* height of the top logo bar        */

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */
static int blog_y       = 0;    /* current Y for next line           */
static int blog_line    = 0;    /* line counter                      */
static int blog_last_y  = 0;    /* Y of last printed message (for badge) */
static int scr_w        = 0;
static int scr_h        = 0;
static bool blog_ready  = false;

/* ------------------------------------------------------------------ */
/* Low-level pixel write (bypasses dirty-rect - safe at early boot)    */
/* ------------------------------------------------------------------ */
static inline void _put_px(uint32_t *fb, int pitch_px, int x, int y, uint32_t col) {
    if (x < 0 || x >= scr_w || y < 0 || y >= scr_h) return;
    fb[y * pitch_px + x] = col;
}

static void _fill_rect(uint32_t *fb, int pitch_px,
                       int x, int y, int w, int h, uint32_t col) {
    for (int row = y; row < y + h; row++) {
        if (row < 0 || row >= scr_h) continue;
        for (int col_ = x; col_ < x + w; col_++) {
            if (col_ < 0 || col_ >= scr_w) continue;
            fb[row * pitch_px + col_] = col;
        }
    }
}

/* Draw a single character directly using the 8x16 bitmap font */
static void _draw_char(uint32_t *fb, int pitch_px,
                       int x, int y, char c, uint32_t fg) {
    const uint8_t *glyph = font_get_glyph(c);
    if (!glyph) return;
    for (int row = 0; row < CHAR_H; row++) {
        uint8_t bits = glyph[row];
        for (int col_ = 0; col_ < CHAR_W; col_++) {
            if (bits & (0x80 >> col_))
                _put_px(fb, pitch_px, x + col_, y + row, fg);
        }
    }
}

static void _draw_str(uint32_t *fb, int pitch_px,
                      int x, int y, const char *s, uint32_t fg) {
    while (*s) {
        _draw_char(fb, pitch_px, x, y, *s, fg);
        x += CHAR_W;
        s++;
    }
}

/* ------------------------------------------------------------------ */
/* Draw the top "ZarqaaOS Boot Logger" header bar                     */
/* ------------------------------------------------------------------ */
static void _draw_header(uint32_t *fb, int pitch_px) {
    /* Gradient background */
    for (int y = 0; y < LOGO_H; y++) {
        uint32_t r = 0x0A + y;
        uint32_t g = 0x0E + y / 2;
        uint32_t b = 0x1A + y;
        if (r > 0x25) r = 0x25;
        if (b > 0x3A) b = 0x3A;
        uint32_t row_col = 0xFF000000 | (r << 16) | (g << 8) | b;
        for (int x = 0; x < scr_w; x++)
            fb[y * pitch_px + x] = row_col;
    }

    /* Bottom border of header */
    for (int x = 0; x < scr_w; x++)
        fb[(LOGO_H - 1) * pitch_px + x] = COL_LOGO_B;

    /* Title text */
    const char *title  = "Zarqaa OS  -  Early Boot Logger";
    const char *sub    = "Tracking hardware initialization in real-time...";
    int tx = (scr_w - (int)strlen(title) * CHAR_W) / 2;
    int sx = (scr_w - (int)strlen(sub)   * CHAR_W) / 2;
    _draw_str(fb, pitch_px, tx,  9, title, COL_LOGO_B);
    _draw_str(fb, pitch_px, sx, 28, sub,   COL_MUTED);

    /* Left accent rectangle */
    _fill_rect(fb, pitch_px, 0, 0, 4, LOGO_H, COL_LOGO_A);
}

/* ------------------------------------------------------------------ */
/* Scroll the log area up by one line                                  */
/* ------------------------------------------------------------------ */
static void _scroll(uint32_t *fb, int pitch_px) {
    int log_top = LOGO_H + MARGIN_Y;
    int log_bot = scr_h - MARGIN_Y;
    int log_h   = log_bot - log_top;

    /* Move pixel rows up by LINE_H */
    for (int row = log_top; row < log_bot - LINE_H; row++) {
        uint32_t *src = &fb[( row + LINE_H) * pitch_px];
        uint32_t *dst = &fb[row * pitch_px];
        for (int col_ = 0; col_ < scr_w; col_++)
            dst[col_] = src[col_];
    }
    /* Clear the newly freed bottom line */
    int clear_y = log_bot - LINE_H;
    for (int row = clear_y; row < clear_y + LINE_H; row++) {
        for (int col_ = 0; col_ < scr_w; col_++)
            fb[row * pitch_px + col_] = COL_BG;
    }
    (void)log_h;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void early_blog_init(void) {
    scr_w = fb_get_width();
    scr_h = fb_get_height();
    if (!scr_w || !scr_h) return;

    int pitch_bytes = fb_get_pitch();
    int pitch_px    = pitch_bytes / 4;
    uint32_t *fb    = fb_get_backbuffer();

    /* Fill background */
    for (int y = 0; y < scr_h; y++)
        for (int x = 0; x < scr_w; x++)
            fb[y * pitch_px + x] = COL_BG;

    _draw_header(fb, pitch_px);

    blog_y     = LOGO_H + MARGIN_Y;
    blog_line  = 0;
    blog_ready = true;

    /* Flush to screen */
    dirty_rect_add(0, 0, scr_w, scr_h);
    fb_swap();
}

void early_blog(int level, const char *module, const char *msg) {
    if (!blog_ready) return;

    int pitch_bytes = fb_get_pitch();
    int pitch_px    = pitch_bytes / 4;
    uint32_t *fb    = fb_get_backbuffer();

    /* Auto-scroll if needed */
    int max_y = scr_h - MARGIN_Y - LINE_H;
    if (blog_y + LINE_H > max_y) {
        _scroll(fb, pitch_px);
        blog_y -= LINE_H;
    }

    blog_last_y = blog_y;

    /* Alternate row background for readability */
    uint32_t row_bg = (blog_line & 1) ? 0xFF0C1220 : COL_BG;
    _fill_rect(fb, pitch_px, MARGIN_X - 4, blog_y,
               scr_w - (MARGIN_X - 4) * 2, LINE_H, row_bg);

    /* Format Time */
    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t ms = (ticks % 100) * 10;
    
    char time_str[16];
    int t_idx = 0;
    time_str[t_idx++] = '[';
    time_str[t_idx++] = ' ';
    /* simple num to string for secs (assume < 1000s for boot) */
    if (secs > 99) time_str[t_idx++] = '0' + (secs / 100) % 10;
    if (secs > 9)  time_str[t_idx++] = '0' + (secs / 10) % 10;
    time_str[t_idx++] = '0' + (secs % 10);
    time_str[t_idx++] = '.';
    time_str[t_idx++] = '0' + (ms / 100) % 10;
    time_str[t_idx++] = '0' + (ms / 10) % 10;
    time_str[t_idx++] = '0' + (ms % 10);
    time_str[t_idx++] = ']';
    time_str[t_idx] = '\0';

    /* Level badge & color */
    uint32_t text_col = COL_WHITE;
    const char *badge  = "";
    uint32_t badge_col = COL_MUTED;

    switch (level) {
        case BLOG_OK:   text_col = COL_GREEN;  badge = "[ OK ]"; badge_col = COL_GREEN;  break;
        case BLOG_WARN: text_col = COL_YELLOW; badge = "[WARN]"; badge_col = COL_YELLOW; break;
        case BLOG_FAIL: text_col = COL_RED;    badge = "[FAIL]"; badge_col = COL_RED;    break;
        case BLOG_STEP: text_col = COL_CYAN;   badge = "[ >> ]"; badge_col = COL_CYAN;   break;
        default:        text_col = COL_WHITE;  badge = "[INFO]"; badge_col = COL_MUTED;  break;
    }

    /* Format Module: [ MODULE ] padding up to 8 chars */
    char mod_str[16] = "[        ]";
    if (module) {
        int m_len = strlen(module);
        if (m_len > 6) m_len = 6;
        int m_pad = (6 - m_len) / 2;
        for (int i = 0; i < m_len; i++) mod_str[2 + m_pad + i] = module[i];
    }

    /* Draw Time */
    int bx = MARGIN_X;
    _draw_str(fb, pitch_px, bx, blog_y + 1, time_str, COL_MUTED);
    bx += 11 * CHAR_W; /* "[ 0.000] " */

    /* Draw Level Badge */
    _draw_str(fb, pitch_px, bx, blog_y + 1, badge, badge_col);
    bx += 7 * CHAR_W; /* "[INFO] " */

    /* Draw Module Badge */
    _draw_str(fb, pitch_px, bx, blog_y + 1, mod_str, COL_MUTED);
    bx += 11 * CHAR_W; /* "[ MODULE ] " */

    /* Draw message */
    _draw_str(fb, pitch_px, bx, blog_y + 1, msg, text_col);

    blog_y    += LINE_H;
    blog_line++;

    /* Flush immediately so it's visible even if the next step hangs */
    dirty_rect_add(0, 0, scr_w, scr_h);
    fb_swap();
}

void early_blog_ok(void) {
    if (!blog_ready) return;
    int pitch_bytes = fb_get_pitch();
    int pitch_px    = pitch_bytes / 4;
    uint32_t *fb    = fb_get_backbuffer();

    /* Draw " OK" at the right edge of the last line */
    int rx = scr_w - MARGIN_X - BADGE_W;
    _fill_rect(fb, pitch_px, rx - 4, blog_last_y, BADGE_W + 4, LINE_H, COL_BG);
    _draw_str(fb, pitch_px, rx, blog_last_y + 1, "[  OK  ]", COL_GREEN);
    dirty_rect_add(0, 0, scr_w, scr_h);
    fb_swap();
}

void early_blog_fail(const char *reason) {
    if (!blog_ready) return;
    int pitch_bytes = fb_get_pitch();
    int pitch_px    = pitch_bytes / 4;
    uint32_t *fb    = fb_get_backbuffer();

    int rx = scr_w - MARGIN_X - BADGE_W;
    _fill_rect(fb, pitch_px, rx - 4, blog_last_y, BADGE_W + 4, LINE_H, COL_BG);
    _draw_str(fb, pitch_px, rx, blog_last_y + 1, "[ FAIL ]", COL_RED);
    dirty_rect_add(0, 0, scr_w, scr_h);
    fb_swap();

    if (reason) {
        early_blog(BLOG_FAIL, "SYSTEM", reason);
    }
}

void early_blog_sep(void) {
    if (!blog_ready) return;
    int pitch_bytes = fb_get_pitch();
    int pitch_px    = pitch_bytes / 4;
    uint32_t *fb    = fb_get_backbuffer();

    int sy = blog_y + LINE_H / 2 - 1;
    for (int x = MARGIN_X; x < scr_w - MARGIN_X; x++)
        fb[sy * pitch_px + x] = COL_BORDER;

    blog_y += LINE_H;
    dirty_rect_add(0, 0, scr_w, scr_h);
    fb_swap();
}

void early_blog_flush(void) {
    dirty_rect_add(0, 0, scr_w, scr_h);
    fb_swap();
}

void early_blog_finish(void) {
    blog_ready = false;
}
