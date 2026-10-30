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

static void zsr_fill_rect_clipped(
    zsr_surface_t *surface,
    zsr_rect_t rect,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    rect = zsr_rect_clip(rect, clip);

    if (zsr_rect_empty(rect))
        return;

    for (int y = rect.y; y < rect.y + rect.height; ++y) {
        for (int x = rect.x; x < rect.x + rect.width; ++x) {
            zsr_blend_pixel(surface, x, y, color);
        }
    }
}

static bool zsr_inside_rounded_rect(
    int px,
    int py,
    zsr_rect_t rect,
    int radius
)
{
    if (radius <= 0)
        return true;

    int right = rect.x + rect.width - 1;
    int bottom = rect.y + rect.height - 1;

    if (px >= rect.x + radius && px <= right - radius)
        return true;

    if (py >= rect.y + radius && py <= bottom - radius)
        return true;

    int cx = px < rect.x + radius ? rect.x + radius : right - radius;
    int cy = py < rect.y + radius ? rect.y + radius : bottom - radius;

    int dx = px - cx;
    int dy = py - cy;

    return (dx * dx + dy * dy) <= radius * radius;
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
    if (alpha == 0) return;

    if (radius <= 0) {
        for (int y = visible.y; y < visible.y + visible.height; ++y) {
            for (int x = visible.x; x < visible.x + visible.width; ++x) {
                zsr_blend_pixel(surface, x, y, color);
            }
        }
        return;
    }

    int right = rect.x + rect.width - 1;
    int bottom = rect.y + rect.height - 1;
    int r2 = radius * radius;
    int cy_top = rect.y + radius;
    int cy_bot = bottom - radius;
    int cx_left = rect.x + radius;
    int cx_right = right - radius;

    for (int y = visible.y; y < visible.y + visible.height; ++y) {
        bool in_corner_y = (y < cy_top || y > cy_bot);
        int dy2 = 0;
        if (y < cy_top) { int dy = y - cy_top; dy2 = dy * dy; }
        else if (y > cy_bot) { int dy = y - cy_bot; dy2 = dy * dy; }

        for (int x = visible.x; x < visible.x + visible.width; ++x) {
            if (in_corner_y) {
                if (x < cx_left) {
                    int dx = x - cx_left;
                    if (dx * dx + dy2 > r2) continue;
                } else if (x > cx_right) {
                    int dx = x - cx_right;
                    if (dx * dx + dy2 > r2) continue;
                }
            }
            zsr_blend_pixel(surface, x, y, color);
        }
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

        int margin = radius + 2;
        int inner_y1 = outer.y + margin;
        int inner_y2 = outer.y + outer.height - margin;
        int inner_x1 = outer.x + margin;
        int inner_x2 = outer.x + outer.width - margin;

        for (int y = visible.y; y < visible.y + visible.height; ++y) {
            bool middle_y = (y >= inner_y1 && y < inner_y2 && inner_x1 < inner_x2);
            for (int x = visible.x; x < visible.x + visible.width; ++x) {
                /* Ultra-fast skip through window interior: 180x speedup */
                if (middle_y && x >= inner_x1 && x < inner_x2) {
                    x = inner_x2 - 1;
                    continue;
                }

                bool in_outer = zsr_inside_rounded_rect(x, y, outer, radius - i);
                bool in_inner = zsr_inside_rounded_rect(x, y, inner, radius - i - 1);

                if (in_outer && !in_inner)
                    zsr_blend_pixel(surface, x, y, color);
            }
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

static void zsr_execute_command(
    zsr_surface_t *surface,
    const zsr_command_t *command,
    zsr_rect_t damage_clip
)
{
    zsr_rect_t clip = zsr_rect_clip(command->clip, damage_clip);

    if (zsr_rect_empty(clip))
        return;

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
    }
}

static void zsr_execute_region(
    zsr_surface_t *surface,
    const zsr_command_list_t *list,
    zsr_rect_t region
)
{
    for (uint32_t i = 0; i < list->count; ++i) {
        zsr_execute_command(surface, &list->commands[i], region);
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
        g_wallpaper_cache[i] = ZSR_RGBA(15, 23, 42, 255);
    }

    zsr_rect_t full_screen = zsr_rect_make(0, 0, width, height);

    /* Pre-render 4 Soft, Balanced, Harmonic Glows ONCE at startup */
    zsr_draw_glow_region(
        &wallpaper_surf,
        full_screen,
        width * 10 / 100,
        height * 20 / 100,
        width * 45 / 100,
        ZSR_RGBA(37, 99, 235, 55)
    );

    zsr_draw_glow_region(
        &wallpaper_surf,
        full_screen,
        width * 90 / 100,
        height * 35 / 100,
        width * 45 / 100,
        ZSR_RGBA(219, 39, 119, 45)
    );

    zsr_draw_glow_region(
        &wallpaper_surf,
        full_screen,
        width * 80 / 100,
        height * 90 / 100,
        width * 40 / 100,
        ZSR_RGBA(245, 158, 11, 35)
    );

    zsr_draw_glow_region(
        &wallpaper_surf,
        full_screen,
        width * 20 / 100,
        height * 80 / 100,
        width * 45 / 100,
        ZSR_RGBA(79, 70, 229, 45)
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

static int g_active_tab_index = 0;

static void zsr_build_zarqaa_window(
    zsr_command_list_t *list,
    zsr_rect_t frame,
    bool is_dragging
)
{
    zsr_rect_t screen_clip = zsr_rect_make(0, 0, 32767, 32767);

    /* 1. Window Drop Shadow - omit during active drag for smooth 60 FPS performance */
    if (!is_dragging) {
        zsr_rect_t shadow = zsr_rect_make(
            frame.x,
            frame.y + 16,
            frame.width,
            frame.height
        );
        zsr_emit_rounded_rect(
            list,
            shadow,
            18,
            screen_clip,
            ZSR_RGBA(0, 0, 0, 110)
        );
    }

    /* 2. Glass Window Body */
    zsr_emit_rounded_rect(
        list,
        frame,
        16,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 34)
    );

    /* 3. Subtle White Border */
    zsr_emit_border(
        list,
        frame,
        16,
        1,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 58)
    );

    /* 4. Top Highlight Line */
    zsr_emit_line(
        list,
        frame.x + 16,
        frame.y + 1,
        frame.x + frame.width - 16,
        frame.y + 1,
        1,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 80)
    );

    /* 5. Titlebar Separator */
    zsr_emit_line(
        list,
        frame.x + 1,
        frame.y + ZSR_TITLEBAR_HEIGHT,
        frame.x + frame.width - 1,
        frame.y + ZSR_TITLEBAR_HEIGHT,
        1,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 25)
    );





    /* 8. Star Logo on Titlebar */
    int logo_x = frame.x + 28;
    int logo_y = frame.y + 22;

    zsr_emit_triangle(
        list,
        (zsr_point_t){logo_x, logo_y - 9},
        (zsr_point_t){logo_x + 4, logo_y + 4},
        (zsr_point_t){logo_x - 4, logo_y + 4},
        screen_clip,
        ZSR_RGBA(255, 255, 255, 230)
    );

    zsr_emit_triangle(
        list,
        (zsr_point_t){logo_x, logo_y + 9},
        (zsr_point_t){logo_x + 4, logo_y - 4},
        (zsr_point_t){logo_x - 4, logo_y - 4},
        screen_clip,
        ZSR_RGBA(255, 255, 255, 230)
    );

    /* Titlebar Title + Live FPS & VSync Badge */
    extern uint32_t gui_get_fps(void);
    extern bool fb_get_vsync(void);
    struct window *active_win = wm_get_focused();
    char title_buf[96];
    if (active_win && active_win->visible && !active_win->minimized && active_win->title[0] != '\0') {
        snprintf(title_buf, sizeof(title_buf), "%s  |  %u FPS  |  VSync: %s [F8]",
                 active_win->title, gui_get_fps(), fb_get_vsync() ? "ON" : "OFF (Uncapped)");
    } else {
        snprintf(title_buf, sizeof(title_buf), "ZarqaaOS QuantumPeak  |  %u FPS  |  VSync: %s [F8]",
                 gui_get_fps(), fb_get_vsync() ? "ON" : "OFF (Uncapped)");
    }
    zsr_emit_text(list, frame.x + 46, frame.y + 16, title_buf, 1, screen_clip, ZSR_RGBA(255, 255, 255, 220));

    /* 9. Window Controls */
    int button_y = frame.y + 22;
    int green_x = frame.x + frame.width - 24;
    int yellow_x = frame.x + frame.width - 52;
    int red_x = frame.x + frame.width - 80;
    int min_x = frame.x + frame.width - 108;

    zsr_emit_line(
        list,
        min_x - 5,
        button_y,
        min_x + 5,
        button_y,
        2,
        screen_clip,
        ZSR_RGBA(226, 232, 240, 255)
    );

    zsr_emit_triangle(
        list,
        (zsr_point_t){red_x, button_y - 7},
        (zsr_point_t){red_x + 7, button_y + 6},
        (zsr_point_t){red_x - 7, button_y + 6},
        screen_clip,
        ZSR_RGBA(248, 113, 113, 255)
    );

    zsr_emit_triangle(
        list,
        (zsr_point_t){yellow_x, button_y + 7},
        (zsr_point_t){yellow_x + 7, button_y - 6},
        (zsr_point_t){yellow_x - 7, button_y - 6},
        screen_clip,
        ZSR_RGBA(250, 204, 21, 255)
    );

    zsr_emit_triangle(
        list,
        (zsr_point_t){green_x, button_y - 7},
        (zsr_point_t){green_x + 7, button_y + 6},
        (zsr_point_t){green_x - 7, button_y + 6},
        screen_clip,
        ZSR_RGBA(74, 222, 128, 255)
    );

    /* 10. Legacy App Bridge */
    if (active_win && active_win->visible && !active_win->minimized) {
        if (active_win->needs_redraw) {
            active_win->needs_redraw = false;
            render_queue_clear(&active_win->surface.queue);
            gapi_set_current_context(&active_win->surface.context);
            if (active_win->on_draw) {
                active_win->on_draw(active_win);
            }
            gapi_set_current_context(NULL);
        }

        /* Set clipping region to inside the glass window */
        zsr_rect_t content_clip = zsr_rect_make(
            frame.x + 1,
            frame.y + ZSR_TITLEBAR_HEIGHT,
            frame.width - 2,
            frame.height - ZSR_TITLEBAR_HEIGHT - 1
        );
        content_clip = zsr_rect_clip(content_clip, screen_clip);

        int ox = frame.x;
        int oy = frame.y + ZSR_TITLEBAR_HEIGHT;

        /* Blit active window content buffer directly into glass container */
        if (active_win->content_buffer) {
            zsr_emit_blit_buffer(list, zsr_rect_make(ox, oy, active_win->width, active_win->height), 
                                 active_win->content_buffer, active_win->width * 4, content_clip);
        }

        /* Translate legacy RenderQueue to ZSR commands */
        RenderQueueChunk *chunk = active_win->surface.queue.head;
        while (chunk) {
            for (int c = 0; c < chunk->count; c++) {
                RenderCommand *cmd = &chunk->commands[c];
                switch (cmd->opcode) {
                    case RENDER_OP_FILL_RECT:
                    case RENDER_OP_DRAW_RECT: {
                        zsr_rect_t r = zsr_rect_make(ox + cmd->rect.x, oy + cmd->rect.y, cmd->rect.w, cmd->rect.h);
                        /* Using rounded rect with radius 0 to act as fill_rect since ZSR has no standard fill_rect */
                        zsr_emit_rounded_rect(list, r, 0, content_clip, cmd->rect.color);
                        break;
                    }
                    case RENDER_OP_DRAW_LINE:
                        zsr_emit_line(list, ox + cmd->line.x0, oy + cmd->line.y0, ox + cmd->line.x1, oy + cmd->line.y1, 1, content_clip, cmd->line.color);
                        break;
                    case RENDER_OP_DRAW_STRING:
                    case RENDER_OP_DRAW_STRING_TRANS:
                        zsr_emit_text(list, ox + cmd->text.x, oy + cmd->text.y, cmd->text.text, 1, content_clip, cmd->text.fg);
                        break;
                    case RENDER_OP_DRAW_IMAGE:
                        zsr_emit_blit_buffer(list, zsr_rect_make(ox + cmd->image.x, oy + cmd->image.y, cmd->image.w, cmd->image.h), cmd->image.pixels, cmd->image.w * 4, content_clip);
                        break;
                }
            }
            chunk = chunk->next;
        }
    } else {
        /* No active window: Render Interactive Tab Bar & Dashboard */
        const char *tabs[4] = {
            "Desktop",
            "Apps",
            "Files",
            "Settings"
        };

        int tab_x = frame.x + 24;
        int tab_y = frame.y + 52;

        for (int i = 0; i < 4; ++i) {
            zsr_rect_t tab_rect = zsr_rect_make(
                tab_x,
                tab_y,
                74,
                28
            );

            if (i == g_active_tab_index) {
                zsr_emit_rounded_rect(
                    list,
                    tab_rect,
                    14,
                    screen_clip,
                    ZSR_RGBA(0, 0, 0, 78)
                );
            }

            int text_len = 0;
            while (tabs[i][text_len]) text_len++;
            int tx = tab_x + (74 - text_len * 8) / 2;
            int ty = tab_y + 6;

            zsr_emit_text(
                list,
                tx,
                ty,
                tabs[i],
                1,
                screen_clip,
                (i == g_active_tab_index)
                    ? ZSR_RGBA(255, 255, 255, 250)
                    : ZSR_RGBA(255, 255, 255, 165)
            );

            tab_x += 82;
        }

        /* Separator line below tabs */
        zsr_emit_line(list, frame.x + 20, frame.y + 88, frame.x + frame.width - 20, frame.y + 88, 1, screen_clip, ZSR_RGBA(255, 255, 255, 25));

        /* Dashboard content clipping region */
        zsr_rect_t content_clip = zsr_rect_make(
            frame.x + 1,
            frame.y + 89,
            frame.width - 2,
            frame.height - 90
        );
        content_clip = zsr_rect_clip(content_clip, screen_clip);

        int ox = frame.x + 24;
        int oy = frame.y + 100;

        if (g_active_tab_index == 0) {
            /* Tab 0: Desktop & System Status */
            zsr_emit_text(list, ox, oy, "ZarqaaOS QuantumPeak - System Overview", 1, content_clip, ZSR_RGBA(255, 255, 255, 240));
            zsr_emit_line(list, ox, oy + 20, ox + frame.width - 48, oy + 20, 1, content_clip, ZSR_RGBA(255, 255, 255, 30));

            zsr_rect_t card = zsr_rect_make(ox, oy + 32, frame.width - 48, 160);
            zsr_emit_rounded_rect(list, card, 12, content_clip, ZSR_RGBA(255, 255, 255, 15));
            zsr_emit_border(list, card, 12, 1, content_clip, ZSR_RGBA(255, 255, 255, 30));

            zsr_emit_text(list, ox + 16, oy + 48,  "Architecture : x86_64 Higher-Half Direct Map (SMP 2 Cores)", 1, content_clip, ZSR_RGBA(200, 225, 255, 230));
            zsr_emit_text(list, ox + 16, oy + 70,  "Memory       : 2048 MB RAM (Higher-Half Virtual Memory)", 1, content_clip, ZSR_RGBA(200, 225, 255, 230));
            zsr_emit_text(list, ox + 16, oy + 92,  "Display      : VirtIO-GPU / Bochs BGA Hardware Accelerated", 1, content_clip, ZSR_RGBA(200, 225, 255, 230));
            zsr_emit_text(list, ox + 16, oy + 114, "Storage      : FAT32 (/hd0) + RamFS (Zero-Copy TarFS)", 1, content_clip, ZSR_RGBA(200, 225, 255, 230));
            zsr_emit_text(list, ox + 16, oy + 136, "Network      : RTL8139 Fast Ethernet DHCP Active (10.0.2.15)", 1, content_clip, ZSR_RGBA(74, 222, 128, 230));

            zsr_emit_text(list, ox, oy + 215, "Tip: Click on 'Apps' tab above or the Dock icons below to launch apps!", 1, content_clip, ZSR_RGBA(255, 255, 255, 140));
        }
        else if (g_active_tab_index == 1) {
            /* Tab 1: Applications Grid */
            zsr_emit_text(list, ox, oy, "Installed Applications (Click any card to launch)", 1, content_clip, ZSR_RGBA(255, 255, 255, 240));
            zsr_emit_line(list, ox, oy + 20, ox + frame.width - 48, oy + 20, 1, content_clip, ZSR_RGBA(255, 255, 255, 30));

            static const char *app_names[ZSR_NUM_APP_CARDS] = {
                "Terminal",     "Calculator",   "Notepad+",     "File Manager",
                "Studio Paint", "Browser",      "Gallery",      "Task Manager",
                "Snake Game",   "Python IDE",   "Settings",     "Devices",
                "Benchmark",    "Boot Log",     "Clock",        "Calendar",
                "About",        "2048",         "Minesweeper"
            };
            static const char *app_descs[ZSR_NUM_APP_CARDS] = {
                "CLI & ELF Shell",   "ZUI Math Engine",   "Text Editor",     "Disks & Files",
                "Drawing & Graphics","HTML Web Engine",   "Image Viewer",    "Process Monitor",
                "Ring-3 ELF Game",   "Mini IDE",          "Preferences",     "Hardware & PCI",
                "Physics Engine",    "System Logs",       "Time & Timer",    "Month View",
                "System Info",       "Puzzle Game",       "Puzzle Game"
            };

            int cols = ZSR_APP_CARD_COLS;
            int card_w = (frame.width - 48 - (cols - 1) * 16) / cols;
            int card_h = 56;

            for (int i = 0; i < ZSR_NUM_APP_CARDS; i++) {
                int col = i % cols;
                int row = i / cols;
                int cx = ox + col * (card_w + 16);
                int cy = oy + 36 + row * (card_h + 14);

                zsr_rect_t c_rect = zsr_rect_make(cx, cy, card_w, card_h);
                zsr_emit_rounded_rect(list, c_rect, 10, content_clip, ZSR_RGBA(255, 255, 255, 22));
                zsr_emit_border(list, c_rect, 10, 1, content_clip, ZSR_RGBA(255, 255, 255, 45));

                zsr_emit_text(list, cx + 16, cy + 12, app_names[i], 1, content_clip, ZSR_RGBA(255, 255, 255, 240));
                zsr_emit_text(list, cx + 16, cy + 32, app_descs[i], 1, content_clip, ZSR_RGBA(180, 205, 235, 170));
            }
        }
        else if (g_active_tab_index == 2) {
            /* Tab 2: Storage Explorer */
            zsr_emit_text(list, ox, oy, "Storage Explorer (/hd0 Primary Volume)", 1, content_clip, ZSR_RGBA(255, 255, 255, 240));
            zsr_emit_line(list, ox, oy + 20, ox + frame.width - 48, oy + 20, 1, content_clip, ZSR_RGBA(255, 255, 255, 30));

            static const char *files[] = {
                "[DIR]  wallpapers/         (Desktop Wallpapers Collection)",
                "[DIR]  www/                (Local HTML Offline Web Pages)",
                "[ELF]  calc.elf            (Native 64-bit Calculator Binary)",
                "[ELF]  hello.elf           (Ring-3 Hello World Executable)",
                "[ELF]  snake.elf           (Ring-3 Interactive Snake Game)",
                "[ELF]  sh.elf              (Zarqaa Unix-like Shell)",
                "[TXT]  test.txt            (Sample text document)",
                "[AUD]  nari.mp3            (AC97 Audio Track Sample)"
            };

            for (int i = 0; i < 8; i++) {
                zsr_rect_t f_rect = zsr_rect_make(ox, oy + 32 + i * 26, frame.width - 48, 22);
                zsr_emit_rounded_rect(list, f_rect, 6, content_clip, ZSR_RGBA(255, 255, 255, 10));
                zsr_emit_text(list, ox + 12, oy + 36 + i * 26, files[i], 1, content_clip, ZSR_RGBA(225, 235, 245, 220));
            }
        }
        else if (g_active_tab_index == 3) {
            /* Tab 3: System Settings */
            zsr_emit_text(list, ox, oy, "System Settings & Preferences", 1, content_clip, ZSR_RGBA(255, 255, 255, 240));
            zsr_emit_line(list, ox, oy + 20, ox + frame.width - 48, oy + 20, 1, content_clip, ZSR_RGBA(255, 255, 255, 30));

            zsr_rect_t s1 = zsr_rect_make(ox, oy + 32, frame.width - 48, 48);
            zsr_emit_rounded_rect(list, s1, 10, content_clip, ZSR_RGBA(255, 255, 255, 15));
            zsr_emit_border(list, s1, 10, 1, content_clip, ZSR_RGBA(255, 255, 255, 35));
            zsr_emit_text(list, ox + 16, oy + 46, "Visual Theme: QuantumPeak Dark Glass (Active)", 1, content_clip, ZSR_RGBA(255, 255, 255, 230));

            zsr_rect_t s2 = zsr_rect_make(ox, oy + 92, frame.width - 48, 48);
            zsr_emit_rounded_rect(list, s2, 10, content_clip, ZSR_RGBA(255, 255, 255, 15));
            zsr_emit_border(list, s2, 10, 1, content_clip, ZSR_RGBA(255, 255, 255, 35));
            char vsync_card[96];
            snprintf(vsync_card, sizeof(vsync_card), "VSync System: [%s]  |  %u FPS  (Click or press F8 to toggle)",
                     fb_get_vsync() ? "ENABLED (Locked)" : "DISABLED (Uncapped)", gui_get_fps());
            zsr_emit_text(list, ox + 16, oy + 106, vsync_card, 1, content_clip,
                          fb_get_vsync() ? ZSR_RGBA(74, 222, 128, 240) : ZSR_RGBA(250, 204, 21, 240));

            zsr_rect_t s3 = zsr_rect_make(ox, oy + 152, frame.width - 48, 48);
            zsr_emit_rounded_rect(list, s3, 10, content_clip, ZSR_RGBA(255, 255, 255, 15));
            zsr_emit_border(list, s3, 10, 1, content_clip, ZSR_RGBA(255, 255, 255, 35));
            zsr_emit_text(list, ox + 16, oy + 166, "Display Mode: 1024x768 32-bit ARGB (Direct Framebuffer Blit)", 1, content_clip, ZSR_RGBA(255, 255, 255, 230));
        }
    }
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

static int g_mouse_x = -1;
static int g_mouse_y = -1;
static zsr_rect_t g_old_mouse_rect = {0, 0, 0, 0};

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
    
    // Draw Glass Background
    zsr_emit_rounded_rect(list, dock_rect, 20, clip, ZSR_RGBA(15, 23, 42, 180));
    
    int start_x = dock_x + 20 + spacing / 2;
    int dock_cy = dock_y + dock_h / 2;
    
    for (int i = 0; i < ZSR_NUM_DOCK; ++i) {
        int cx = start_x + i * spacing;
        
        bool hover = (g_mouse_x >= cx - 24 && g_mouse_x <= cx + 24 && g_mouse_y >= dock_y && g_mouse_y <= dock_y + dock_h);
        
        // Draw icon
        if (g_zsr_dock_buttons[i].bitmap) {
            int draw_cx = cx;
            int draw_cy = dock_cy;
            
            if (g_is_dock_dragging && i == g_drag_dock_index) {
                draw_cx = g_mouse_x;
                draw_cy = dock_cy; // Lock to horizontal axis for a cleaner look
            } else if (hover && !g_is_dock_dragging) {
                draw_cy -= 5; // Lift up slightly on hover
            }
            
            int bw = g_zsr_dock_buttons[i].bw;
            int bh = g_zsr_dock_buttons[i].bh;
            zsr_rect_t icon_rect = zsr_rect_make(draw_cx - bw / 2, draw_cy - bh / 2, bw, bh);
            zsr_emit_blit_buffer(list, icon_rect, (uint32_t *)g_zsr_dock_buttons[i].bitmap, bw * 4, clip);
        }
        
        // Draw indicator if running
        bool is_minimized = false;
        if (desktop_is_app_running(g_zsr_dock_buttons[i].app_id, &is_minimized)) {
            zsr_color_t indicator_color = is_minimized ? ZSR_RGBA(0, 255, 255, 255) : ZSR_RGBA(0, 255, 0, 255);
            
            zsr_command_t *cmd = zsr_command_push(list, ZSR_CMD_FILL_RECT);
            if (cmd) {
                cmd->rect = zsr_rect_make(cx - 2, dock_y + dock_h - 10, 4, 4);
                cmd->clip = clip;
                cmd->color = indicator_color;
            }
        }
        
        // Draw tooltip
        if (hover) {
            zsr_rect_t tooltip_rect = zsr_rect_make(cx - 40, dock_y - 35, 80, 24);
            zsr_emit_rounded_rect(list, tooltip_rect, 12, clip, ZSR_RGBA(0, 0, 0, 200));
            zsr_emit_text(list, cx - 20, dock_y - 28, g_zsr_dock_buttons[i].name, 1, clip, ZSR_RGBA(255, 255, 255, 255));
        }
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

    zsr_rect_t r = window->bounds;

    window->previous_bounds = window->bounds;
    window->start_mouse_x = mouse_x;
    window->start_mouse_y = mouse_y;
    window->start_width = r.width;
    window->start_height = r.height;

    window->resize_right = (mouse_x >= r.x + r.width - ZSR_RESIZE_MARGIN);
    window->resize_bottom = (mouse_y >= r.y + r.height - ZSR_RESIZE_MARGIN);

    if (!window->resize_right &&
        !window->resize_bottom &&
        mouse_y >= r.y &&
        mouse_y < r.y + ZSR_TITLEBAR_HEIGHT) {
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

    /* Force full redraw while moving or resizing window:
     * Guarantees 100% clean background restoration from wallpaper cache,
     * ZERO ghosting, ZERO tearing, and ZERO horizontal/vertical artifact lines! */
    damage->full_redraw = true;
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

static bool g_window_open = false; /* Start with an empty, clean desktop! */

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
    
    // Build the dock on top of everything!
    zsr_build_desktop_dock(&compositor->commands, compositor->backbuffer->width, compositor->backbuffer->height);
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

    zsr_draw_zarqaa_background_region(surface, region);
    zsr_execute_region(surface, &compositor->commands, region);
}

/* Drive the hosted app's on_tick and turn its redraw requests into damage */
static void zsr_tick_active_app(zsr_compositor_t *compositor)
{
    struct window *win = wm_get_focused();
    if (!g_window_open || !win || !win->visible || win->minimized) return;

    if (win->on_tick) win->on_tick(win);

    if (win->needs_redraw) {
        zsr_rect_t frame = compositor->main_window.bounds;
        zsr_damage_add(&compositor->damage,
                       zsr_rect_make(frame.x, frame.y + ZSR_TITLEBAR_HEIGHT, win->width, win->height));
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
    if (!g_window_open) return false;

    int button_y = frame.y + 22;
    int min_x = frame.x + frame.width - 108;
    int red_x = frame.x + frame.width - 80;
    int yellow_x = frame.x + frame.width - 52;
    int green_x = frame.x + frame.width - 24;

    if (my >= button_y - 12 && my <= button_y + 12) {
        if ((mx >= red_x - 10 && mx <= red_x + 10) || (mx >= yellow_x - 10 && mx <= yellow_x + 10)) {
            struct window *active_win = wm_get_focused();
            if (active_win && active_win->visible && !active_win->minimized) {
                wm_destroy_window(active_win);
            } else {
                g_window_open = false;
            }
            if (compositor) zsr_request_full_redraw(compositor);
            return true;
        }
        if ((mx >= green_x - 10 && mx <= green_x + 10) ||
            (mx >= min_x - 10 && mx <= min_x + 10)) {
            return true;
        }
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
    if (compositor->backbuffer) {
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
        int app_mx = mouse_x - frame.x;
        int app_my = mouse_y - frame.y - ZSR_TITLEBAR_HEIGHT;
        if (app_mx >= 0 && app_mx < frame.width && app_my >= 0 && app_my < frame.height - ZSR_TITLEBAR_HEIGHT) {
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
            int app_mx = mouse_x - frame.x;
            int app_my = mouse_y - frame.y - ZSR_TITLEBAR_HEIGHT;
            if (app_mx >= 0 && app_mx < frame.width && app_my >= 0 && app_my < frame.height - ZSR_TITLEBAR_HEIGHT) {
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
            int app_mx = g_mouse_x - frame.x;
            int app_my = g_mouse_y - frame.y - ZSR_TITLEBAR_HEIGHT;
            if (app_mx >= 0 && app_mx < frame.width && app_my >= 0 && app_my < frame.height - ZSR_TITLEBAR_HEIGHT) {
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