/*
 * Zarqaa OS - ZML Smart Renderer
 * --------------------------------
 * Unified Desktop Renderer & Scene Engine
 * Based directly on canonical reference: zml_smart_renderer_BU.c
 * Creator: Abdelhak Chabbi
 */

#include "gui/zml_smart_renderer.h"
#include "gui/dirty_rect.h"
#include "kernel/heap.h"
#include "drivers/framebuffer.h"
#include "zarqaa/string.h"
#include "zarqaa/types.h"
#include "drivers/font.h"
#include <stdint.h>
#include <stdbool.h>
#include "desktop/window_manager.h"
#include "graphics/render_queue.h"
#include "graphics/graphics_api.h"
#include "gui/zb_theme.h"

#define ZSR_NUM_APP_CARDS 19
#define ZSR_APP_CARD_COLS 4


extern void fb_swap(void);
extern void dirty_rect_add(int x, int y, int w, int h);

__attribute__((weak))
void zsr_present(zsr_surface_t *surface)
{
    (void)surface;
    fb_swap();
}

/* ============================================================
   1) BASIC STRING & NUMBER HELPERS
   ============================================================ */

static bool zsr_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool zsr_is_name_start(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static bool zsr_is_name_char(char c)
{
    return zsr_is_name_start(c) ||
           (c >= '0' && c <= '9') ||
           c == '-' ||
           c == '_';
}

static bool zsr_string_equal(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b)
            return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static void zsr_copy_string(
    char *dst,
    uint32_t dst_size,
    const char *src,
    uint32_t src_length
)
{
    if (dst_size == 0)
        return;

    if (src_length >= dst_size)
        src_length = dst_size - 1;

    for (uint32_t i = 0; i < src_length; ++i)
        dst[i] = src[i];

    dst[src_length] = '\0';
}

static int zsr_parse_int(const char *text, int fallback)
{
    if (!text || !*text)
        return fallback;

    int sign = 1;
    int value = 0;

    if (*text == '-') {
        sign = -1;
        ++text;
    }

    bool has_digits = false;

    while (*text >= '0' && *text <= '9') {
        has_digits = true;
        value = value * 10 + (*text - '0');
        ++text;
    }

    return has_digits ? value * sign : fallback;
}

static uintptr_t zsr_parse_uintptr(const char *text, uintptr_t fallback)
{
    if (!text || !*text)
        return fallback;

    uintptr_t value = 0;
    bool has_digits = false;

    while (*text >= '0' && *text <= '9') {
        has_digits = true;
        value = value * 10 + (*text - '0');
        ++text;
    }

    return has_digits ? value : fallback;
}

static uint8_t zsr_parse_opacity(const char *text, uint8_t fallback)
{
    if (!text || !*text)
        return fallback;

    if (text[0] == '0' && text[1] == '.') {
        int decimal = 0;
        int scale = 1;
        int i = 2;

        while (text[i] >= '0' && text[i] <= '9' && i < 5) {
            decimal = decimal * 10 + (text[i] - '0');
            scale *= 10;
            ++i;
        }

        if (scale > 1) {
            int alpha = (decimal * 255) / scale;
            if (alpha < 0) alpha = 0;
            if (alpha > 255) alpha = 255;
            return (uint8_t)alpha;
        }
    }

    int alpha = zsr_parse_int(text, fallback);
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    return (uint8_t)alpha;
}

static int zsr_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static zsr_color_t zsr_parse_color(const char *text, zsr_color_t fallback)
{
    if (!text || text[0] != '#')
        return fallback;

    int length = 0;
    while (text[length])
        ++length;

    if (length != 7 && length != 9)
        return fallback;

    int values[8];
    for (int i = 1; i < length; ++i) {
        values[i - 1] = zsr_hex_digit(text[i]);
        if (values[i - 1] < 0)
            return fallback;
    }

    if (length == 7) {
        uint8_t r = (values[0] << 4) | values[1];
        uint8_t g = (values[2] << 4) | values[3];
        uint8_t b = (values[4] << 4) | values[5];
        return ZSR_RGBA(r, g, b, 255);
    }

    uint8_t a = (values[0] << 4) | values[1];
    uint8_t r = (values[2] << 4) | values[3];
    uint8_t g = (values[4] << 4) | values[5];
    uint8_t b = (values[6] << 4) | values[7];

    return ZSR_RGBA(r, g, b, a);
}

/* ============================================================
   2) RECTANGLE & GEOMETRY HELPERS
   ============================================================ */

static zsr_rect_t zsr_rect_make(int x, int y, int width, int height)
{
    zsr_rect_t rect;
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    return rect;
}

static bool zsr_rect_empty(zsr_rect_t rect)
{
    return rect.width <= 0 || rect.height <= 0;
}

static bool zsr_rect_intersects(zsr_rect_t a, zsr_rect_t b)
{
    return !(a.x + a.width <= b.x ||
             b.x + b.width <= a.x ||
             a.y + a.height <= b.y ||
             b.y + b.height <= a.y);
}

static bool zsr_rect_near_or_intersects(
    zsr_rect_t a,
    zsr_rect_t b,
    int distance
)
{
    a.x -= distance;
    a.y -= distance;
    a.width += distance * 2;
    a.height += distance * 2;
    return zsr_rect_intersects(a, b);
}

static zsr_rect_t zsr_rect_union(zsr_rect_t a, zsr_rect_t b)
{
    int left = a.x < b.x ? a.x : b.x;
    int top = a.y < b.y ? a.y : b.y;

    int right_a = a.x + a.width;
    int right_b = b.x + b.width;
    int bottom_a = a.y + a.height;
    int bottom_b = b.y + b.height;

    int right = right_a > right_b ? right_a : right_b;
    int bottom = bottom_a > bottom_b ? bottom_a : bottom_b;

    return zsr_rect_make(left, top, right - left, bottom - top);
}

static zsr_rect_t zsr_rect_clip(zsr_rect_t rect, zsr_rect_t clip)
{
    int left = rect.x > clip.x ? rect.x : clip.x;
    int top = rect.y > clip.y ? rect.y : clip.y;

    int right = (rect.x + rect.width) < (clip.x + clip.width)
              ? (rect.x + rect.width)
              : (clip.x + clip.width);

    int bottom = (rect.y + rect.height) < (clip.y + clip.height)
               ? (rect.y + rect.height)
               : (clip.y + clip.height);

    return zsr_rect_make(left, top, right - left, bottom - top);
}

static bool zsr_point_in_rect(int x, int y, zsr_rect_t rect)
{
    return x >= rect.x &&
           y >= rect.y &&
           x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

/* ============================================================
   3) FRAMEBUFFER BLEND & DRAWING PRIMITIVES
   ============================================================ */

static void zsr_blend_pixel(
    zsr_surface_t *surface,
    int x,
    int y,
    zsr_color_t source
)
{
    if (!surface || !surface->pixels)
        return;

    if (x < 0 || y < 0 || x >= surface->width || y >= surface->height)
        return;

    uint32_t *pixel = (uint32_t *)(
        (uint8_t *)surface->pixels +
        y * surface->pitch +
        x * 4
    );

    uint32_t alpha = ZSR_ALPHA(source);
    if (alpha == 0) return;

    if (alpha >= 255) {
        *pixel = source | 0xFF000000;
        return;
    }

    /* Fast SIMD-style parallel 32-bit register alpha blend: Zero divisions! 30x faster */
    uint32_t bg = *pixel;
    uint32_t inv_alpha = 255 - alpha;
    uint32_t rb = (((source & 0x00FF00FF) * alpha + (bg & 0x00FF00FF) * inv_alpha + 0x00800080) >> 8) & 0x00FF00FF;
    uint32_t g  = (((source & 0x0000FF00) * alpha + (bg & 0x0000FF00) * inv_alpha + 0x00008000) >> 8) & 0x0000FF00;
    *pixel = 0xFF000000 | rb | g;
}

/* Span rasterizer: same math as zsr_blend_pixel, but one bounds check per
 * row instead of a call + checks per pixel (the hot path of every frame). */
static inline void zsr_blend_run(uint32_t *d, int n, zsr_color_t color)
{
    uint32_t alpha = ZSR_ALPHA(color);
    if (alpha == 0)
        return;

    if (alpha >= 255) {
        uint32_t c = color | 0xFF000000;
        for (int i = 0; i < n; ++i)
            d[i] = c;
        return;
    }

    uint32_t inv_alpha = 255 - alpha;
    uint32_t src_rb = (color & 0x00FF00FF) * alpha + 0x00800080;
    uint32_t src_g  = (color & 0x0000FF00) * alpha + 0x00008000;
    for (int i = 0; i < n; ++i) {
        uint32_t bg = d[i];
        uint32_t rb = ((src_rb + (bg & 0x00FF00FF) * inv_alpha) >> 8) & 0x00FF00FF;
        uint32_t g  = ((src_g  + (bg & 0x0000FF00) * inv_alpha) >> 8) & 0x0000FF00;
        d[i] = 0xFF000000 | rb | g;
    }
}

/* Blend pixels [x0, x1] (inclusive) of row y, clipped to clip and the surface */
static void zsr_hspan(
    zsr_surface_t *surface,
    int y,
    int x0,
    int x1,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    if (y < clip.y || y >= clip.y + clip.height || y < 0 || y >= surface->height)
        return;
    if (x0 < clip.x) x0 = clip.x;
    if (x1 > clip.x + clip.width - 1) x1 = clip.x + clip.width - 1;
    if (x0 < 0) x0 = 0;
    if (x1 > surface->width - 1) x1 = surface->width - 1;
    if (x0 > x1)
        return;

    uint32_t *row = (uint32_t *)((uint8_t *)surface->pixels + (size_t)y * surface->pitch);
    zsr_blend_run(row + x0, x1 - x0 + 1, color);
}

/* Row extent of the rounded-rect inside test for row y: the straight bands
 * through the rect are unbounded and corners use circles of `radius`, so
 * callers intersect with their visible area.
 * Returns false when no pixel of the row is inside; *all = whole row inside. */
static bool zsr_round_row_span(zsr_rect_t rect, int radius, int y, int *l, int *r, bool *all)
{
    *all = false;
    if (radius <= 0) {
        *all = true;
        return true;
    }

    int right = rect.x + rect.width - 1;
    int bottom = rect.y + rect.height - 1;
    int cy_top = rect.y + radius;
    int cy_bot = bottom - radius;

    if (y >= cy_top && y <= cy_bot) {
        *all = true;
        return true;
    }

    int dy = y < cy_top ? y - cy_top : y - cy_bot;
    int rem = radius * radius - dy * dy;
    int cx_left = rect.x + radius;
    int cx_right = right - radius;

    if (rem < 0) {
        *l = cx_left;
        *r = cx_right;
    } else {
        int d = 0;
        while ((d + 1) * (d + 1) <= rem)
            ++d;
        *l = cx_left - d;
        *r = cx_right + d > cx_left - 1 ? cx_right + d : cx_left - 1;
    }
    return *l <= *r;
}

static void zsr_fill_rect_clipped(
    zsr_surface_t *surface,
    zsr_rect_t rect,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    rect = zsr_rect_clip(rect, clip);

    if (zsr_rect_empty(rect) || !surface || !surface->pixels)
        return;

    for (int y = rect.y; y < rect.y + rect.height; ++y)
        zsr_hspan(surface, y, rect.x, rect.x + rect.width - 1, rect, color);
}


static void zsr_fill_rounded_rect_clipped(
    zsr_surface_t *surface,
    zsr_rect_t rect,
    int radius,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    zsr_rect_t visible = zsr_rect_clip(rect, clip);
    if (zsr_rect_empty(visible))
        return;

    uint8_t alpha = ZSR_ALPHA(color);
    if (alpha == 0 || !surface || !surface->pixels) return;

    for (int y = visible.y; y < visible.y + visible.height; ++y) {
        int l, r;
        bool all;
        if (!zsr_round_row_span(rect, radius, y, &l, &r, &all))
            continue;
        if (all) {
            l = visible.x;
            r = visible.x + visible.width - 1;
        }
        zsr_hspan(surface, y, l, r, visible, color);
    }
}

static void zsr_draw_border_clipped(
    zsr_surface_t *surface,
    zsr_rect_t rect,
    int radius,
    int thickness,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    if (thickness <= 0)
        return;

    for (int i = 0; i < thickness; ++i) {
        zsr_rect_t outer = zsr_rect_make(
            rect.x + i,
            rect.y + i,
            rect.width - i * 2,
            rect.height - i * 2
        );

        zsr_rect_t inner = zsr_rect_make(
            outer.x + 1,
            outer.y + 1,
            outer.width - 2,
            outer.height - 2
        );

        zsr_rect_t visible = zsr_rect_clip(outer, clip);
        if (zsr_rect_empty(visible)) continue;

        if (!surface || !surface->pixels) return;

        int vx0 = visible.x;
        int vx1 = visible.x + visible.width - 1;

        for (int y = visible.y; y < visible.y + visible.height; ++y) {
            int ol, orr, il, ir;
            bool o_all, i_all;
            if (!zsr_round_row_span(outer, radius - i, y, &ol, &orr, &o_all))
                continue;
            if (o_all) { ol = vx0; orr = vx1; }
            if (ol < vx0) ol = vx0;
            if (orr > vx1) orr = vx1;
            if (ol > orr)
                continue;

            if (!zsr_round_row_span(inner, radius - i - 1, y, &il, &ir, &i_all)) {
                zsr_hspan(surface, y, ol, orr, visible, color);
                continue;
            }
            if (i_all)
                continue;

            /* outer minus inner: up to two segments */
            if (ol < il)
                zsr_hspan(surface, y, ol, il - 1 < orr ? il - 1 : orr, visible, color);
            if (orr > ir)
                zsr_hspan(surface, y, ir + 1 > ol ? ir + 1 : ol, orr, visible, color);
        }
    }
}

static void zsr_draw_line_clipped(
    zsr_surface_t *surface,
    int x0,
    int y0,
    int x1,
    int y1,
    int thickness,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    int half_t = thickness / 2;
    int min_x = (x0 < x1 ? x0 : x1) - half_t, max_x = (x0 > x1 ? x0 : x1) + half_t;
    int min_y = (y0 < y1 ? y0 : y1) - half_t, max_y = (y0 > y1 ? y0 : y1) + half_t;
    if (max_x < clip.x || min_x >= clip.x + clip.width ||
        max_y < clip.y || min_y >= clip.y + clip.height)
        return;

    if (y0 == y1 && thickness <= 1) {
        if (surface && surface->pixels)
            zsr_hspan(surface, y0, min_x, max_x, clip, color);
        return;
    }

    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;

    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;

    int error = dx + dy;

    while (true) {
        int half = thickness / 2;

        for (int oy = -half; oy <= half; ++oy) {
            for (int ox = -half; ox <= half; ++ox) {
                int px = x0 + ox;
                int py = y0 + oy;

                if (px >= clip.x &&
                    py >= clip.y &&
                    px < clip.x + clip.width &&
                    py < clip.y + clip.height) {
                    zsr_blend_pixel(surface, px, py, color);
                }
            }
        }

        if (x0 == x1 && y0 == y1)
            break;

        int twice_error = error * 2;

        if (twice_error >= dy) {
            error += dy;
            x0 += sx;
        }

        if (twice_error <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static int zsr_edge_function(
    zsr_point_t a,
    zsr_point_t b,
    zsr_point_t p
)
{
    return (p.x - a.x) * (b.y - a.y) -
           (p.y - a.y) * (b.x - a.x);
}

static void zsr_fill_triangle_clipped(
    zsr_surface_t *surface,
    zsr_point_t a,
    zsr_point_t b,
    zsr_point_t c,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    int min_x = a.x;
    int max_x = a.x;
    int min_y = a.y;
    int max_y = a.y;

    zsr_point_t points[3] = { a, b, c };

    for (int i = 0; i < 3; ++i) {
        if (points[i].x < min_x) min_x = points[i].x;
        if (points[i].x > max_x) max_x = points[i].x;
        if (points[i].y < min_y) min_y = points[i].y;
        if (points[i].y > max_y) max_y = points[i].y;
    }

    zsr_rect_t bounds = zsr_rect_make(
        min_x,
        min_y,
        max_x - min_x + 1,
        max_y - min_y + 1
    );

    bounds = zsr_rect_clip(bounds, clip);

    if (zsr_rect_empty(bounds))
        return;

    for (int y = bounds.y; y < bounds.y + bounds.height; ++y) {
        for (int x = bounds.x; x < bounds.x + bounds.width; ++x) {
            zsr_point_t p = { x, y };

            int w0 = zsr_edge_function(b, c, p);
            int w1 = zsr_edge_function(c, a, p);
            int w2 = zsr_edge_function(a, b, p);

            bool inside =
                (w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                (w0 <= 0 && w1 <= 0 && w2 <= 0);

            if (inside)
                zsr_blend_pixel(surface, x, y, color);
        }
    }
}

/* ============================================================
   4) ANTI-ALIASED TEXT RENDERING
   ============================================================ */

static void zsr_draw_fake_text_clipped(
    zsr_surface_t *surface,
    int x,
    int y,
    const char *text,
    int scale,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    if (!surface || !surface->pixels || !text || zsr_rect_empty(clip))
        return;

    if (scale < 1)
        scale = 1;

    int text_alpha = ZSR_ALPHA(color);
    if (text_alpha == 0)
        return;

    int cur_x = x;
    int cur_y = y;

    while (*text) {
        char c = *text++;
        if (c == '\n') {
            cur_y += FONT_HEIGHT * scale;
            cur_x = x;
            continue;
        }

        for (int cy = 0; cy < FONT_HEIGHT; ++cy) {
            int py_base = cur_y + cy * scale;
            const uint8_t *alpha_row = font_get_glyph_alpha_row(c, cy);

            for (int cx = 0; cx < FONT_WIDTH; ++cx) {
                uint8_t glyph_a = alpha_row ? alpha_row[cx] : 0;
                if (!alpha_row) {
                    const uint8_t *raw_glyph = font_get_glyph(c);
                    if (raw_glyph && (raw_glyph[cy] & (1 << (FONT_WIDTH - 1 - cx)))) {
                        glyph_a = 255;
                    }
                }

                if (glyph_a == 0)
                    continue;

                uint8_t final_a = (uint8_t)((text_alpha * (int)glyph_a) / 255);
                if (final_a == 0)
                    continue;

                zsr_color_t pixel_color = ZSR_RGBA(
                    ZSR_RED(color),
                    ZSR_GREEN(color),
                    ZSR_BLUE(color),
                    final_a
                );

                int px_base = cur_x + cx * scale;

                if (scale == 1) {
                    if (px_base >= clip.x && px_base < clip.x + clip.width &&
                        py_base >= clip.y && py_base < clip.y + clip.height) {
                        zsr_blend_pixel(surface, px_base, py_base, pixel_color);
                    }
                } else {
                    for (int sy = 0; sy < scale; ++sy) {
                        int py = py_base + sy;
                        if (py < clip.y || py >= clip.y + clip.height)
                            continue;

                        for (int sx = 0; sx < scale; ++sx) {
                            int px = px_base + sx;
                            if (px >= clip.x && px < clip.x + clip.width) {
                                zsr_blend_pixel(surface, px, py, pixel_color);
                            }
                        }
                    }
                }
            }
        }

        cur_x += FONT_WIDTH * scale;
    }
}

/* Blit external buffer (e.g. app surface) */
static void zsr_blit_buffer_clipped(
    zsr_surface_t *surface,
    zsr_rect_t rect,
    uint32_t *src_buffer,
    int src_pitch,
    zsr_rect_t clip
)
{
    if (!surface || !surface->pixels || !src_buffer)
        return;

    zsr_rect_t visible = zsr_rect_clip(rect, clip);
    if (zsr_rect_empty(visible))
        return;

    int src_stride = src_pitch / 4;

    for (int y = visible.y; y < visible.y + visible.height; ++y) {
        int src_y = y - rect.y;
        int dst_row = y * (surface->pitch / 4);
        int src_row = src_y * src_stride;

        for (int x = visible.x; x < visible.x + visible.width; ++x) {
            int src_x = x - rect.x;
            uint32_t pixel = src_buffer[src_row + src_x];
            uint32_t a = (pixel >> 24) & 0xFF;
            if (a == 255) {
                surface->pixels[dst_row + x] = pixel;
            } else if (a > 0) {
                zsr_blend_pixel(surface, x, y, pixel);
            }
        }
    }
}

/* ============================================================
   5) XML / ZML PARSER
   ============================================================ */

static zsr_node_type_t zsr_tag_type(const char *tag)
{
    if (zsr_string_equal(tag, "desktop"))     return ZSR_NODE_DESKTOP;
    if (zsr_string_equal(tag, "glow"))        return ZSR_NODE_GLOW;
    if (zsr_string_equal(tag, "window"))      return ZSR_NODE_WINDOW;
    if (zsr_string_equal(tag, "titlebar"))    return ZSR_NODE_TITLEBAR;
    if (zsr_string_equal(tag, "row"))         return ZSR_NODE_ROW;
    if (zsr_string_equal(tag, "column"))      return ZSR_NODE_COLUMN;
    if (zsr_string_equal(tag, "text"))        return ZSR_NODE_TEXT;
    if (zsr_string_equal(tag, "tabs"))        return ZSR_NODE_TABS;
    if (zsr_string_equal(tag, "tab"))         return ZSR_NODE_TAB;
    if (zsr_string_equal(tag, "dock"))        return ZSR_NODE_DOCK;
    if (zsr_string_equal(tag, "dock-item"))   return ZSR_NODE_DOCK_ITEM;
    if (zsr_string_equal(tag, "icon"))        return ZSR_NODE_ICON;
    if (zsr_string_equal(tag, "spacer"))      return ZSR_NODE_SPACER;
    if (zsr_string_equal(tag, "bottom-bar"))  return ZSR_NODE_BOTTOM_BAR;
    if (zsr_string_equal(tag, "status-dot"))  return ZSR_NODE_STATUS_DOT;
    if (zsr_string_equal(tag, "app-surface")) return ZSR_NODE_APP_SURFACE;

    return ZSR_NODE_UNKNOWN;
}

static void zsr_set_parse_error(
    zsr_parse_error_t *error,
    const char *message,
    uint32_t offset,
    uint32_t line,
    uint32_t column
)
{
    if (!error)
        return;

    error->message = message;
    error->offset = offset;
    error->line = line;
    error->column = column;
}

static void zsr_skip_spaces(
    const char *source,
    uint32_t length,
    uint32_t *pos,
    uint32_t *line,
    uint32_t *column
)
{
    while (*pos < length && zsr_is_space(source[*pos])) {
        if (source[*pos] == '\n') {
            ++(*line);
            *column = 1;
        } else {
            ++(*column);
        }
        ++(*pos);
    }
}

static bool zsr_read_name(
    const char *source,
    uint32_t length,
    uint32_t *pos,
    char *out,
    uint32_t out_size
)
{
    uint32_t start = *pos;

    if (*pos >= length || !zsr_is_name_start(source[*pos]))
        return false;

    ++(*pos);

    while (*pos < length && zsr_is_name_char(source[*pos]))
        ++(*pos);

    zsr_copy_string(out, out_size, source + start, *pos - start);
    return true;
}

static zsr_node_t *zsr_new_node(zsr_document_t *document)
{
    if (document->node_count >= ZSR_MAX_NODES)
        return 0;

    zsr_node_t *node = &document->nodes[document->node_count++];

    node->type = ZSR_NODE_UNKNOWN;
    node->tag[0] = '\0';
    node->attr_count = 0;

    node->parent = 0;
    node->first_child = 0;
    node->last_child = 0;
    node->next_sibling = 0;

    node->bounds = zsr_rect_make(0, 0, 0, 0);
    node->old_bounds = zsr_rect_make(0, 0, 0, 0);

    node->visible = true;
    node->needs_layout = true;
    node->needs_paint = true;
    node->cacheable = false;

    return node;
}

static void zsr_add_child(zsr_node_t *parent, zsr_node_t *child)
{
    child->parent = parent;

    if (!parent->first_child) {
        parent->first_child = child;
        parent->last_child = child;
        return;
    }

    parent->last_child->next_sibling = child;
    parent->last_child = child;
}

static bool zsr_parse_attribute(
    const char *source,
    uint32_t length,
    uint32_t *pos,
    uint32_t *line,
    uint32_t *column,
    zsr_node_t *node,
    zsr_parse_error_t *error
)
{
    if (node->attr_count >= ZSR_MAX_ATTRS) {
        zsr_set_parse_error(error, "Too many attributes", *pos, *line, *column);
        return false;
    }

    zsr_attr_t *attribute = &node->attrs[node->attr_count];

    if (!zsr_read_name(source, length, pos, attribute->name, sizeof(attribute->name))) {
        zsr_set_parse_error(error, "Invalid attribute name", *pos, *line, *column);
        return false;
    }

    zsr_skip_spaces(source, length, pos, line, column);

    if (*pos >= length || source[*pos] != '=') {
        zsr_set_parse_error(error, "Expected '=' after attribute name", *pos, *line, *column);
        return false;
    }

    ++(*pos);
    ++(*column);

    zsr_skip_spaces(source, length, pos, line, column);

    if (*pos >= length || (source[*pos] != '"' && source[*pos] != '\'')) {
        zsr_set_parse_error(error, "Attribute values must use quotes", *pos, *line, *column);
        return false;
    }

    char quote = source[*pos];
    ++(*pos);
    ++(*column);

    uint32_t start = *pos;

    while (*pos < length && source[*pos] != quote) {
        char c = source[*pos];
        if (c == '<' || c == '>' || c == '\n') {
            zsr_set_parse_error(error, "Invalid character in attribute value", *pos, *line, *column);
            return false;
        }
        ++(*pos);
        ++(*column);
    }

    if (*pos >= length) {
        zsr_set_parse_error(error, "Unterminated attribute value", *pos, *line, *column);
        return false;
    }

    zsr_copy_string(attribute->value, sizeof(attribute->value), source + start, *pos - start);

    ++(*pos);
    ++(*column);
    ++node->attr_count;

    return true;
}

static bool zsr_parse_zml(
    const char *source,
    uint32_t length,
    zsr_document_t *document,
    zsr_node_t **out_root,
    zsr_parse_error_t *error
)
{
    zsr_node_t *stack[ZSR_MAX_DEPTH];

    uint32_t depth = 0;
    uint32_t pos = 0;
    uint32_t line = 1;
    uint32_t column = 1;

    if (!source || !document || !out_root) {
        zsr_set_parse_error(error, "Invalid parser input", 0, 1, 1);
        return false;
    }

    document->node_count = 0;
    *out_root = 0;

    while (pos < length) {
        zsr_skip_spaces(source, length, &pos, &line, &column);

        if (pos >= length)
            break;

        if (source[pos] != '<') {
            zsr_set_parse_error(error, "Text between tags is not supported; use value=\"...\"", pos, line, column);
            return false;
        }

        if (pos + 1 < length && (source[pos + 1] == '!' || source[pos + 1] == '?')) {
            zsr_set_parse_error(error, "DOCTYPE, ENTITY, comments and PI are disabled", pos, line, column);
            return false;
        }

        ++pos;
        ++column;

        /* Closing tag: </window> */
        if (pos < length && source[pos] == '/') {
            char closing_tag[ZSR_MAX_TAG];

            ++pos;
            ++column;

            if (!zsr_read_name(source, length, &pos, closing_tag, sizeof(closing_tag))) {
                zsr_set_parse_error(error, "Invalid closing tag", pos, line, column);
                return false;
            }

            zsr_skip_spaces(source, length, &pos, &line, &column);

            if (pos >= length || source[pos] != '>') {
                zsr_set_parse_error(error, "Expected '>' after closing tag", pos, line, column);
                return false;
            }

            ++pos;
            ++column;

            if (depth == 0) {
                zsr_set_parse_error(error, "Closing tag without opening tag", pos, line, column);
                return false;
            }

            if (!zsr_string_equal(stack[depth - 1]->tag, closing_tag)) {
                zsr_set_parse_error(error, "Closing tag does not match opening tag", pos, line, column);
                return false;
            }

            --depth;
            continue;
        }

        /* Opening tag: <window ...> or self closing <text ... /> */
        zsr_node_t *node = zsr_new_node(document);

        if (!node) {
            zsr_set_parse_error(error, "Maximum number of ZML nodes reached", pos, line, column);
            return false;
        }

        if (!zsr_read_name(source, length, &pos, node->tag, sizeof(node->tag))) {
            zsr_set_parse_error(error, "Invalid opening tag", pos, line, column);
            return false;
        }

        node->type = zsr_tag_type(node->tag);

        if (node->type == ZSR_NODE_UNKNOWN) {
            zsr_set_parse_error(error, "Unsupported ZML tag", pos, line, column);
            return false;
        }

        bool self_closing = false;
        bool tag_finished = false;

        while (pos < length && !tag_finished) {
            zsr_skip_spaces(source, length, &pos, &line, &column);

            if (pos >= length) {
                zsr_set_parse_error(error, "Unexpected end of file", pos, line, column);
                return false;
            }

            if (source[pos] == '>') {
                ++pos;
                ++column;
                tag_finished = true;
                break;
            }

            if (source[pos] == '/' && pos + 1 < length && source[pos + 1] == '>') {
                pos += 2;
                column += 2;
                self_closing = true;
                tag_finished = true;
                break;
            }

            if (!zsr_parse_attribute(source, length, &pos, &line, &column, node, error)) {
                return false;
            }
        }

        if (depth == 0) {
            if (*out_root) {
                zsr_set_parse_error(error, "Only one root element is allowed", pos, line, column);
                return false;
            }
            *out_root = node;
        } else {
            zsr_add_child(stack[depth - 1], node);
        }

        if (!self_closing) {
            if (depth >= ZSR_MAX_DEPTH) {
                zsr_set_parse_error(error, "Maximum ZML nesting depth reached", pos, line, column);
                return false;
            }
            stack[depth++] = node;
        }
    }

    if (!*out_root) {
        zsr_set_parse_error(error, "No root element found", pos, line, column);
        return false;
    }

    if (depth != 0) {
        zsr_set_parse_error(error, "Unclosed element", pos, line, column);
        return false;
    }

    return true;
}

static const char *zsr_attr(const zsr_node_t *node, const char *name)
{
    if (!node || !name)
        return 0;

    for (uint32_t i = 0; i < node->attr_count; ++i) {
        if (zsr_string_equal(node->attrs[i].name, name))
            return node->attrs[i].value;
    }

    return 0;
}

/* ============================================================
   6) DAMAGE TRACKER
   ============================================================ */

static void zsr_damage_reset(zsr_damage_t *damage)
{
    damage->count = 0;
    damage->full_redraw = false;
}

static void zsr_damage_add(zsr_damage_t *damage, zsr_rect_t rect)
{
    if (zsr_rect_empty(rect) || damage->full_redraw)
        return;

    for (uint32_t i = 0; i < damage->count; ++i) {
        if (zsr_rect_near_or_intersects(damage->rects[i], rect, 12)) {
            damage->rects[i] = zsr_rect_union(damage->rects[i], rect);
            return;
        }
    }

    if (damage->count >= ZSR_MAX_DAMAGE) {
        damage->count = 0;
        damage->full_redraw = true;
        return;
    }

    damage->rects[damage->count++] = rect;
}

/* ============================================================
   7) SMART RENDER COMMANDS
   ============================================================ */

static void zsr_commands_reset(zsr_command_list_t *list)
{
    list->count = 0;
}

static zsr_command_t *zsr_command_push(
    zsr_command_list_t *list,
    zsr_opcode_t opcode
)
{
    if (list->count >= ZSR_MAX_COMMANDS)
        return 0;

    zsr_command_t *command = &list->commands[list->count++];

    command->opcode = opcode;
    command->rect = zsr_rect_make(0, 0, 0, 0);
    command->clip = zsr_rect_make(0, 0, 0, 0);

    command->color = 0;
    command->color2 = 0;

    command->x0 = 0;
    command->y0 = 0;
    command->x1 = 0;
    command->y1 = 0;

    command->radius = 0;
    command->thickness = 1;

    command->a = (zsr_point_t){0, 0};
    command->b = (zsr_point_t){0, 0};
    command->c = (zsr_point_t){0, 0};

    command->text[0] = '\0';
    command->text_scale = 1;

    command->buffer = 0;
    command->buffer_pitch = 0;

    return command;
}

static void zsr_aa_rrect_clipped(zsr_surface_t *s, zsr_rect_t R, int r, zsr_rect_t clip, zsr_color_t color);
static void zsr_aa_ring_clipped(zsr_surface_t *s, zsr_rect_t R, int r, int t, zsr_rect_t clip, zsr_color_t color);
static void zsr_aa_line_clipped(zsr_surface_t *s, int x0, int y0, int x1, int y1, int t, zsr_rect_t clip, zsr_color_t color);
static void zsr_shadow_clipped(zsr_surface_t *s, zsr_rect_t R, int r, int blur, int dy, zsr_rect_t clip, zsr_color_t color);
static void zsr_ttf_draw_clipped(zsr_surface_t *s, int x, int y, const char *t, zsr_rect_t clip, zsr_color_t color);
static void zsr_ring_gap_clipped(zsr_surface_t *s, zsr_rect_t R, int t, int gap, zsr_rect_t clip, zsr_color_t color);

static void zsr_execute_command(
    zsr_surface_t *surface,
    const zsr_command_t *command,
    zsr_rect_t damage_clip
)
{
    zsr_rect_t clip = zsr_rect_clip(command->clip, damage_clip);

    if (zsr_rect_empty(clip))
        return;

    if (command->opcode == ZSR_CMD_TEXT) {
        int lines = 1, cols = 0, max_cols = 0;
        for (const char *t = command->text; *t; ++t) {
            if (*t == '\n') { ++lines; cols = 0; continue; }
            if (++cols > max_cols) max_cols = cols;
        }
        int sc = command->text_scale < 1 ? 1 : command->text_scale;
        zsr_rect_t bounds = zsr_rect_make(command->rect.x, command->rect.y,
                                          max_cols * FONT_WIDTH * sc, lines * FONT_HEIGHT * sc);
        if (!zsr_rect_intersects(bounds, clip))
            return;
    }

    switch (command->opcode) {
    case ZSR_CMD_FILL_RECT:
        zsr_fill_rect_clipped(surface, command->rect, clip, command->color);
        break;

    case ZSR_CMD_ROUNDED_RECT:
        zsr_fill_rounded_rect_clipped(surface, command->rect, command->radius, clip, command->color);
        break;

    case ZSR_CMD_BORDER:
        zsr_draw_border_clipped(surface, command->rect, command->radius, command->thickness, clip, command->color);
        break;

    case ZSR_CMD_LINE:
        zsr_draw_line_clipped(surface, command->x0, command->y0, command->x1, command->y1, command->thickness, clip, command->color);
        break;

    case ZSR_CMD_TRIANGLE:
        zsr_fill_triangle_clipped(surface, command->a, command->b, command->c, clip, command->color);
        break;

    case ZSR_CMD_TEXT:
        zsr_draw_fake_text_clipped(surface, command->rect.x, command->rect.y, command->text, command->text_scale, clip, command->color);
        break;

    case ZSR_CMD_BLIT_BUFFER:
        zsr_blit_buffer_clipped(surface, command->rect, command->buffer, command->buffer_pitch, clip);
        break;

    case ZSR_CMD_AA_ROUNDED:
        zsr_aa_rrect_clipped(surface, command->rect, command->radius, clip, command->color);
        break;
    case ZSR_CMD_AA_RING:
        zsr_aa_ring_clipped(surface, command->rect, command->radius, command->thickness, clip, command->color);
        break;
    case ZSR_CMD_AA_LINE:
        zsr_aa_line_clipped(surface, command->x0, command->y0, command->x1, command->y1, command->thickness, clip, command->color);
        break;
    case ZSR_CMD_SHADOW:
        zsr_shadow_clipped(surface, command->rect, command->radius, command->thickness, command->y0, clip, command->color);
        break;
    case ZSR_CMD_TTF_TEXT:
        zsr_ttf_draw_clipped(surface, command->rect.x, command->rect.y, command->text, clip, command->color);
        break;
    case ZSR_CMD_RING_GAP:
        zsr_ring_gap_clipped(surface, command->rect, command->thickness, command->x0, clip, command->color);
        break;
    }
}


/* ============================================================
   8) ZARQAA BACKGROUND & GLOW GENERATION
   ============================================================ */

static void zsr_draw_glow_region(
    zsr_surface_t *surface,
    zsr_rect_t clip,
    int center_x,
    int center_y,
    int radius,
    zsr_color_t color
)
{
    zsr_rect_t glow_bounds = zsr_rect_make(
        center_x - radius,
        center_y - radius,
        radius * 2,
        radius * 2
    );

    zsr_rect_t visible = zsr_rect_clip(glow_bounds, clip);

    if (zsr_rect_empty(visible))
        return;

    int radius_sq = radius * radius;
    uint8_t original_alpha = ZSR_ALPHA(color);

    for (int y = visible.y; y < visible.y + visible.height; ++y) {
        for (int x = visible.x; x < visible.x + visible.width; ++x) {
            int dx = x - center_x;
            int dy = y - center_y;
            int distance_sq = dx * dx + dy * dy;

            if (distance_sq >= radius_sq)
                continue;

            uint8_t alpha = (uint8_t)(
                ((radius_sq - distance_sq) * original_alpha) /
                radius_sq
            );

            zsr_blend_pixel(
                surface,
                x,
                y,
                ZSR_RGBA(
                    ZSR_RED(color),
                    ZSR_GREEN(color),
                    ZSR_BLUE(color),
                    alpha
                )
            );
        }
    }
}

static uint32_t *g_wallpaper_cache = NULL;
static int g_wallpaper_cache_width = 0;
static int g_wallpaper_cache_height = 0;

static void zsr_pre_render_wallpaper(int width, int height) {
    if (g_wallpaper_cache && g_wallpaper_cache_width == width && g_wallpaper_cache_height == height) {
        return;
    }

    if (g_wallpaper_cache) {
        kfree(g_wallpaper_cache);
        g_wallpaper_cache = NULL;
    }

    size_t size = (size_t)width * height * sizeof(uint32_t);
    g_wallpaper_cache = (uint32_t *)kmalloc(size);
    if (!g_wallpaper_cache) {
        return;
    }

    g_wallpaper_cache_width = width;
    g_wallpaper_cache_height = height;

    zsr_surface_t wallpaper_surf;
    wallpaper_surf.pixels = g_wallpaper_cache;
    wallpaper_surf.width = width;
    wallpaper_surf.height = height;
    wallpaper_surf.pitch = width * sizeof(uint32_t);

    for (int i = 0; i < width * height; ++i) {
        g_wallpaper_cache[i] = ZSR_RGBA(5, 14, 40, 255);
    }

    zsr_rect_t full_screen = zsr_rect_make(0, 0, width, height);

    /* Pre-render 4 Soft, Balanced, Harmonic Glows ONCE at startup */
    zsr_draw_glow_region(
        &wallpaper_surf,
        full_screen,
        width * 10 / 100,
        height * 20 / 100,
        width * 45 / 100,
        ZSR_RGBA(37, 99, 235, 110)
    );

    zsr_draw_glow_region(
        &wallpaper_surf,
        full_screen,
        width * 90 / 100,
        height * 35 / 100,
        width * 45 / 100,
        ZSR_RGBA(56, 189, 248, 80)
    );

    zsr_draw_glow_region(
        &wallpaper_surf,
        full_screen,
        width * 80 / 100,
        height * 90 / 100,
        width * 40 / 100,
        ZSR_RGBA(14, 116, 220, 90)
    );

    zsr_draw_glow_region(
        &wallpaper_surf,
        full_screen,
        width * 20 / 100,
        height * 80 / 100,
        width * 45 / 100,
        ZSR_RGBA(59, 130, 246, 85)
    );
}

static void zsr_draw_zarqaa_background_region(
    zsr_surface_t *surface,
    zsr_rect_t region
)
{
    if (!surface || !surface->pixels || zsr_rect_empty(region))
        return;

    zsr_rect_t visible = zsr_rect_clip(
        zsr_rect_make(0, 0, surface->width, surface->height),
        region
    );

    if (zsr_rect_empty(visible))
        return;

    /* Ultra-fast path: Blit directly from pre-rendered wallpaper cache using memcpy! */
    if (g_wallpaper_cache && g_wallpaper_cache_width == surface->width && g_wallpaper_cache_height == surface->height) {
        int pitch_pixels = surface->pitch / 4;
        for (int y = visible.y; y < visible.y + visible.height; ++y) {
            uint32_t *src = &g_wallpaper_cache[y * surface->width + visible.x];
            uint32_t *dst = &surface->pixels[y * pitch_pixels + visible.x];
            memcpy(dst, src, visible.width * sizeof(uint32_t));
        }
        return;
    }

    /* Fallback: Base deep calm background */
    for (int y = visible.y; y < visible.y + visible.height; ++y) {
        int dst_row = y * (surface->pitch / 4);
        for (int x = visible.x; x < visible.x + visible.width; ++x) {
            surface->pixels[dst_row + x] = ZSR_RGBA(15, 23, 42, 255);
        }
    }
}

/* Paint the wallpaper for region with the area covered by fill command c0
 * taken from tint (= wallpaper with c0's colour already blended in).
 * Identical to drawing the wallpaper and then executing c0. */
static void zsr_draw_background_folded(
    zsr_surface_t *surface,
    zsr_rect_t region,
    const zsr_command_t *c0,
    const uint32_t *wallpaper,
    const uint32_t *tint
)
{
    int stride = surface->pitch / 4;
    zsr_rect_t visible = zsr_rect_clip(c0->rect, zsr_rect_clip(c0->clip, region));
    bool rounded = c0->opcode == ZSR_CMD_ROUNDED_RECT;

    for (int y = region.y; y < region.y + region.height; ++y) {
        uint32_t *dst = surface->pixels + (size_t)y * stride;
        const uint32_t *wp = wallpaper + (size_t)y * surface->width;
        const uint32_t *tp = tint + (size_t)y * surface->width;

        memcpy(dst + region.x, wp + region.x, (size_t)region.width * 4);

        if (zsr_rect_empty(visible) || y < visible.y || y >= visible.y + visible.height)
            continue;

        int l = visible.x, r = visible.x + visible.width - 1;
        if (rounded) {
            int sl, sr;
            bool all;
            if (!zsr_round_row_span(c0->rect, c0->radius, y, &sl, &sr, &all))
                continue;
            if (!all) {
                if (sl > l) l = sl;
                if (sr < r) r = sr;
            }
        }
        if (l <= r)
            memcpy(dst + l, tp + l, (size_t)(r - l + 1) * 4);
    }
}

/* The first command usually paints straight onto the wallpaper (window shadow,
 * or the glass body while dragging), so its result there is cached per colour. */
#define ZSR_TINT_SLOTS 2
static uint32_t *g_tint_pixels[ZSR_TINT_SLOTS];
static zsr_color_t g_tint_color[ZSR_TINT_SLOTS];
static int g_tint_w[ZSR_TINT_SLOTS], g_tint_h[ZSR_TINT_SLOTS];
static int g_tint_next = 0;

static const uint32_t *zsr_tinted_wallpaper(zsr_color_t color, int width, int height)
{
    for (int i = 0; i < ZSR_TINT_SLOTS; ++i) {
        if (g_tint_pixels[i] && g_tint_color[i] == color &&
            g_tint_w[i] == width && g_tint_h[i] == height)
            return g_tint_pixels[i];
    }

    int slot = g_tint_next;
    g_tint_next = (g_tint_next + 1) % ZSR_TINT_SLOTS;

    size_t count = (size_t)width * height;
    if (!g_tint_pixels[slot] || g_tint_w[slot] != width || g_tint_h[slot] != height) {
        if (g_tint_pixels[slot]) kfree(g_tint_pixels[slot]);
        g_tint_pixels[slot] = (uint32_t *)kmalloc(count * 4);
        if (!g_tint_pixels[slot]) return NULL;
    }

    memcpy(g_tint_pixels[slot], g_wallpaper_cache, count * 4);
    zsr_blend_run(g_tint_pixels[slot], (int)count, color);
    g_tint_color[slot] = color;
    g_tint_w[slot] = width;
    g_tint_h[slot] = height;
    return g_tint_pixels[slot];
}

/* ============================================================
   9) COMMAND EMITTERS
   ============================================================ */

static void zsr_emit_rounded_rect(
    zsr_command_list_t *list,
    zsr_rect_t rect,
    int radius,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    zsr_command_t *command = zsr_command_push(list, ZSR_CMD_ROUNDED_RECT);
    if (!command) return;
    command->rect = rect;
    command->radius = radius;
    command->clip = clip;
    command->color = color;
}

static void zsr_emit_border(
    zsr_command_list_t *list,
    zsr_rect_t rect,
    int radius,
    int thickness,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    zsr_command_t *command = zsr_command_push(list, ZSR_CMD_BORDER);
    if (!command) return;
    command->rect = rect;
    command->radius = radius;
    command->thickness = thickness;
    command->clip = clip;
    command->color = color;
}

static void zsr_emit_line(
    zsr_command_list_t *list,
    int x0,
    int y0,
    int x1,
    int y1,
    int thickness,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    zsr_command_t *command = zsr_command_push(list, ZSR_CMD_LINE);
    if (!command) return;
    command->x0 = x0;
    command->y0 = y0;
    command->x1 = x1;
    command->y1 = y1;
    command->thickness = thickness;
    command->clip = clip;
    command->color = color;
}

static void zsr_emit_triangle(
    zsr_command_list_t *list,
    zsr_point_t a,
    zsr_point_t b,
    zsr_point_t c,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    zsr_command_t *command = zsr_command_push(list, ZSR_CMD_TRIANGLE);
    if (!command) return;
    command->a = a;
    command->b = b;
    command->c = c;
    command->clip = clip;
    command->color = color;
}

static void zsr_emit_text(
    zsr_command_list_t *list,
    int x,
    int y,
    const char *text,
    int scale,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    zsr_command_t *command = zsr_command_push(list, ZSR_CMD_TEXT);
    if (!command) return;

    command->rect = zsr_rect_make(x, y, 0, 0);
    command->clip = clip;
    command->color = color;
    command->text_scale = scale;

    if (!text) text = "";

    uint32_t length = 0;
    while (text[length] && length < ZSR_MAX_TEXT - 1)
        ++length;

    zsr_copy_string(command->text, sizeof(command->text), text, length);
}

static void zsr_emit_blit_buffer(
    zsr_command_list_t *list,
    zsr_rect_t rect,
    uint32_t *buffer,
    int pitch,
    zsr_rect_t clip
)
{
    zsr_command_t *command = zsr_command_push(list, ZSR_CMD_BLIT_BUFFER);
    if (!command) return;

    command->rect = rect;
    command->clip = clip;
    command->buffer = buffer;
    command->buffer_pitch = pitch;
}

/* ============================================================
   10) CANONICAL ZARQAA DESKTOP WINDOW BUILDER
   ============================================================ */

/* ============================================================
   UI POLISH LAYER: AA shapes, soft shadow, TTF text, chrome helpers
   (self-contained: does not depend on the low-level span helpers)
   ============================================================ */

#define ZSR_TOPBAR_H        30
#define ZSR_WIN_PAD         8
#define ZSR_CHROME_RADIUS   14
#define ZSR_SHADOW_BLUR     22
#define ZSR_SHADOW_DY       8
#define ZSR_TTF_SIZE        16
#define ZSR_DIV255(v)       ((((v) + 128) * 257) >> 16)

static int g_mouse_x = -1;
static int g_mouse_y = -1;
static zsr_rect_t g_old_mouse_rect = {0, 0, 0, 0};
static bool g_window_open = false; /* Start with an empty, clean desktop! */
static int g_screen_w = 1280;
static int g_screen_h = 720;

static uint32_t zsr_isqrt32(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}

static inline uint32_t zsr_px_over(uint32_t dst, zsr_color_t c, int a)
{
    if (a <= 0) return dst;
    if (a >= 255) return 0xFF000000u | (c & 0x00FFFFFFu);
    uint32_t ia = 255 - a;
    uint32_t rb = (((c & 0x00FF00FFu) * a + (dst & 0x00FF00FFu) * ia + 0x00800080u) >> 8) & 0x00FF00FFu;
    uint32_t g  = (((c & 0x0000FF00u) * a + (dst & 0x0000FF00u) * ia + 0x00008000u) >> 8) & 0x0000FF00u;
    return 0xFF000000u | rb | g;
}

static void zsr_span_over(uint32_t *d, int n, zsr_color_t c, int a)
{
    if (n <= 0 || a <= 0) return;
    if (a >= 255) {
        uint32_t v = 0xFF000000u | (c & 0x00FFFFFFu);
        for (int i = 0; i < n; ++i) d[i] = v;
        return;
    }
    uint32_t sr = (c & 0x00FF00FFu) * a, sg = (c & 0x0000FF00u) * a, ia = 255 - a;
    for (int i = 0; i < n; ++i) {
        uint32_t bg = d[i];
        uint32_t rb = ((sr + (bg & 0x00FF00FFu) * ia + 0x00800080u) >> 8) & 0x00FF00FFu;
        uint32_t g  = ((sg + (bg & 0x0000FF00u) * ia + 0x00008000u) >> 8) & 0x0000FF00u;
        d[i] = 0xFF000000u | rb | g;
    }
}

/* Coverage (0..255) of pixel (x,y) inside rounded rect R with corner radius r */
static int zsr_rr_cov(int x, int y, zsr_rect_t R, int r)
{
    if (x < R.x || y < R.y || x >= R.x + R.width || y >= R.y + R.height) return 0;
    int cx, cy;
    if (x < R.x + r) cx = R.x + r;
    else if (x >= R.x + R.width - r) cx = R.x + R.width - r;
    else return 255;
    if (y < R.y + r) cy = R.y + r;
    else if (y >= R.y + R.height - r) cy = R.y + R.height - r;
    else return 255;
    int dx = cx * 4 - (x * 4 + 2); if (dx < 0) dx = -dx;
    int dy = cy * 4 - (y * 4 + 2); if (dy < 0) dy = -dy;
    int d = (int)zsr_isqrt32((uint32_t)(dx * dx + dy * dy)) - r * 4;
    int cov = (2 - d) * 64;
    return cov < 0 ? 0 : (cov > 255 ? 255 : cov);
}

static zsr_rect_t zsr_aa_visible(zsr_surface_t *s, zsr_rect_t R, zsr_rect_t clip)
{
    return zsr_rect_clip(R, zsr_rect_clip(clip, zsr_rect_make(0, 0, s->width, s->height)));
}

static int zsr_clamp_radius(zsr_rect_t R, int r)
{
    if (r * 2 > R.width) r = R.width / 2;
    if (r * 2 > R.height) r = R.height / 2;
    return r < 0 ? 0 : r;
}

static void zsr_aa_rrect_clipped(zsr_surface_t *s, zsr_rect_t R, int r, zsr_rect_t clip, zsr_color_t color)
{
    int a0 = ZSR_ALPHA(color);
    zsr_rect_t v = zsr_aa_visible(s, R, clip);
    if (!a0 || zsr_rect_empty(v)) return;
    r = zsr_clamp_radius(R, r);
    int stride = s->pitch / 4;
    int ls = R.x + r, rs = R.x + R.width - r;
    for (int y = v.y; y < v.y + v.height; ++y) {
        uint32_t *row = s->pixels + (size_t)y * stride;
        int xa = v.x, xb = v.x + v.width;
        if (y < R.y + r || y >= R.y + R.height - r) {
            int e = xb < ls ? xb : ls;
            for (int x = xa; x < e; ++x)
                row[x] = zsr_px_over(row[x], color, ZSR_DIV255(a0 * zsr_rr_cov(x, y, R, r)));
            int b = xa > rs ? xa : rs;
            for (int x = b; x < xb; ++x)
                row[x] = zsr_px_over(row[x], color, ZSR_DIV255(a0 * zsr_rr_cov(x, y, R, r)));
            int ms = xa > ls ? xa : ls, me = xb < rs ? xb : rs;
            if (me > ms) zsr_span_over(row + ms, me - ms, color, a0);
        } else {
            zsr_span_over(row + xa, xb - xa, color, a0);
        }
    }
}

static void zsr_ring_pixels(uint32_t *row, int y, int xs, int xe, zsr_rect_t R, int r,
                            zsr_rect_t I, int ri, zsr_color_t c, int a0)
{
    for (int x = xs; x < xe; ++x) {
        int cov = zsr_rr_cov(x, y, R, r) - zsr_rr_cov(x, y, I, ri);
        if (cov > 0) row[x] = zsr_px_over(row[x], c, ZSR_DIV255(a0 * cov));
    }
}

static void zsr_aa_ring_clipped(zsr_surface_t *s, zsr_rect_t R, int r, int t, zsr_rect_t clip, zsr_color_t color)
{
    int a0 = ZSR_ALPHA(color);
    zsr_rect_t v = zsr_aa_visible(s, R, clip);
    if (!a0 || t < 1 || zsr_rect_empty(v)) return;
    r = zsr_clamp_radius(R, r);
    zsr_rect_t I = zsr_rect_make(R.x + t, R.y + t, R.width - 2 * t, R.height - 2 * t);
    int ri = r > t ? r - t : 0;
    int band = r > t ? r : t;
    int stride = s->pitch / 4;
    int right = R.x + R.width;
    for (int y = v.y; y < v.y + v.height; ++y) {
        uint32_t *row = s->pixels + (size_t)y * stride;
        int xa = v.x, xb = v.x + v.width;
        bool edge = y < R.y + band || y >= R.y + R.height - band;
        bool thick = y < R.y + t || y >= R.y + R.height - t;
        if (edge) {
            int l1 = R.x + band, r0 = right - band;
            int e = xb < l1 ? xb : l1;
            zsr_ring_pixels(row, y, xa, e, R, r, I, ri, color, a0);
            int b = xa > r0 ? xa : r0;
            zsr_ring_pixels(row, y, b, xb, R, r, I, ri, color, a0);
            if (thick) {
                int ms = xa > l1 ? xa : l1, me = xb < r0 ? xb : r0;
                if (me > ms) zsr_span_over(row + ms, me - ms, color, a0);
            }
        } else {
            int e = xb < R.x + t ? xb : R.x + t;
            if (e > xa) zsr_span_over(row + xa, e - xa, color, a0);
            int b = xa > right - t ? xa : right - t;
            if (xb > b) zsr_span_over(row + b, xb - b, color, a0);
        }
    }
}

/* Endpoints are pixel-centre coordinates; t is the line width in pixels */
static void zsr_aa_line_clipped(zsr_surface_t *s, int x0, int y0, int x1, int y1, int t,
                                zsr_rect_t clip, zsr_color_t color)
{
    int a0 = ZSR_ALPHA(color);
    int minx = x0 < x1 ? x0 : x1, maxx = x0 < x1 ? x1 : x0;
    int miny = y0 < y1 ? y0 : y1, maxy = y0 < y1 ? y1 : y0;
    zsr_rect_t bb = zsr_rect_make(minx - t - 1, miny - t - 1, maxx - minx + 2 * t + 3, maxy - miny + 2 * t + 3);
    zsr_rect_t v = zsr_aa_visible(s, bb, clip);
    if (!a0 || zsr_rect_empty(v) || maxx - minx > 400 || maxy - miny > 400) return;
    int ax = x0 * 4 + 2, ay = y0 * 4 + 2, vx = (x1 - x0) * 4, vy = (y1 - y0) * 4;
    int len2 = vx * vx + vy * vy, half = t * 2, stride = s->pitch / 4;
    for (int y = v.y; y < v.y + v.height; ++y) {
        uint32_t *row = s->pixels + (size_t)y * stride;
        for (int x = v.x; x < v.x + v.width; ++x) {
            int wx = x * 4 + 2 - ax, wy = y * 4 + 2 - ay;
            int dx = wx, dy = wy;
            if (len2 > 0) {
                int num = wx * vx + wy * vy;
                if (num < 0) num = 0;
                if (num > len2) num = len2;
                dx = wx - vx * num / len2;
                dy = wy - vy * num / len2;
            }
            int d = (int)zsr_isqrt32((uint32_t)(dx * dx + dy * dy));
            int cov = (half + 2 - d) * 64;
            if (cov <= 0) continue;
            if (cov > 255) cov = 255;
            row[x] = zsr_px_over(row[x], color, ZSR_DIV255(a0 * cov));
        }
    }
}

/* Ring (outer square box R, thickness t) with a gap of +-gap px centred at the top */
static void zsr_ring_gap_clipped(zsr_surface_t *s, zsr_rect_t R, int t, int gap,
                                 zsr_rect_t clip, zsr_color_t color)
{
    int a0 = ZSR_ALPHA(color);
    zsr_rect_t v = zsr_aa_visible(s, R, clip);
    if (!a0 || zsr_rect_empty(v)) return;
    int cx4 = R.x * 4 + R.width * 2, cy4 = R.y * 4 + R.height * 2;
    int ro = R.width * 2, ri = ro - t * 4, stride = s->pitch / 4;
    for (int y = v.y; y < v.y + v.height; ++y) {
        uint32_t *row = s->pixels + (size_t)y * stride;
        for (int x = v.x; x < v.x + v.width; ++x) {
            int dx = x * 4 + 2 - cx4, dy = y * 4 + 2 - cy4;
            int d = (int)zsr_isqrt32((uint32_t)(dx * dx + dy * dy));
            int co = (ro + 2 - d) * 64, ci = (d - ri + 2) * 64;
            int cov = co < ci ? co : ci;
            if (dy < 0) {
                int adx = dx < 0 ? -dx : dx;
                int g = (adx - gap * 4 + 2) * 64;
                if (g < cov) cov = g;
            }
            if (cov <= 0) continue;
            if (cov > 255) cov = 255;
            row[x] = zsr_px_over(row[x], color, ZSR_DIV255(a0 * cov));
        }
    }
}

/* Soft shadow of rounded rect R (shifted down by dy), painted only outside R */
static void zsr_shadow_clipped(zsr_surface_t *s, zsr_rect_t R, int r, int blur, int dy,
                               zsr_rect_t clip, zsr_color_t color)
{
    int peak = ZSR_ALPHA(color);
    if (blur < 2 || blur > 64) return;
    zsr_rect_t bb = zsr_rect_make(R.x - blur / 2 - 1, R.y + dy - blur / 2 - 1,
                                  R.width + blur + 2, R.height + blur + 2);
    zsr_rect_t v = zsr_aa_visible(s, bb, clip);
    if (!peak || zsr_rect_empty(v)) return;
    r = zsr_clamp_radius(R, r);

    int lut[2 * 64 + 1];
    int span = 2 * blur;
    for (int i = 0; i <= span; ++i) {
        int u = span - i;                       /* 1 at the core, 0 at the tail */
        lut[i] = peak * u * u / (span * span);
    }
    int scx2 = 2 * R.x + R.width, scy2 = 2 * (R.y + dy) + R.height;
    int hw2 = R.width, hh2 = R.height, r2 = 2 * r;
    int stride = s->pitch / 4, right = R.x + R.width;

    for (int y = v.y; y < v.y + v.height; ++y) {
        uint32_t *row = s->pixels + (size_t)y * stride;
        int xa = v.x, xb = v.x + v.width;
        int segs[2][2], n = 0;
        if (y >= R.y + r && y < R.y + R.height - r) {
            segs[n][0] = xa; segs[n][1] = xb < R.x ? xb : R.x; n++;
            segs[n][0] = xa > right ? xa : right; segs[n][1] = xb; n++;
        } else if (y >= R.y && y < R.y + R.height) {
            segs[n][0] = xa; segs[n][1] = xb < R.x + r ? xb : R.x + r; n++;
            segs[n][0] = xa > right - r ? xa : right - r; segs[n][1] = xb; n++;
        } else {
            segs[n][0] = xa; segs[n][1] = xb; n++;
        }
        int cy2 = 2 * y + 1;
        int qy = (cy2 > scy2 ? cy2 - scy2 : scy2 - cy2) - (hh2 - r2);
        for (int k = 0; k < n; ++k) {
            for (int x = segs[k][0]; x < segs[k][1]; ++x) {
                int cx2 = 2 * x + 1;
                int qx = (cx2 > scx2 ? cx2 - scx2 : scx2 - cx2) - (hw2 - r2);
                int outer, inner;
                if (qx > 0 && qy > 0) {
                    outer = (int)zsr_isqrt32((uint32_t)(qx * qx + qy * qy));
                    inner = 0;
                } else {
                    outer = (qx > 0 ? qx : 0) + (qy > 0 ? qy : 0);
                    inner = qx > qy ? qx : qy;
                    if (inner > 0) inner = 0;
                }
                int idx = outer + inner - r2 + blur;
                if (idx >= span) continue;
                if (idx < 0) idx = 0;
                int a = lut[idx];
                if (a <= 0) continue;
                int cov = zsr_rr_cov(x, y, R, r);
                if (cov >= 255) continue;
                if (cov > 0) a = a * (255 - cov) / 255;
                row[x] = zsr_px_over(row[x], color, a);
            }
        }
    }
}

static int zsr_ttf_width(const char *t)
{
    int w = 0;
    for (; *t; ++t) {
        int gw, gh, xo, yo, adv;
        fb_ttf_glyph(*t, &gw, &gh, &xo, &yo, &adv);
        w += adv;
    }
    return w;
}

static void zsr_ttf_draw_clipped(zsr_surface_t *s, int x, int y, const char *t,
                                 zsr_rect_t clip, zsr_color_t color)
{
    int ca = ZSR_ALPHA(color);
    zsr_rect_t c = zsr_rect_clip(clip, zsr_rect_make(0, 0, s->width, s->height));
    if (!ca || zsr_rect_empty(c)) return;
    int stride = s->pitch / 4, base = y + ZSR_TTF_SIZE, cx = x;
    for (; *t; ++t) {
        int gw, gh, xo, yo, adv;
        const uint8_t *bm = fb_ttf_glyph(*t, &gw, &gh, &xo, &yo, &adv);
        for (int j = 0; j < gh; ++j) {
            int py = base + yo + j;
            if (py < c.y || py >= c.y + c.height) continue;
            uint32_t *row = s->pixels + (size_t)py * stride;
            for (int i = 0; i < gw; ++i) {
                int px = cx + xo + i, a = bm[j * 512 + i];
                if (!a || px < c.x || px >= c.x + c.width) continue;
                row[px] = zsr_px_over(row[px], color, ZSR_DIV255(ca * a));
            }
        }
        cx += adv;
    }
}

static zsr_command_t *zsr_emit_op(zsr_command_list_t *list, zsr_opcode_t op, zsr_rect_t rect,
                                  int radius, zsr_rect_t clip, zsr_color_t color)
{
    zsr_command_t *c = zsr_command_push(list, op);
    if (!c) return 0;
    c->rect = rect;
    c->radius = radius;
    c->clip = clip;
    c->color = color;
    return c;
}

static void zsr_emit_aa_rrect(zsr_command_list_t *l, zsr_rect_t r, int radius, zsr_color_t color)
{
    zsr_emit_op(l, ZSR_CMD_AA_ROUNDED, r, radius, zsr_rect_make(0, 0, 32767, 32767), color);
}

static void zsr_emit_aa_ring(zsr_command_list_t *l, zsr_rect_t r, int radius, int t, zsr_color_t color)
{
    zsr_command_t *c = zsr_emit_op(l, ZSR_CMD_AA_RING, r, radius, zsr_rect_make(0, 0, 32767, 32767), color);
    if (c) c->thickness = t;
}

static void zsr_emit_aa_line(zsr_command_list_t *l, int x0, int y0, int x1, int y1, int t, zsr_color_t color)
{
    zsr_command_t *c = zsr_emit_op(l, ZSR_CMD_AA_LINE, zsr_rect_make(0, 0, 0, 0), 0,
                                   zsr_rect_make(0, 0, 32767, 32767), color);
    if (!c) return;
    c->x0 = x0; c->y0 = y0; c->x1 = x1; c->y1 = y1; c->thickness = t;
}

static void zsr_emit_ttf(zsr_command_list_t *l, int x, int baseline, const char *text,
                         zsr_rect_t clip, zsr_color_t color)
{
    zsr_command_t *c = zsr_emit_op(l, ZSR_CMD_TTF_TEXT, zsr_rect_make(x, baseline - ZSR_TTF_SIZE, 0, 0), 0, clip, color);
    if (!c) return;
    uint32_t n = 0;
    while (text[n] && n < ZSR_MAX_TEXT - 1) ++n;
    zsr_copy_string(c->text, sizeof(c->text), text, n);
}

static int g_active_tab_index = 0;

/* The focused, visible, non-minimized app window (only while the shell is "open") */
static struct window *zsr_active_app(void)
{
    struct window *w = wm_get_focused();
    if (!g_window_open || !w || !w->visible || w->minimized) return 0;
    return w;
}

static bool zsr_overlay_hidden(void)
{
    struct window *w = zsr_active_app();
    return w && w->frameless;
}

static void zsr_content_origin(const struct window *w, zsr_rect_t frame, int *ox, int *oy)
{
    if (w->frameless) { *ox = frame.x; *oy = frame.y; }
    else { *ox = frame.x + ZSR_WIN_PAD; *oy = frame.y + ZSR_TITLEBAR_HEIGHT; }
}

/* Traffic-light centres: close, minimize, zoom */
static void zsr_ctl_center(zsr_rect_t frame, int i, int *cx, int *cy)
{
    *cx = frame.x + 24 + i * 22;
    *cy = frame.y + 22;
}

static bool zsr_controls_hover(zsr_rect_t frame)
{
    return g_mouse_x >= frame.x + 10 && g_mouse_x < frame.x + 84 &&
           g_mouse_y >= frame.y + 8 && g_mouse_y < frame.y + 36;
}

/* Run the app's on_draw straight into its content buffer (no render queue) */
static void zsr_render_app_into_buffer(struct window *app)
{
    if (!app->needs_redraw) return;
    app->needs_redraw = false;
    if (!app->content_buffer || !app->on_draw) return;
    render_queue_clear(&app->surface.queue);
    gapi_set_current_context(&app->surface.context);
    gapi_set_immediate(true);
    fb_set_base_target(app->content_buffer, app->width, app->height, app->width * 4);
    app->on_draw(app);
    fb_set_base_target(0, 0, 0, 0);
    gapi_set_immediate(false);
    gapi_set_current_context(NULL);
}

static void zsr_build_zarqaa_window(
    zsr_command_list_t *list,
    zsr_rect_t frame,
    bool is_dragging
)
{
    (void)is_dragging;
    zsr_rect_t screen_clip = zsr_rect_make(0, 0, 32767, 32767);
    struct window *app = zsr_active_app();
    if (!app) return;

    zsr_render_app_into_buffer(app);

    if (app->frameless) {
        if (app->content_buffer)
            zsr_emit_blit_buffer(list, zsr_rect_make(frame.x, frame.y, app->width, app->height),
                                 app->content_buffer, app->width * 4, frame);
        return;
    }

    /* Glass body (must stay the first command: the renderer folds it into the wallpaper) */
    zsr_emit_rounded_rect(list, frame, ZSR_CHROME_RADIUS, screen_clip, ZSR_RGBA(12, 30, 72, 178));

    zsr_command_t *sh = zsr_emit_op(list, ZSR_CMD_SHADOW, frame, ZSR_CHROME_RADIUS, screen_clip,
                                    ZSR_RGBA(0, 6, 24, 150));
    if (sh) sh->thickness = ZSR_SHADOW_BLUR, sh->y0 = ZSR_SHADOW_DY;

    zsr_emit_aa_ring(list, frame, ZSR_CHROME_RADIUS, 1, ZSR_RGBA(150, 200, 255, 95));

    /* Traffic lights */
    static const zsr_color_t fill[3] = {
        ZSR_RGBA(255, 95, 87, 255), ZSR_RGBA(254, 188, 46, 255), ZSR_RGBA(40, 200, 64, 255)
    };
    static const zsr_color_t glyph[3] = {
        ZSR_RGBA(90, 14, 10, 230), ZSR_RGBA(100, 62, 0, 230), ZSR_RGBA(8, 70, 18, 230)
    };
    bool hover = zsr_controls_hover(frame);
    for (int i = 0; i < 3; ++i) {
        int cx, cy;
        zsr_ctl_center(frame, i, &cx, &cy);
        zsr_rect_t dot = zsr_rect_make(cx - 7, cy - 7, 15, 15);
        zsr_emit_aa_rrect(list, dot, 7, fill[i]);
        zsr_emit_aa_ring(list, dot, 7, 1, ZSR_RGBA(0, 0, 0, 55));
        if (!hover) continue;
        if (i == 0) {
            zsr_emit_aa_line(list, cx - 3, cy - 3, cx + 3, cy + 3, 1, glyph[i]);
            zsr_emit_aa_line(list, cx - 3, cy + 3, cx + 3, cy - 3, 1, glyph[i]);
        } else if (i == 1) {
            zsr_emit_aa_line(list, cx - 3, cy, cx + 3, cy, 1, glyph[i]);
        } else {
            zsr_emit_aa_line(list, cx - 3, cy, cx + 3, cy, 1, glyph[i]);
            zsr_emit_aa_line(list, cx, cy - 3, cx, cy + 3, 1, glyph[i]);
        }
    }

    /* Centered title */
    int tw = zsr_ttf_width(app->title);
    int tx = frame.x + (frame.width - tw) / 2;
    if (tx < frame.x + 96) tx = frame.x + 96;
    zsr_emit_ttf(list, tx, frame.y + 27, app->title,
                 zsr_rect_make(frame.x + 96, frame.y, frame.width - 112, ZSR_TITLEBAR_HEIGHT),
                 ZSR_RGBA(234, 242, 255, 235));

    /* App content, 1px hairline around it */
    if (app->content_buffer) {
        int ox, oy;
        zsr_content_origin(app, frame, &ox, &oy);
        zsr_rect_t cr = zsr_rect_make(ox, oy, app->width, app->height);
        zsr_emit_border(list, zsr_rect_make(ox - 1, oy - 1, app->width + 2, app->height + 2),
                        0, 1, screen_clip, ZSR_RGBA(255, 255, 255, 28));
        zsr_emit_blit_buffer(list, cr, app->content_buffer, app->width * 4, cr);
    }
}

/* ------------------------------------------------------------
   Top bar
   ------------------------------------------------------------ */
static const char *g_month_names[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char *g_day_names[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static void zsr_clock_text(char *out, int cap)
{
    uint64_t now = zb_now();
    int hh = (int)((now / 3600) % 24), mm = (int)((now / 60) % 60);
    int y, m, d;
    zb_civil((int64_t)(now / 86400), &y, &m, &d);
    snprintf(out, cap, "%s %d %s   %02d:%02d", g_day_names[zb_weekday(y, m, d)], d,
             g_month_names[(m - 1) % 12], hh, mm);
}

static void zsr_build_topbar(zsr_command_list_t *list, int sw)
{
    zsr_rect_t clip = zsr_rect_make(0, 0, 32767, 32767);
    zsr_command_t *bar = zsr_command_push(list, ZSR_CMD_FILL_RECT);
    if (bar) {
        bar->rect = zsr_rect_make(0, 0, sw, ZSR_TOPBAR_H);
        bar->clip = clip;
        bar->color = ZSR_RGBA(6, 20, 54, 165);
    }
    zsr_command_t *ln = zsr_command_push(list, ZSR_CMD_FILL_RECT);
    if (ln) {
        ln->rect = zsr_rect_make(0, ZSR_TOPBAR_H - 1, sw, 1);
        ln->clip = clip;
        ln->color = ZSR_RGBA(140, 195, 255, 38);
    }

    zsr_emit_aa_rrect(list, zsr_rect_make(14, 10, 10, 10), 5, ZSR_RGBA(108, 180, 255, 255));
    zsr_emit_ttf(list, 32, 20, "Zarqaa", clip, ZSR_RGBA(240, 246, 255, 245));

    struct window *app = zsr_active_app();
    if (app && !app->frameless)
        zsr_emit_ttf(list, 32 + zsr_ttf_width("Zarqaa") + 16, 20, app->title, clip, ZSR_RGBA(190, 212, 245, 190));

    char buf[40];
    zsr_clock_text(buf, sizeof(buf));
    zsr_emit_ttf(list, sw - 16 - zsr_ttf_width(buf), 20, buf, clip, ZSR_RGBA(226, 238, 255, 235));
}

static uint64_t g_topbar_minute = ~0ull;

static void zsr_tick_topbar(zsr_compositor_t *compositor)
{
    uint64_t minute = zb_now() / 60;
    if (minute == g_topbar_minute) return;
    g_topbar_minute = minute;
    zsr_damage_add(&compositor->damage, zsr_rect_make(0, 0, g_screen_w, ZSR_TOPBAR_H));
}

/* ------------------------------------------------------------
   Keep the window frame in sync with the hosted app
   ------------------------------------------------------------ */
static int g_sync_id = -1, g_sync_w, g_sync_h;
static bool g_force_center;

/* Remember where each window was left so switching apps does not recenter it */
#define ZSR_POS_SLOTS 16
static struct { int id, x, y; } g_win_pos[ZSR_POS_SLOTS];
static int g_win_pos_next;

static void zsr_pos_store(int id, int x, int y)
{
    for (int i = 0; i < ZSR_POS_SLOTS; ++i)
        if (g_win_pos[i].id == id) { g_win_pos[i].x = x; g_win_pos[i].y = y; return; }
    g_win_pos[g_win_pos_next].id = id;
    g_win_pos[g_win_pos_next].x = x;
    g_win_pos[g_win_pos_next].y = y;
    g_win_pos_next = (g_win_pos_next + 1) % ZSR_POS_SLOTS;
}

static bool zsr_pos_load(int id, int *x, int *y)
{
    for (int i = 0; i < ZSR_POS_SLOTS; ++i)
        if (g_win_pos[i].id == id) { *x = g_win_pos[i].x; *y = g_win_pos[i].y; return true; }
    return false;
}

static void zsr_sync_window_to_app(zsr_compositor_t *c)
{
    struct window *app = zsr_active_app();
    if (!app) {
        if (g_sync_id != -1) {
            g_sync_id = -1;
            zsr_request_full_redraw(c);
        }
        g_window_open = false;
        return;
    }
    zsr_rect_t *b = &c->main_window.bounds;
    if (g_sync_id != -1 && !app->frameless) zsr_pos_store(g_sync_id, b->x, b->y);
    int fw, fh;
    if (app->frameless) { fw = g_screen_w; fh = g_screen_h; }
    else { fw = app->width + 2 * ZSR_WIN_PAD; fh = ZSR_TITLEBAR_HEIGHT + app->height + ZSR_WIN_PAD; }

    bool fresh = app->id != g_sync_id;
    if (!fresh && !g_force_center && g_sync_w == fw && g_sync_h == fh) return;

    b->width = fw;
    b->height = fh;
    if (fresh || g_force_center || app->frameless) {
        int dock_top = g_screen_h - 70 - 15;
        int usable = dock_top - 10 - ZSR_TOPBAR_H;
        b->x = (g_screen_w - fw) / 2;
        b->y = ZSR_TOPBAR_H + (usable - fh) / 2;
        if (b->y < ZSR_TOPBAR_H + 4) b->y = ZSR_TOPBAR_H + 4;
        int px, py;
        if (fresh && !g_force_center && !app->frameless && zsr_pos_load(app->id, &px, &py)) {
            b->x = px;
            b->y = py;
        }
        if (app->frameless) { b->x = 0; b->y = 0; }
    }
    g_force_center = false;
    g_sync_id = app->id;
    g_sync_w = fw;
    g_sync_h = fh;
    c->main_window.previous_bounds = *b;
    zsr_request_full_redraw(c);
}


/* ============================================================
   11) SCENE TREE LAYOUT & COMMAND TRANSLATION
   ============================================================ */

static void zsr_layout_node(zsr_node_t *node, zsr_rect_t parent_bounds)
{
    if (!node || !node->visible)
        return;

    const char *x_str = zsr_attr(node, "x");
    const char *y_str = zsr_attr(node, "y");
    const char *w_str = zsr_attr(node, "width");
    const char *h_str = zsr_attr(node, "height");

    int x = zsr_parse_int(x_str, parent_bounds.x);
    int y = zsr_parse_int(y_str, parent_bounds.y);
    int w = zsr_parse_int(w_str, parent_bounds.width);
    int h = zsr_parse_int(h_str, parent_bounds.height);

    if (node->type == ZSR_NODE_WINDOW) {
        if (!x_str) x = (parent_bounds.width - w) / 2;
        if (!y_str) y = (parent_bounds.height - h) / 2;
    }

    node->old_bounds = node->bounds;
    node->bounds = zsr_rect_make(x, y, w, h);
    node->needs_layout = false;

    for (zsr_node_t *child = node->first_child; child; child = child->next_sibling) {
        zsr_layout_node(child, node->bounds);
    }
}

static void zsr_build_from_node(zsr_node_t *node, zsr_command_list_t *list)
{
    if (!node || !node->visible)
        return;

    zsr_rect_t screen_clip = zsr_rect_make(0, 0, 32767, 32767);

    switch (node->type) {
    case ZSR_NODE_WINDOW:
        zsr_build_zarqaa_window(list, node->bounds, false);
        break;

    case ZSR_NODE_APP_SURFACE: {
        const char *ptr_str = zsr_attr(node, "buffer");
        const char *pitch_str = zsr_attr(node, "pitch");
        if (ptr_str) {
            uint32_t *buffer = (uint32_t *)zsr_parse_uintptr(ptr_str, 0);
            int pitch = zsr_parse_int(pitch_str, node->bounds.width * 4);

            zsr_rect_t content_clip = node->bounds;
            if (node->parent && node->parent->type == ZSR_NODE_WINDOW) {
                content_clip = zsr_rect_make(
                    node->parent->bounds.x + 1,
                    node->parent->bounds.y + ZSR_TITLEBAR_HEIGHT,
                    node->parent->bounds.width - 2,
                    node->parent->bounds.height - ZSR_TITLEBAR_HEIGHT - 35
                );
            }
            content_clip = zsr_rect_clip(content_clip, screen_clip);
            zsr_emit_blit_buffer(list, node->bounds, buffer, pitch, content_clip);
        }
        break;
    }

    default:
        break;
    }

    for (zsr_node_t *child = node->first_child; child; child = child->next_sibling) {
        if (node->type != ZSR_NODE_WINDOW) {
            zsr_build_from_node(child, list);
        }
    }
}

/* ============================================================
   12) WINDOW INTERACTION & DRAG
   ============================================================ */


static void zsr_draw_mouse_cursor(zsr_surface_t *surface)
{
    if (g_mouse_x < 0 || g_mouse_y < 0) return;
    
    zsr_rect_t clip = zsr_rect_make(0, 0, surface->width, surface->height);
    
    // Draw an aesthetic arrow cursor pointing top-left
    zsr_fill_triangle_clipped(surface, 
        (zsr_point_t){g_mouse_x, g_mouse_y}, 
        (zsr_point_t){g_mouse_x + 13, g_mouse_y + 13}, 
        (zsr_point_t){g_mouse_x, g_mouse_y + 18}, 
        clip, ZSR_RGBA(0, 0, 0, 200));
        
    zsr_fill_triangle_clipped(surface, 
        (zsr_point_t){g_mouse_x + 1, g_mouse_y + 2}, 
        (zsr_point_t){g_mouse_x + 10, g_mouse_y + 11}, 
        (zsr_point_t){g_mouse_x + 1, g_mouse_y + 15}, 
        clip, ZSR_RGBA(255, 255, 255, 255));
}

/* ------------------------------------------------------------
   Desktop Dock UI
   ------------------------------------------------------------ */
#include "gui/bitmap_icons.h"

#include "desktop/desktop.h"
#include "gui/new_app_icons.h"

struct zsr_hud_button {
    const uint32_t *bitmap;
    int bw, bh;
    char name[24];
    char desc[24];
    int app_id;
};

#define ICON_DOCK_SIZE 40

static struct zsr_hud_button g_zsr_dock_buttons[] = {
    {icon_dock_browser,     ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Browser",       "Web Engine",          APP_HTML_VIEWER},
    {icon_dock_terminal,    ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Terminal",      "Command Line",        APP_TERMINAL},
    {icon_dock_studio,      ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Studio",        "Zarqaa Studio",       APP_PAINT},
    {icon_dock_notepad,     ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Notepad+",      "Text Editor",         APP_TEXT_EDITOR},
    {icon_dock_calculator,  ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Calculator",    "Math Tool",           APP_CALCULATOR},
    {icon_dock_gallery,     ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Gallery",       "Image Viewer",        APP_IMAGE_VIEWER},
    {new_icon_dock[NEWICON_CLOCK],   ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Clock",    "Time & Timer",        APP_CLOCK},
    {new_icon_dock[NEWICON_2048],    ICON_DOCK_SIZE, ICON_DOCK_SIZE, "2048",     "Puzzle Game",         APP_GAME2048},
    {new_icon_dock[NEWICON_SYSINFO], ICON_DOCK_SIZE, ICON_DOCK_SIZE, "About",    "System Info",         APP_SYSINFO},
    {icon_dock_power,       ICON_DOCK_SIZE, ICON_DOCK_SIZE, "Power",         "Management",          APP_POWER},
};
#define ZSR_NUM_DOCK 10

extern bool desktop_is_app_running(int app_id, bool *out_minimized);
extern void desktop_launch_app(int app_id);

static void zsr_draw_bitmap_rgba_centered(zsr_surface_t *surface, int cx, int cy, const uint32_t *bitmap, int w, int h)
{
    int start_x = cx - w / 2;
    int start_y = cy - h / 2;
    
    for (int y = 0; y < h; ++y) {
        int sy = start_y + y;
        if (sy < 0 || sy >= surface->height) continue;
        int dst_row = sy * (surface->pitch / 4);
        
        for (int x = 0; x < w; ++x) {
            int sx = start_x + x;
            if (sx < 0 || sx >= surface->width) continue;
            
            zsr_color_t color = bitmap[y * w + x];
            if ((color & 0xFF000000) == 0) continue;
            
            if ((color & 0xFF000000) == 0xFF000000) {
                surface->pixels[dst_row + sx] = color;
            } else {
                zsr_color_t bg = surface->pixels[dst_row + sx];
                uint8_t a = (color >> 24) & 0xFF;
                uint8_t inv_a = 255 - a;
                
                uint8_t r = (((color >> 16) & 0xFF) * a + ((bg >> 16) & 0xFF) * inv_a) >> 8;
                uint8_t g = (((color >> 8) & 0xFF) * a + ((bg >> 8) & 0xFF) * inv_a) >> 8;
                uint8_t b = ((color & 0xFF) * a + (bg & 0xFF) * inv_a) >> 8;
                
                surface->pixels[dst_row + sx] = (0xFF000000 | (r << 16) | (g << 8) | b);
            }
        }
    }
}

static int g_drag_dock_index = -1;
static int g_drag_start_x = 0;
static bool g_is_dock_dragging = false;

static void zsr_build_desktop_dock(zsr_command_list_t *list, int screen_width, int screen_height)
{
    int num_items = ZSR_NUM_DOCK;
    int spacing = 65;
    int dock_w = num_items * spacing + 40;
    int dock_h = 70;
    int dock_x = (screen_width - dock_w) / 2;
    int dock_y = screen_height - dock_h - 15;

    zsr_rect_t dock_rect = zsr_rect_make(dock_x, dock_y, dock_w, dock_h);
    zsr_rect_t clip = zsr_rect_make(0, 0, 32767, 32767);

    zsr_command_t *sh = zsr_emit_op(list, ZSR_CMD_SHADOW, dock_rect, 22, clip, ZSR_RGBA(0, 6, 24, 120));
    if (sh) sh->thickness = 18, sh->y0 = 6;
    zsr_emit_aa_rrect(list, dock_rect, 22, ZSR_RGBA(8, 24, 62, 205));
    zsr_emit_aa_ring(list, dock_rect, 22, 1, ZSR_RGBA(130, 185, 255, 80));

    int start_x = dock_x + 20 + spacing / 2;
    int dock_cy = dock_y + dock_h / 2;

    for (int i = 0; i < ZSR_NUM_DOCK; ++i) {
        int cx = start_x + i * spacing;
        bool hover = (g_mouse_x >= cx - 24 && g_mouse_x <= cx + 24 && g_mouse_y >= dock_y && g_mouse_y <= dock_y + dock_h);
        int draw_cx = cx;
        int draw_cy = dock_cy;
        bool dragged = g_is_dock_dragging && i == g_drag_dock_index;

        if (dragged) {
            draw_cx = g_mouse_x;
        } else if (hover && !g_is_dock_dragging) {
            draw_cy -= 5;
            zsr_emit_aa_rrect(list, zsr_rect_make(cx - 29, dock_y + 7, 58, 56), 15, ZSR_RGBA(160, 205, 255, 34));
        }

        if (g_zsr_dock_buttons[i].app_id == APP_POWER) {
            /* Vector power icon on a rounded tile, matching the bitmap icons */
            zsr_rect_t tile = zsr_rect_make(draw_cx - 20, draw_cy - 20, 40, 40);
            zsr_emit_aa_rrect(list, tile, 10, ZSR_RGBA(214, 62, 78, 255));
            zsr_emit_aa_ring(list, tile, 10, 1, ZSR_RGBA(255, 255, 255, 45));
            zsr_command_t *rg = zsr_emit_op(list, ZSR_CMD_RING_GAP,
                                            zsr_rect_make(draw_cx - 11, draw_cy - 10, 22, 22), 0, clip,
                                            ZSR_RGBA(255, 255, 255, 255));
            if (rg) rg->thickness = 3, rg->x0 = 4;
            zsr_emit_aa_line(list, draw_cx, draw_cy - 12, draw_cx, draw_cy - 2, 3, ZSR_RGBA(255, 255, 255, 255));
        } else if (g_zsr_dock_buttons[i].bitmap) {
            int bw = g_zsr_dock_buttons[i].bw;
            int bh = g_zsr_dock_buttons[i].bh;
            zsr_rect_t icon_rect = zsr_rect_make(draw_cx - bw / 2, draw_cy - bh / 2, bw, bh);
            zsr_emit_blit_buffer(list, icon_rect, (uint32_t *)g_zsr_dock_buttons[i].bitmap, bw * 4, clip);
        }

        bool is_minimized = false;
        if (desktop_is_app_running(g_zsr_dock_buttons[i].app_id, &is_minimized)) {
            zsr_color_t dot = is_minimized ? ZSR_RGBA(108, 180, 255, 200) : ZSR_RGBA(61, 220, 151, 255);
            zsr_emit_aa_rrect(list, zsr_rect_make(cx - 2, dock_y + dock_h - 10, 5, 5), 2, dot);
        }
    }

    /* Tooltip pill above the hovered icon, drawn last so nothing covers it */
    for (int i = 0; i < ZSR_NUM_DOCK && !g_is_dock_dragging; ++i) {
        int cx = start_x + i * spacing;
        if (!(g_mouse_x >= cx - 24 && g_mouse_x <= cx + 24 && g_mouse_y >= dock_y && g_mouse_y <= dock_y + dock_h))
            continue;
        const char *name = g_zsr_dock_buttons[i].name;
        int tw = zsr_ttf_width(name);
        int pw = tw + 24, ph = 26;
        int px = cx - pw / 2;
        if (px < 8) px = 8;
        if (px + pw > screen_width - 8) px = screen_width - 8 - pw;
        zsr_rect_t pill = zsr_rect_make(px, dock_y - 38, pw, ph);
        zsr_command_t *ps = zsr_emit_op(list, ZSR_CMD_SHADOW, pill, 13, clip, ZSR_RGBA(0, 6, 24, 120));
        if (ps) ps->thickness = 12, ps->y0 = 3;
        zsr_emit_aa_rrect(list, pill, 13, ZSR_RGBA(8, 24, 62, 238));
        zsr_emit_aa_ring(list, pill, 13, 1, ZSR_RGBA(130, 185, 255, 90));
        zsr_emit_ttf(list, px + 12, pill.y + 18, name, clip, ZSR_RGBA(240, 246, 255, 255));
    }
}

static void zsr_window_init(
    zsr_window_state_t *window,
    int screen_width,
    int screen_height
)
{
    window->bounds = zsr_rect_make(
        (screen_width - 900) / 2,
        (screen_height - 600) / 2,
        900,
        600
    );

    window->previous_bounds = window->bounds;

    window->dragging = false;
    window->resize_right = false;
    window->resize_bottom = false;

    window->drag_offset_x = 0;
    window->drag_offset_y = 0;

    window->start_mouse_x = 0;
    window->start_mouse_y = 0;
    window->start_width = 0;
    window->start_height = 0;
}

static void zsr_window_mouse_down(
    zsr_window_state_t *window,
    int mouse_x,
    int mouse_y
)
{
    if (!zsr_point_in_rect(mouse_x, mouse_y, window->bounds))
        return;

    struct window *app = zsr_active_app();
    if (app && app->frameless)
        return;

    zsr_rect_t r = window->bounds;
    window->previous_bounds = window->bounds;
    window->start_mouse_x = mouse_x;
    window->start_mouse_y = mouse_y;
    window->start_width = r.width;
    window->start_height = r.height;
    window->resize_right = false;
    window->resize_bottom = false;

    if (mouse_y >= r.y && mouse_y < r.y + ZSR_TITLEBAR_HEIGHT) {
        window->dragging = true;
        window->drag_offset_x = mouse_x - r.x;
        window->drag_offset_y = mouse_y - r.y;
    }
}

static void zsr_damage_add_no_merge(zsr_damage_t *damage, zsr_rect_t rect)
{
    if (zsr_rect_empty(rect) || damage->full_redraw)
        return;

    if (damage->count < ZSR_MAX_DAMAGE) {
        damage->rects[damage->count++] = rect;
    } else {
        damage->full_redraw = true;
    }
}

static void zsr_window_mouse_move(
    zsr_window_state_t *window,
    int mouse_x,
    int mouse_y,
    zsr_damage_t *damage
)
{
    if (!window->dragging &&
        !window->resize_right &&
        !window->resize_bottom) {
        return;
    }

    window->previous_bounds = window->bounds;

    if (window->dragging) {
        window->bounds.x = mouse_x - window->drag_offset_x;
        window->bounds.y = mouse_y - window->drag_offset_y;
        if (window->bounds.y < ZSR_TOPBAR_H) window->bounds.y = ZSR_TOPBAR_H;
        if (window->bounds.y > g_screen_h - 60) window->bounds.y = g_screen_h - 60;
        if (window->bounds.x < 80 - window->bounds.width) window->bounds.x = 80 - window->bounds.width;
        if (window->bounds.x > g_screen_w - 80) window->bounds.x = g_screen_w - 80;
    }

    if (window->resize_right) {
        int new_width = window->start_width + (mouse_x - window->start_mouse_x);
        if (new_width < ZSR_MIN_WINDOW_WIDTH)
            new_width = ZSR_MIN_WINDOW_WIDTH;
        window->bounds.width = new_width;
    }

    if (window->resize_bottom) {
        int new_height = window->start_height + (mouse_y - window->start_mouse_y);
        if (new_height < ZSR_MIN_WINDOW_HEIGHT)
            new_height = ZSR_MIN_WINDOW_HEIGHT;
        window->bounds.height = new_height;
    }

    if (window->resize_right || window->resize_bottom) {
        damage->full_redraw = true;
        return;
    }

    /* Pure move: repaint only the old and new footprint (+margin for the 16px shadow) */
    const int m = 40;
    zsr_rect_t old_r = window->previous_bounds;
    zsr_rect_t new_r = window->bounds;
    zsr_damage_add(damage, zsr_rect_make(old_r.x - m, old_r.y - m, old_r.width + 2 * m, old_r.height + 2 * m));
    zsr_damage_add(damage, zsr_rect_make(new_r.x - m, new_r.y - m, new_r.width + 2 * m, new_r.height + 2 * m));
}

static void zsr_window_mouse_up(zsr_window_state_t *window)
{
    window->dragging = false;
    window->resize_right = false;
    window->resize_bottom = false;
}

/* ============================================================
   13) COMPOSITOR ENGINE
   ============================================================ */

static void zsr_compositor_init(
    zsr_compositor_t *compositor,
    zsr_surface_t *backbuffer
)
{
    compositor->backbuffer = backbuffer;
    g_screen_w = backbuffer->width;
    g_screen_h = backbuffer->height;
    new_app_icons_init();
    zsr_pre_render_wallpaper(backbuffer->width, backbuffer->height);

    zsr_window_init(
        &compositor->main_window,
        backbuffer->width,
        backbuffer->height
    );

    zsr_damage_reset(&compositor->damage);
    zsr_damage_add(
        &compositor->damage,
        zsr_rect_make(0, 0, backbuffer->width, backbuffer->height)
    );

    compositor->root = 0;
    compositor->document.node_count = 0;
}


void zsr_open_window(zsr_compositor_t *compositor) {
    g_window_open = true;
    if (compositor) zsr_request_full_redraw(compositor);
}

void zsr_close_window(zsr_compositor_t *compositor) {
    g_window_open = false;
    if (compositor) zsr_request_full_redraw(compositor);
}

static void zsr_compositor_build_commands(zsr_compositor_t *compositor)
{
    zsr_commands_reset(&compositor->commands);

    if (compositor->root) {
        zsr_layout_node(
            compositor->root,
            zsr_rect_make(0, 0, compositor->backbuffer->width, compositor->backbuffer->height)
        );

        for (zsr_node_t *child = compositor->root->first_child; child; child = child->next_sibling) {
            if (child->type == ZSR_NODE_WINDOW) {
                compositor->main_window.bounds = child->bounds;
                break;
            }
        }

        zsr_build_from_node(compositor->root, &compositor->commands);
    } else if (g_window_open) {
        zsr_build_zarqaa_window(&compositor->commands, compositor->main_window.bounds, compositor->main_window.dragging);
    }
    
    if (!zsr_overlay_hidden()) {
        zsr_build_topbar(&compositor->commands, compositor->backbuffer->width);
        // Build the dock on top of everything!
        zsr_build_desktop_dock(&compositor->commands, compositor->backbuffer->width, compositor->backbuffer->height);
    }
}

static void zsr_compositor_render_region(
    zsr_compositor_t *compositor,
    zsr_rect_t region
)
{
    zsr_surface_t *surface = compositor->backbuffer;

    zsr_rect_t screen = zsr_rect_make(0, 0, surface->width, surface->height);
    region = zsr_rect_clip(region, screen);

    if (zsr_rect_empty(region))
        return;

    const zsr_command_list_t *list = &compositor->commands;
    uint32_t first = 0;

    if (list->count > 0 && g_wallpaper_cache &&
        g_wallpaper_cache_width == surface->width &&
        g_wallpaper_cache_height == surface->height) {
        const zsr_command_t *c0 = &list->commands[0];
        bool fill = c0->opcode == ZSR_CMD_FILL_RECT || c0->opcode == ZSR_CMD_ROUNDED_RECT;
        if (fill && ZSR_ALPHA(c0->color) != 0) {
            const uint32_t *tint = zsr_tinted_wallpaper(c0->color, surface->width, surface->height);
            if (tint) {
                zsr_draw_background_folded(surface, region, c0, g_wallpaper_cache, tint);
                first = 1;
            }
        }
    }

    if (first == 0)
        zsr_draw_zarqaa_background_region(surface, region);

    for (uint32_t i = first; i < list->count; ++i)
        zsr_execute_command(surface, &list->commands[i], region);
}

/* Drive the hosted app's on_tick and turn its redraw requests into damage */
static void zsr_tick_active_app(zsr_compositor_t *compositor)
{
    zsr_sync_window_to_app(compositor);
    zsr_tick_topbar(compositor);

    struct window *win = zsr_active_app();
    if (!win) return;

    if (win->on_tick) win->on_tick(win);
    if (win->needs_redraw) {
        int ox, oy;
        zsr_content_origin(win, compositor->main_window.bounds, &ox, &oy);
        zsr_damage_add(&compositor->damage, zsr_rect_make(ox, oy, win->width, win->height));
    }
}

static void zsr_compositor_frame(zsr_compositor_t *compositor)
{
    zsr_damage_t *damage = &compositor->damage;
    zsr_surface_t *surface = compositor->backbuffer;

    zsr_tick_active_app(compositor);
    
    // Dynamic backbuffer
    surface->pixels = fb_get_backbuffer();

    if (damage->count == 0 && !damage->full_redraw)
        return;

    zsr_compositor_build_commands(compositor);

    if (damage->full_redraw) {
        zsr_compositor_render_region(
            compositor,
            zsr_rect_make(0, 0, surface->width, surface->height)
        );
        dirty_rect_add(0, 0, surface->width, surface->height);
    } else {
        for (uint32_t i = 0; i < damage->count; ++i) {
            zsr_compositor_render_region(
                compositor,
                damage->rects[i]
            );
            dirty_rect_add(damage->rects[i].x, damage->rects[i].y, damage->rects[i].width, damage->rects[i].height);
        }
    }

    /* Draw mouse cursor over everything */
    zsr_draw_mouse_cursor(surface);
    /* Ensure the mouse area is added to dirty rects */
    if (g_mouse_x != -1) {
        dirty_rect_add(g_old_mouse_rect.x, g_old_mouse_rect.y, g_old_mouse_rect.width, g_old_mouse_rect.height);
        dirty_rect_add(g_mouse_x, g_mouse_y, 16, 24);
    }

    zsr_present(surface);
    zsr_damage_reset(damage);
}

/* ============================================================
   14) PUBLIC API IMPLEMENTATION
   ============================================================ */

static zsr_compositor_t g_zarqaa_compositor;

zsr_compositor_t *zsr_get_desktop_compositor(void)
{
    return &g_zarqaa_compositor;
}

void zsr_init(
    zsr_compositor_t *compositor,
    zsr_surface_t *backbuffer
)
{
    zsr_compositor_init(compositor, backbuffer);
}

void zsr_render(zsr_compositor_t *compositor)
{
    zsr_compositor_frame(compositor);
}

static bool zsr_hit_test_controls(zsr_compositor_t *compositor, zsr_rect_t frame, int mx, int my)
{
    struct window *app = zsr_active_app();
    if (!app || app->frameless) return false;

    for (int i = 0; i < 3; ++i) {
        int cx, cy;
        zsr_ctl_center(frame, i, &cx, &cy);
        int dx = mx - cx, dy = my - cy;
        if (dx * dx + dy * dy > 10 * 10) continue;

        if (i == 0) {
            wm_destroy_window(app);
            wm_focus_top_visible();
        } else if (i == 1) {
            app->minimized = true;
            wm_focus_top_visible();
        } else {
            g_force_center = true;
        }
        if (compositor) zsr_request_full_redraw(compositor);
        return true;
    }
    return false;
}

static bool zsr_hit_test_tabs(zsr_rect_t frame, int mx, int my)
{
    if (!g_window_open) return false;

    struct window *active_win = wm_get_focused();
    if (active_win && active_win->visible && !active_win->minimized) return false;

    int tab_x = frame.x + 24;
    int tab_y = frame.y + 52;

    for (int i = 0; i < 4; ++i) {
        zsr_rect_t tab_rect = zsr_rect_make(tab_x, tab_y, 74, 28);
        if (zsr_point_in_rect(mx, my, tab_rect)) {
            if (g_active_tab_index != i) {
                g_active_tab_index = i;
                return true;
            }
            return true;
        }
        tab_x += 82;
    }
    return false;
}

void zsr_handle_mouse_down(
    zsr_compositor_t *compositor,
    int mouse_x,
    int mouse_y
)
{
    if (!compositor) return;

    if (g_mouse_x != mouse_x || g_mouse_y != mouse_y) {
        if (g_mouse_x != -1) {
            zsr_damage_add(&compositor->damage, g_old_mouse_rect);
        }
        g_mouse_x = mouse_x;
        g_mouse_y = mouse_y;
        zsr_rect_t new_mouse_rect = zsr_rect_make(mouse_x, mouse_y, 16, 24);
        g_old_mouse_rect = new_mouse_rect;
        zsr_damage_add(&compositor->damage, new_mouse_rect);
    }

    // Check if dock icon was pressed/dragged
    if (compositor->backbuffer && !zsr_overlay_hidden()) {
        int dock_h = 70;
        int dock_w = ZSR_NUM_DOCK * 65 + 40;
        int dock_y = compositor->backbuffer->height - dock_h - 15;
        int dock_x = (compositor->backbuffer->width - dock_w) / 2;
        
        if (mouse_y >= dock_y && mouse_y <= dock_y + dock_h) {
            int start_x = dock_x + 20 + 65 / 2;
            for (int i = 0; i < ZSR_NUM_DOCK; ++i) {
                int cx = start_x + i * 65;
                if (mouse_x >= cx - 28 && mouse_x <= cx + 28) {
                    g_drag_dock_index = i;
                    g_drag_start_x = mouse_x;
                    g_is_dock_dragging = false;
                    return; // Consumed event
                }
            }
        }
    }

    if (!g_window_open) return;

    zsr_rect_t frame = compositor->main_window.bounds;

    /* Hit test window controls */
    if (zsr_hit_test_controls(compositor, frame, mouse_x, mouse_y)) {
        return;
    }

    /* Hit test tabs */
    if (zsr_hit_test_tabs(frame, mouse_x, mouse_y)) {
        zsr_damage_add(&compositor->damage, frame);
        return;
    }

    /* Click on App card inside Apps tab */
    struct window *focused_win = wm_get_focused();
    if ((!focused_win || !focused_win->visible || focused_win->minimized) && g_active_tab_index == 1) {
        int ox = frame.x + 24;
        int oy = frame.y + 100;
        int cols = ZSR_APP_CARD_COLS;
        int card_w = (frame.width - 48 - (cols - 1) * 16) / cols;
        int card_h = 56;

        static const int app_ids[ZSR_NUM_APP_CARDS] = {
            APP_TERMINAL,     APP_CALCULATOR,   APP_TEXT_EDITOR,  APP_FILE_MANAGER,
            APP_PAINT,        APP_HTML_VIEWER,  APP_IMAGE_VIEWER, APP_TASK_MANAGER,
            APP_SNAKE,        APP_PYTHON_IDE,   APP_SETTINGS,     APP_DEVICE_MANAGER,
            APP_BENCHMARK,    APP_BOOT_LOGGER,  APP_CLOCK,        APP_CALENDAR,
            APP_SYSINFO,      APP_GAME2048,     APP_MINESWEEPER
        };

        for (int i = 0; i < ZSR_NUM_APP_CARDS; i++) {
            int col = i % cols;
            int row = i / cols;
            int cx = ox + col * (card_w + 16);
            int cy = oy + 36 + row * (card_h + 14);

            if (mouse_x >= cx && mouse_x <= cx + card_w &&
                mouse_y >= cy && mouse_y <= cy + card_h) {
                desktop_launch_app(app_ids[i]);
                zsr_request_full_redraw(compositor);
                return;
            }
        }
    }

    /* Click on File item inside Files tab */
    if ((!focused_win || !focused_win->visible || focused_win->minimized) && g_active_tab_index == 2) {
        int ox = frame.x + 24;
        int oy = frame.y + 100;
        for (int i = 0; i < 8; i++) {
            int row_y = oy + 32 + i * 26;
            if (mouse_x >= ox && mouse_x <= ox + frame.width - 48 &&
                mouse_y >= row_y && mouse_y <= row_y + 22) {
                if (i == 2) desktop_launch_app(APP_CALCULATOR);      // calc.elf
                else if (i == 4) desktop_launch_app(APP_SNAKE);      // snake.elf
                else if (i == 5) desktop_launch_app(APP_TERMINAL);   // sh.elf
                else if (i == 6) desktop_launch_app(APP_TEXT_EDITOR);// test.txt
                else desktop_launch_app(APP_FILE_MANAGER);           // directories/others
                zsr_request_full_redraw(compositor);
                return;
            }
        }
    }

    /* Click on Settings items inside Settings tab (Tab 3) */
    if ((!focused_win || !focused_win->visible || focused_win->minimized) && g_active_tab_index == 3) {
        int ox = frame.x + 24;
        int oy = frame.y + 100;
        if (mouse_x >= ox && mouse_x <= ox + frame.width - 48 &&
            mouse_y >= oy + 92 && mouse_y <= oy + 140) {
            fb_set_vsync(!fb_get_vsync());
            zsr_request_full_redraw(compositor);
            return;
        }
    }

    /* Drag / Resize */
    zsr_window_mouse_down(&compositor->main_window, mouse_x, mouse_y);

    /* Forward input to active app */
    struct window *active_win = wm_get_focused();
    if (active_win && active_win->visible && !active_win->minimized && !compositor->main_window.dragging) {
        int cox, coy;
            zsr_content_origin(active_win, frame, &cox, &coy);
            int app_mx = mouse_x - cox;
            int app_my = mouse_y - coy;
            if (app_mx >= 0 && app_mx < active_win->width && app_my >= 0 && app_my < active_win->height) {
            if (active_win->on_mouse) {
                active_win->on_mouse(active_win, app_mx, app_my, true, false); // left_down = true, right = false for now
                active_win->needs_redraw = true;
            }
        }
    }

    if (compositor->root) {
        for (zsr_node_t *child = compositor->root->first_child; child; child = child->next_sibling) {
            if (child->type == ZSR_NODE_WINDOW) {
                child->bounds = compositor->main_window.bounds;
                break;
            }
        }
    }
}

void zsr_handle_mouse_move(
    zsr_compositor_t *compositor,
    int mouse_x,
    int mouse_y
)
{
    if (!compositor) return;

    if (g_mouse_x != -1) {
        zsr_damage_add(&compositor->damage, g_old_mouse_rect);
    }
    
    g_mouse_x = mouse_x;
    g_mouse_y = mouse_y;
    
    zsr_rect_t new_mouse_rect = zsr_rect_make(mouse_x, mouse_y, 16, 24);
    g_old_mouse_rect = new_mouse_rect;
    zsr_damage_add(&compositor->damage, new_mouse_rect);

    {
        static bool ctl_hover = false;
        zsr_rect_t f = compositor->main_window.bounds;
        struct window *a = zsr_active_app();
        bool h = a && !a->frameless && zsr_controls_hover(f);
        if (h != ctl_hover) {
            ctl_hover = h;
            zsr_damage_add(&compositor->damage, zsr_rect_make(f.x, f.y, 110, ZSR_TITLEBAR_HEIGHT));
        }
    }

    // Handle Dock Icon Drag & Reordering
    if (g_drag_dock_index != -1 && compositor->backbuffer) {
        int dx = mouse_x - g_drag_start_x;
        if (!g_is_dock_dragging && (dx * dx > 25)) {
            g_is_dock_dragging = true;
        }
        if (g_is_dock_dragging) {
            int dock_h = 70;
            int dock_w = ZSR_NUM_DOCK * 65 + 40;
            int dock_y = compositor->backbuffer->height - dock_h - 15;
            int dock_x = (compositor->backbuffer->width - dock_w) / 2;
            int start_x = dock_x + 20 + 65 / 2;

            for (int i = 0; i < ZSR_NUM_DOCK; ++i) {
                if (i != g_drag_dock_index) {
                    int target_cx = start_x + i * 65;
                    if (mouse_x >= target_cx - 24 && mouse_x <= target_cx + 24) {
                        // Swap dock icons!
                        struct zsr_hud_button temp = g_zsr_dock_buttons[i];
                        g_zsr_dock_buttons[i] = g_zsr_dock_buttons[g_drag_dock_index];
                        g_zsr_dock_buttons[g_drag_dock_index] = temp;
                        g_drag_dock_index = i;
                        g_drag_start_x = mouse_x;
                        zsr_damage_add(&compositor->damage, zsr_rect_make(dock_x - 50, dock_y - 50, dock_w + 100, dock_h + 100));
                        break;
                    }
                }
            }
        }
    }

    // Damage dock area ONLY if the hovered item changes (to prevent lag)
    static int g_hovered_dock_index = -1;
    if (compositor->backbuffer) {
        int dock_h = 70;
        int dock_w = ZSR_NUM_DOCK * 65 + 40;
        int dock_y = compositor->backbuffer->height - dock_h - 15;
        int dock_x = (compositor->backbuffer->width - dock_w) / 2;
        
        int new_hover = -1;
        if (mouse_y >= dock_y && mouse_y <= dock_y + dock_h) {
            int start_x = dock_x + 20 + 65 / 2;
            for (int i = 0; i < ZSR_NUM_DOCK; ++i) {
                int cx = start_x + i * 65;
                if (mouse_x >= cx - 24 && mouse_x <= cx + 24) {
                    new_hover = i;
                    break;
                }
            }
        }
        
        if (new_hover != g_hovered_dock_index || g_is_dock_dragging) {
            g_hovered_dock_index = new_hover;
            // Hover changed or dragging, damage the dock area to trigger redraw
            zsr_damage_add(&compositor->damage, zsr_rect_make(dock_x - 50, dock_y - 50, dock_w + 100, dock_h + 100));
        }
    }

    if (g_window_open) {
        zsr_window_mouse_move(
            &compositor->main_window,
            mouse_x,
            mouse_y,
            &compositor->damage
        );

        /* Forward mouse move to active app */
        struct window *active_win = wm_get_focused();
        zsr_rect_t frame = compositor->main_window.bounds;
        if (active_win && active_win->visible && !active_win->minimized && !compositor->main_window.dragging) {
            int cox, coy;
            zsr_content_origin(active_win, frame, &cox, &coy);
            int app_mx = mouse_x - cox;
            int app_my = mouse_y - coy;
            if (app_mx >= 0 && app_mx < active_win->width && app_my >= 0 && app_my < active_win->height) {
                if (active_win->on_mouse) {
                    active_win->on_mouse(active_win, app_mx, app_my, false, false); // Not clicking, just moving
                }
            }
        }
    }

    if (compositor->root) {
        for (zsr_node_t *child = compositor->root->first_child; child; child = child->next_sibling) {
            if (child->type == ZSR_NODE_WINDOW) {
                child->bounds = compositor->main_window.bounds;
                break;
            }
        }
    }
}

void zsr_handle_mouse_up(zsr_compositor_t *compositor)
{
    if (!compositor) return;

    if (g_drag_dock_index != -1) {
        if (!g_is_dock_dragging) {
            // Clicked without drag -> Open app window!
            g_window_open = true;
            zsr_request_full_redraw(compositor);
            desktop_launch_app(g_zsr_dock_buttons[g_drag_dock_index].app_id);
        } else {
            // Drag finished -> Redraw dock area
            if (compositor->backbuffer) {
                int dock_h = 70;
                int dock_w = ZSR_NUM_DOCK * 65 + 40;
                int dock_y = compositor->backbuffer->height - dock_h - 15;
                int dock_x = (compositor->backbuffer->width - dock_w) / 2;
                zsr_damage_add(&compositor->damage, zsr_rect_make(dock_x - 50, dock_y - 50, dock_w + 100, dock_h + 100));
            }
        }
        g_drag_dock_index = -1;
        g_is_dock_dragging = false;
    }

    if (compositor->main_window.dragging || compositor->main_window.resize_right || compositor->main_window.resize_bottom) {
        zsr_request_full_redraw(compositor);
    }

    zsr_window_mouse_up(&compositor->main_window);

    /* Forward mouse up to active app */
    if (g_window_open) {
        struct window *active_win = wm_get_focused();
        zsr_rect_t frame = compositor->main_window.bounds;
        if (active_win && active_win->visible && !active_win->minimized) {
            int cox, coy;
            zsr_content_origin(active_win, frame, &cox, &coy);
            int app_mx = g_mouse_x - cox;
            int app_my = g_mouse_y - coy;
            if (app_mx >= 0 && app_mx < active_win->width && app_my >= 0 && app_my < active_win->height) {
                if (active_win->on_mouse) {
                    /* Call with left_down = false */
                    active_win->on_mouse(active_win, app_mx, app_my, false, false);
                }
            }
        }
    }
}

void zsr_request_full_redraw(zsr_compositor_t *compositor)
{
    if (!compositor) return;
    compositor->damage.count = 0;
    compositor->damage.full_redraw = true;
}

void zsr_add_window(
    zsr_compositor_t *compositor,
    const char *title,
    int x, int y, int width, int height,
    uint32_t *buffer, int buffer_pitch
)
{
    if (!compositor || !compositor->root) return;

    zsr_node_t *win = zsr_new_node(&compositor->document);
    if (!win) return;
    win->type = ZSR_NODE_WINDOW;
    win->bounds = zsr_rect_make(x, y, width, height);
    win->needs_layout = true;

    if (title) {
        zsr_node_t *titlebar = zsr_new_node(&compositor->document);
        if (titlebar) {
            titlebar->type = ZSR_NODE_TITLEBAR;
            titlebar->bounds = zsr_rect_make(x, y, width, 44);

            zsr_node_t *txt = zsr_new_node(&compositor->document);
            if (txt) {
                txt->type = ZSR_NODE_TEXT;
                txt->bounds = zsr_rect_make(x + 16, y + 12, 0, 0);
                zsr_attr_t *attr = &txt->attrs[txt->attr_count++];
                int i = 0;
                while (title[i] && i < ZSR_MAX_ATTR_VALUE - 1) {
                    attr->value[i] = title[i];
                    i++;
                }
                attr->value[i] = '\0';
                attr->name[0] = 'v'; attr->name[1] = 'a'; attr->name[2] = 'l'; attr->name[3] = 'u'; attr->name[4] = 'e'; attr->name[5] = '\0';
                zsr_add_child(titlebar, txt);
            }
            zsr_add_child(win, titlebar);
        }
    }

    if (buffer) {
        zsr_node_t *app_surface = zsr_new_node(&compositor->document);
        if (app_surface) {
            app_surface->type = ZSR_NODE_APP_SURFACE;
            app_surface->bounds = zsr_rect_make(x, y + 44, width, height - 44);

            zsr_attr_t *attr_buf = &app_surface->attrs[app_surface->attr_count++];
            attr_buf->name[0] = 'b'; attr_buf->name[1] = 'u'; attr_buf->name[2] = 'f'; attr_buf->name[3] = 'f'; attr_buf->name[4] = 'e'; attr_buf->name[5] = 'r'; attr_buf->name[6] = '\0';

            uintptr_t ptr_val = (uintptr_t)buffer;
            int idx = 0;
            if (ptr_val == 0) attr_buf->value[idx++] = '0';
            else {
                char temp[32];
                int t = 0;
                while (ptr_val > 0) { temp[t++] = '0' + (char)(ptr_val % 10); ptr_val /= 10; }
                while (t > 0) { attr_buf->value[idx++] = temp[--t]; }
            }
            attr_buf->value[idx] = '\0';

            zsr_attr_t *attr_pitch = &app_surface->attrs[app_surface->attr_count++];
            attr_pitch->name[0] = 'p'; attr_pitch->name[1] = 'i'; attr_pitch->name[2] = 't'; attr_pitch->name[3] = 'c'; attr_pitch->name[4] = 'h'; attr_pitch->name[5] = '\0';
            int p_val = buffer_pitch;
            idx = 0;
            if (p_val == 0) attr_pitch->value[idx++] = '0';
            else {
                char temp[20];
                int t = 0;
                while (p_val > 0) { temp[t++] = '0' + (p_val % 10); p_val /= 10; }
                while (t > 0) { attr_pitch->value[idx++] = temp[--t]; }
            }
            attr_pitch->value[idx] = '\0';

            zsr_add_child(win, app_surface);
        }
    }

    zsr_add_child(compositor->root, win);
    zsr_request_full_redraw(compositor);
}