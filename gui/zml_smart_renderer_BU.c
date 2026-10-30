/*
 * Zarqaa OS - ZML Smart Renderer
 * --------------------------------
 * ملف C واحد:
 * 1. ZML / XML subset parser
 * 2. Widget scene tree
 * 3. Smart render command list
 * 4. Dirty rectangles
 * 5. CPU framebuffer renderer
 * 6. Zarqaa glass window demo
 *
 * لا يعتمد على:
 * - C++
 * - Qt
 * - X11 / Wayland
 * - malloc / free
 * - libc كاملة
 *
 * يجب ربط zsr_present() مع framebuffer/backbuffer الخاص بك.
 */

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
   1) CONFIGURATION
   ============================================================ */

#define ZSR_MAX_NODES            128
#define ZSR_MAX_ATTRS            12
#define ZSR_MAX_DEPTH            24
#define ZSR_MAX_TAG              24
#define ZSR_MAX_ATTR_NAME        24
#define ZSR_MAX_ATTR_VALUE       96
#define ZSR_MAX_DAMAGE           32
#define ZSR_MAX_COMMANDS         512
#define ZSR_MAX_TEXT             96

#define ZSR_MIN_WINDOW_WIDTH     500
#define ZSR_MIN_WINDOW_HEIGHT    400

#define ZSR_TITLEBAR_HEIGHT      44
#define ZSR_RESIZE_MARGIN        8

/* ============================================================
   2) FRAMEBUFFER ADAPTER
   ============================================================ */

/*
 * عدّل هذه البنية لتطابق framebuffer عندك.
 *
 * pixels: عنوان الـ backbuffer وليس بالضرورة VRAM المباشر.
 * width:  عرض الشاشة بالبكسل.
 * height: ارتفاع الشاشة بالبكسل.
 * pitch:  عدد الـ bytes في كل سطر.
 *
 * الافتراض في هذا الملف:
 * 32-bit pixels بصيغة 0xAARRGGBB.
 */

typedef uint32_t zsr_color_t;

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    int pitch;
} zsr_surface_t;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} zsr_rect_t;

typedef struct {
    int x;
    int y;
} zsr_point_t;

#define ZSR_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | \
     ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) << 8)  | \
     ((uint32_t)(b)))

#define ZSR_RED(c)   (((c) >> 16) & 0xFF)
#define ZSR_GREEN(c) (((c) >> 8) & 0xFF)
#define ZSR_BLUE(c)  ((c) & 0xFF)
#define ZSR_ALPHA(c) (((c) >> 24) & 0xFF)

/*
 * اربط هذا مع دالة العرض لديك.
 *
 * مثال تقريبي:
 *
 * void zsr_present(zsr_surface_t *backbuffer) {
 *     fb_copy_to_frontbuffer(
 *         backbuffer->pixels,
 *         backbuffer->pitch
 *     );
 * }
 */
__attribute__((weak))
void zsr_present(zsr_surface_t *surface)
{
    (void)surface;
}

/* ============================================================
   3) BASIC STRING / NUMBER HELPERS
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

static uint8_t zsr_parse_opacity(const char *text, uint8_t fallback)
{
    /*
     * تقبل:
     * opacity="0.14" -> 35 تقريباً
     * opacity="35"   -> 35
     */

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
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

static zsr_color_t zsr_parse_color(const char *text, zsr_color_t fallback)
{
    /*
     * يدعم فقط:
     * #RRGGBB
     * #AARRGGBB
     */

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
   4) RECTANGLE HELPERS
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

/* ============================================================
   5) FRAMEBUFFER DRAWING
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

    zsr_color_t destination = *pixel;

    uint32_t alpha = ZSR_ALPHA(source);
    uint32_t inverse_alpha = 255 - alpha;

    uint32_t r =
        (ZSR_RED(source) * alpha +
         ZSR_RED(destination) * inverse_alpha) / 255;

    uint32_t g =
        (ZSR_GREEN(source) * alpha +
         ZSR_GREEN(destination) * inverse_alpha) / 255;

    uint32_t b =
        (ZSR_BLUE(source) * alpha +
         ZSR_BLUE(destination) * inverse_alpha) / 255;

    *pixel = ZSR_RGBA(r, g, b, 255);
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

    for (int y = visible.y; y < visible.y + visible.height; ++y) {
        for (int x = visible.x; x < visible.x + visible.width; ++x) {
            if (zsr_inside_rounded_rect(x, y, rect, radius)) {
                zsr_blend_pixel(surface, x, y, color);
            }
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

        for (int y = visible.y; y < visible.y + visible.height; ++y) {
            for (int x = visible.x; x < visible.x + visible.width; ++x) {
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
   6) TINY BITMAP TEXT
   ============================================================ */

/*
 * هذا ليس خطًا نهائيًا. هو placeholder صغير للـ demo.
 * اربطه لاحقًا مع font.c الخاص بك:
 *
 * gfx_draw_text(surface, x, y, text, font, size, color);
 */

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
    if (!text)
        return;

    int cursor_x = x;

    while (*text) {
        if (*text != ' ') {
            zsr_rect_t glyph = zsr_rect_make(
                cursor_x,
                y,
                5 * scale,
                8 * scale
            );

            zsr_fill_rect_clipped(
                surface,
                glyph,
                clip,
                color
            );

            zsr_rect_t cut_a = zsr_rect_make(
                cursor_x + scale,
                y + scale,
                3 * scale,
                2 * scale
            );

            zsr_fill_rect_clipped(
                surface,
                cut_a,
                clip,
                ZSR_RGBA(0, 0, 0, 0)
            );
        }

        cursor_x += 7 * scale;
        ++text;
    }
}

/* ============================================================
   7) XML / ZML TREE
   ============================================================ */

typedef enum {
    ZSR_NODE_UNKNOWN,
    ZSR_NODE_DESKTOP,
    ZSR_NODE_GLOW,
    ZSR_NODE_WINDOW,
    ZSR_NODE_TITLEBAR,
    ZSR_NODE_ROW,
    ZSR_NODE_COLUMN,
    ZSR_NODE_TEXT,
    ZSR_NODE_TABS,
    ZSR_NODE_TAB,
    ZSR_NODE_DOCK,
    ZSR_NODE_DOCK_ITEM,
    ZSR_NODE_ICON,
    ZSR_NODE_SPACER,
    ZSR_NODE_BOTTOM_BAR,
    ZSR_NODE_STATUS_DOT
} zsr_node_type_t;

typedef struct {
    char name[ZSR_MAX_ATTR_NAME];
    char value[ZSR_MAX_ATTR_VALUE];
} zsr_attr_t;

typedef struct zsr_node zsr_node_t;

struct zsr_node {
    zsr_node_type_t type;
    char tag[ZSR_MAX_TAG];

    zsr_attr_t attrs[ZSR_MAX_ATTRS];
    uint8_t attr_count;

    zsr_node_t *parent;
    zsr_node_t *first_child;
    zsr_node_t *last_child;
    zsr_node_t *next_sibling;

    zsr_rect_t bounds;
    zsr_rect_t old_bounds;

    bool visible;
    bool needs_layout;
    bool needs_paint;
    bool cacheable;
};

typedef struct {
    zsr_node_t nodes[ZSR_MAX_NODES];
    uint32_t node_count;
} zsr_document_t;

typedef struct {
    const char *message;
    uint32_t offset;
    uint32_t line;
    uint32_t column;
} zsr_parse_error_t;

static zsr_node_type_t zsr_tag_type(const char *tag)
{
    if (zsr_string_equal(tag, "desktop"))    return ZSR_NODE_DESKTOP;
    if (zsr_string_equal(tag, "glow"))       return ZSR_NODE_GLOW;
    if (zsr_string_equal(tag, "window"))     return ZSR_NODE_WINDOW;
    if (zsr_string_equal(tag, "titlebar"))   return ZSR_NODE_TITLEBAR;
    if (zsr_string_equal(tag, "row"))        return ZSR_NODE_ROW;
    if (zsr_string_equal(tag, "column"))     return ZSR_NODE_COLUMN;
    if (zsr_string_equal(tag, "text"))       return ZSR_NODE_TEXT;
    if (zsr_string_equal(tag, "tabs"))       return ZSR_NODE_TABS;
    if (zsr_string_equal(tag, "tab"))        return ZSR_NODE_TAB;
    if (zsr_string_equal(tag, "dock"))       return ZSR_NODE_DOCK;
    if (zsr_string_equal(tag, "dock-item"))  return ZSR_NODE_DOCK_ITEM;
    if (zsr_string_equal(tag, "icon"))       return ZSR_NODE_ICON;
    if (zsr_string_equal(tag, "spacer"))     return ZSR_NODE_SPACER;
    if (zsr_string_equal(tag, "bottom-bar")) return ZSR_NODE_BOTTOM_BAR;
    if (zsr_string_equal(tag, "status-dot")) return ZSR_NODE_STATUS_DOT;

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
        zsr_set_parse_error(
            error,
            "Too many attributes",
            *pos,
            *line,
            *column
        );
        return false;
    }

    zsr_attr_t *attribute = &node->attrs[node->attr_count];

    if (!zsr_read_name(
            source,
            length,
            pos,
            attribute->name,
            sizeof(attribute->name)
        )) {
        zsr_set_parse_error(
            error,
            "Invalid attribute name",
            *pos,
            *line,
            *column
        );
        return false;
    }

    zsr_skip_spaces(source, length, pos, line, column);

    if (*pos >= length || source[*pos] != '=') {
        zsr_set_parse_error(
            error,
            "Expected '=' after attribute name",
            *pos,
            *line,
            *column
        );
        return false;
    }

    ++(*pos);
    ++(*column);

    zsr_skip_spaces(source, length, pos, line, column);

    if (*pos >= length ||
        (source[*pos] != '"' && source[*pos] != '\'')) {
        zsr_set_parse_error(
            error,
            "Attribute values must use quotes",
            *pos,
            *line,
            *column
        );
        return false;
    }

    char quote = source[*pos];
    ++(*pos);
    ++(*column);

    uint32_t start = *pos;

    while (*pos < length && source[*pos] != quote) {
        char c = source[*pos];

        if (c == '<' || c == '>' || c == '\n') {
            zsr_set_parse_error(
                error,
                "Invalid character in attribute value",
                *pos,
                *line,
                *column
            );
            return false;
        }

        ++(*pos);
        ++(*column);
    }

    if (*pos >= length) {
        zsr_set_parse_error(
            error,
            "Unterminated attribute value",
            *pos,
            *line,
            *column
        );
        return false;
    }

    zsr_copy_string(
        attribute->value,
        sizeof(attribute->value),
        source + start,
        *pos - start
    );

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
            zsr_set_parse_error(
                error,
                "Text between tags is not supported; use value=\"...\"",
                pos,
                line,
                column
            );
            return false;
        }

        /*
         * أمان:
         * نرفض XML الكامل المعقد:
         * <!DOCTYPE ...>
         * <!ENTITY ...>
         * <!-- ... -->
         * <?xml ... ?>
         */
        if (pos + 1 < length &&
            (source[pos + 1] == '!' || source[pos + 1] == '?')) {
            zsr_set_parse_error(
                error,
                "DOCTYPE, ENTITY, comments and processing instructions are disabled",
                pos,
                line,
                column
            );
            return false;
        }

        ++pos;
        ++column;

        /* Closing tag: </window> */
        if (pos < length && source[pos] == '/') {
            char closing_tag[ZSR_MAX_TAG];

            ++pos;
            ++column;

            if (!zsr_read_name(
                    source,
                    length,
                    &pos,
                    closing_tag,
                    sizeof(closing_tag)
                )) {
                zsr_set_parse_error(
                    error,
                    "Invalid closing tag",
                    pos,
                    line,
                    column
                );
                return false;
            }

            zsr_skip_spaces(source, length, &pos, &line, &column);

            if (pos >= length || source[pos] != '>') {
                zsr_set_parse_error(
                    error,
                    "Expected '>' after closing tag",
                    pos,
                    line,
                    column
                );
                return false;
            }

            ++pos;
            ++column;

            if (depth == 0) {
                zsr_set_parse_error(
                    error,
                    "Closing tag without opening tag",
                    pos,
                    line,
                    column
                );
                return false;
            }

            if (!zsr_string_equal(
                    stack[depth - 1]->tag,
                    closing_tag
                )) {
                zsr_set_parse_error(
                    error,
                    "Closing tag does not match opening tag",
                    pos,
                    line,
                    column
                );
                return false;
            }

            --depth;
            continue;
        }

        /* Opening tag: <window ...> or self closing <text ... /> */
        zsr_node_t *node = zsr_new_node(document);

        if (!node) {
            zsr_set_parse_error(
                error,
                "Maximum number of ZML nodes reached",
                pos,
                line,
                column
            );
            return false;
        }

        if (!zsr_read_name(
                source,
                length,
                &pos,
                node->tag,
                sizeof(node->tag)
            )) {
            zsr_set_parse_error(
                error,
                "Invalid opening tag",
                pos,
                line,
                column
            );
            return false;
        }

        node->type = zsr_tag_type(node->tag);

        if (node->type == ZSR_NODE_UNKNOWN) {
            zsr_set_parse_error(
                error,
                "Unsupported ZML tag",
                pos,
                line,
                column
            );
            return false;
        }

        bool self_closing = false;
        bool tag_finished = false;

        while (pos < length && !tag_finished) {
            zsr_skip_spaces(source, length, &pos, &line, &column);

            if (pos >= length) {
                zsr_set_parse_error(
                    error,
                    "Unexpected end of file",
                    pos,
                    line,
                    column
                );
                return false;
            }

            if (source[pos] == '>') {
                ++pos;
                ++column;
                tag_finished = true;
                break;
            }

            if (source[pos] == '/' &&
                pos + 1 < length &&
                source[pos + 1] == '>') {
                pos += 2;
                column += 2;

                self_closing = true;
                tag_finished = true;
                break;
            }

            if (!zsr_parse_attribute(
                    source,
                    length,
                    &pos,
                    &line,
                    &column,
                    node,
                    error
                )) {
                return false;
            }
        }

        if (depth == 0) {
            if (*out_root) {
                zsr_set_parse_error(
                    error,
                    "Only one root element is allowed",
                    pos,
                    line,
                    column
                );
                return false;
            }

            *out_root = node;
        } else {
            zsr_add_child(stack[depth - 1], node);
        }

        if (!self_closing) {
            if (depth >= ZSR_MAX_DEPTH) {
                zsr_set_parse_error(
                    error,
                    "Maximum ZML nesting depth reached",
                    pos,
                    line,
                    column
                );
                return false;
            }

            stack[depth++] = node;
        }
    }

    if (!*out_root) {
        zsr_set_parse_error(
            error,
            "No root element found",
            pos,
            line,
            column
        );
        return false;
    }

    if (depth != 0) {
        zsr_set_parse_error(
            error,
            "Unclosed element",
            pos,
            line,
            column
        );
        return false;
    }

    return true;
}

static const char *zsr_attr(
    const zsr_node_t *node,
    const char *name
)
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
   8) DAMAGE TRACKER
   ============================================================ */

typedef struct {
    zsr_rect_t rects[ZSR_MAX_DAMAGE];
    uint32_t count;
    bool full_redraw;
} zsr_damage_t;

static void zsr_damage_reset(zsr_damage_t *damage)
{
    damage->count = 0;
    damage->full_redraw = false;
}

static void zsr_damage_add(
    zsr_damage_t *damage,
    zsr_rect_t rect
)
{
    if (zsr_rect_empty(rect) || damage->full_redraw)
        return;

    for (uint32_t i = 0; i < damage->count; ++i) {
        if (zsr_rect_near_or_intersects(damage->rects[i], rect, 12)) {
            damage->rects[i] = zsr_rect_union(
                damage->rects[i],
                rect
            );
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

static void zsr_invalidate_node(
    zsr_node_t *node,
    zsr_damage_t *damage
)
{
    zsr_damage_add(damage, node->old_bounds);
    zsr_damage_add(damage, node->bounds);

    node->needs_paint = true;
}

/* ============================================================
   9) SMART RENDER COMMANDS
   ============================================================ */

typedef enum {
    ZSR_CMD_FILL_RECT,
    ZSR_CMD_ROUNDED_RECT,
    ZSR_CMD_BORDER,
    ZSR_CMD_LINE,
    ZSR_CMD_TRIANGLE,
    ZSR_CMD_TEXT
} zsr_opcode_t;

typedef struct {
    zsr_opcode_t opcode;

    zsr_rect_t rect;
    zsr_rect_t clip;

    zsr_color_t color;
    zsr_color_t color2;

    int x0;
    int y0;
    int x1;
    int y1;

    int radius;
    int thickness;

    zsr_point_t a;
    zsr_point_t b;
    zsr_point_t c;

    char text[ZSR_MAX_TEXT];
    int text_scale;
} zsr_command_t;

typedef struct {
    zsr_command_t commands[ZSR_MAX_COMMANDS];
    uint32_t count;
} zsr_command_list_t;

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
        zsr_fill_rect_clipped(
            surface,
            command->rect,
            clip,
            command->color
        );
        break;

    case ZSR_CMD_ROUNDED_RECT:
        zsr_fill_rounded_rect_clipped(
            surface,
            command->rect,
            command->radius,
            clip,
            command->color
        );
        break;

    case ZSR_CMD_BORDER:
        zsr_draw_border_clipped(
            surface,
            command->rect,
            command->radius,
            command->thickness,
            clip,
            command->color
        );
        break;

    case ZSR_CMD_LINE:
        zsr_draw_line_clipped(
            surface,
            command->x0,
            command->y0,
            command->x1,
            command->y1,
            command->thickness,
            clip,
            command->color
        );
        break;

    case ZSR_CMD_TRIANGLE:
        zsr_fill_triangle_clipped(
            surface,
            command->a,
            command->b,
            command->c,
            clip,
            command->color
        );
        break;

    case ZSR_CMD_TEXT:
        zsr_draw_fake_text_clipped(
            surface,
            command->rect.x,
            command->rect.y,
            command->text,
            command->text_scale,
            clip,
            command->color
        );
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
        zsr_execute_command(
            surface,
            &list->commands[i],
            region
        );
    }
}

/* ============================================================
   10) ZARQAA BACKGROUND GENERATION
   ============================================================ */

static void zsr_emit_background(
    zsr_command_list_t *list,
    zsr_surface_t *surface
)
{
    zsr_command_t *base = zsr_command_push(
        list,
        ZSR_CMD_FILL_RECT
    );

    if (!base)
        return;

    base->rect = zsr_rect_make(
        0,
        0,
        surface->width,
        surface->height
    );

    base->clip = base->rect;
    base->color = ZSR_RGBA(15, 23, 42, 255);
}

/*
 * glow مرسوم مباشرة في المنطقة المتضررة لأنه تدرج شعاعي.
 * لاحقًا: ارسم الخلفية إلى wallpaper_surface مرة واحدة
 * ثم انسخ فقط region المطلوب إلى backbuffer.
 */
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

static void zsr_draw_zarqaa_background_region(
    zsr_surface_t *surface,
    zsr_rect_t region
)
{
    zsr_fill_rect_clipped(
        surface,
        zsr_rect_make(0, 0, surface->width, surface->height),
        region,
        ZSR_RGBA(15, 23, 42, 255)
    );

    int screen_w = surface->width;
    int screen_h = surface->height;

    zsr_draw_glow_region(
        surface,
        region,
        screen_w * 10 / 100,
        screen_h * 20 / 100,
        screen_w * 45 / 100,
        ZSR_RGBA(37, 99, 235, 205)
    );

    zsr_draw_glow_region(
        surface,
        region,
        screen_w * 90 / 100,
        screen_h * 40 / 100,
        screen_w * 50 / 100,
        ZSR_RGBA(219, 39, 119, 150)
    );

    zsr_draw_glow_region(
        surface,
        region,
        screen_w * 80 / 100,
        screen_h * 90 / 100,
        screen_w * 40 / 100,
        ZSR_RGBA(245, 158, 11, 120)
    );

    zsr_draw_glow_region(
        surface,
        region,
        screen_w * 20 / 100,
        screen_h * 80 / 100,
        screen_w * 50 / 100,
        ZSR_RGBA(79, 70, 229, 150)
    );
}

/* ============================================================
   11) BUILD ZARQAA WINDOW DISPLAY LIST
   ============================================================ */

static void zsr_emit_rounded_rect(
    zsr_command_list_t *list,
    zsr_rect_t rect,
    int radius,
    zsr_rect_t clip,
    zsr_color_t color
)
{
    zsr_command_t *command = zsr_command_push(
        list,
        ZSR_CMD_ROUNDED_RECT
    );

    if (!command)
        return;

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
    zsr_command_t *command = zsr_command_push(
        list,
        ZSR_CMD_BORDER
    );

    if (!command)
        return;

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
    zsr_command_t *command = zsr_command_push(
        list,
        ZSR_CMD_LINE
    );

    if (!command)
        return;

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
    zsr_command_t *command = zsr_command_push(
        list,
        ZSR_CMD_TRIANGLE
    );

    if (!command)
        return;

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
    zsr_command_t *command = zsr_command_push(
        list,
        ZSR_CMD_TEXT
    );

    if (!command)
        return;

    command->rect = zsr_rect_make(x, y, 0, 0);
    command->clip = clip;
    command->color = color;
    command->text_scale = scale;

    if (!text)
        text = "";

    uint32_t length = 0;

    while (text[length] && length < ZSR_MAX_TEXT - 1)
        ++length;

    zsr_copy_string(
        command->text,
        sizeof(command->text),
        text,
        length
    );
}

static void zsr_build_zarqaa_window(
    zsr_command_list_t *list,
    zsr_rect_t frame
)
{
    zsr_rect_t screen_clip = zsr_rect_make(
        0,
        0,
        32767,
        32767
    );

    zsr_rect_t shadow = zsr_rect_make(
        frame.x,
        frame.y + 16,
        frame.width,
        frame.height
    );

    /* Shadow */
    zsr_emit_rounded_rect(
        list,
        shadow,
        18,
        screen_clip,
        ZSR_RGBA(0, 0, 0, 110)
    );

    /* Glass window */
    zsr_emit_rounded_rect(
        list,
        frame,
        16,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 34)
    );

    /* White border */
    zsr_emit_border(
        list,
        frame,
        16,
        1,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 58)
    );

    /* Top highlight */
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

    /* Titlebar separator */
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

    /* OS title */
    zsr_emit_text(
        list,
        frame.x + 46,
        frame.y + 16,
        "Zarqaa OS",
        1,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 240)
    );

    /* Workspace title */
    zsr_emit_text(
        list,
        frame.x + frame.width / 2 - 35,
        frame.y + 17,
        "Workspace 1",
        1,
        screen_clip,
        ZSR_RGBA(230, 240, 255, 190)
    );

    /* Star / logo */
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

    /* Window controls */
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

    /* Tabs */
    const char *tabs[] = {
        "Desktop",
        "Apps",
        "Files",
        "Settings"
    };

    int tab_x = frame.x + 24;
    int tab_y = frame.y + 56;

    for (int i = 0; i < 4; ++i) {
        zsr_rect_t tab_rect = zsr_rect_make(
            tab_x,
            tab_y,
            74,
            28
        );

        if (i == 0) {
            zsr_emit_rounded_rect(
                list,
                tab_rect,
                14,
                screen_clip,
                ZSR_RGBA(0, 0, 0, 78)
            );
        }

        zsr_emit_text(
            list,
            tab_x + 10,
            tab_y + 9,
            tabs[i],
            1,
            screen_clip,
            i == 0
                ? ZSR_RGBA(255, 255, 255, 250)
                : ZSR_RGBA(255, 255, 255, 165)
        );

        tab_x += 82;
    }

    /* Welcome area */
    int center_x = frame.x + frame.width / 2;
    int center_y = frame.y + frame.height / 2 - 45;

    zsr_emit_triangle(
        list,
        (zsr_point_t){center_x, center_y - 32},
        (zsr_point_t){center_x + 13, center_y + 6},
        (zsr_point_t){center_x - 13, center_y + 6},
        screen_clip,
        ZSR_RGBA(255, 255, 255, 225)
    );

    zsr_emit_triangle(
        list,
        (zsr_point_t){center_x, center_y + 32},
        (zsr_point_t){center_x + 13, center_y - 6},
        (zsr_point_t){center_x - 13, center_y - 6},
        screen_clip,
        ZSR_RGBA(255, 255, 255, 225)
    );

    zsr_emit_text(
        list,
        center_x - 65,
        center_y + 48,
        "Welcome to Zarqaa",
        2,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 250)
    );

    zsr_emit_text(
        list,
        center_x - 55,
        center_y + 78,
        "Fast. Simple. Yours.",
        1,
        screen_clip,
        ZSR_RGBA(219, 234, 254, 165)
    );

    /* Bottom status */
    int bottom_y = frame.y + frame.height - 35;

    zsr_emit_rounded_rect(
        list,
        zsr_rect_make(frame.x + 22, bottom_y, 8, 8),
        4,
        screen_clip,
        ZSR_RGBA(74, 222, 128, 255)
    );

    zsr_emit_text(
        list,
        frame.x + 38,
        bottom_y - 1,
        "Ready",
        1,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 185)
    );

    /* Floating dock */
    zsr_rect_t dock = zsr_rect_make(
        frame.x + (frame.width - 242) / 2,
        frame.y + frame.height - 82,
        242,
        64
    );

    zsr_emit_rounded_rect(
        list,
        dock,
        16,
        screen_clip,
        ZSR_RGBA(0, 0, 0, 62)
    );

    zsr_emit_border(
        list,
        dock,
        16,
        1,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 26)
    );

    zsr_color_t dock_colors[4] = {
        ZSR_RGBA(59, 130, 246, 100),
        ZSR_RGBA(168, 85, 247, 100),
        ZSR_RGBA(249, 115, 22, 100),
        ZSR_RGBA(100, 116, 139, 100)
    };

    for (int i = 0; i < 4; ++i) {
        zsr_rect_t icon = zsr_rect_make(
            dock.x + 11 + i * 57,
            dock.y + 10,
            44,
            44
        );

        zsr_emit_rounded_rect(
            list,
            icon,
            12,
            screen_clip,
            dock_colors[i]
        );

        zsr_emit_border(
            list,
            icon,
            12,
            1,
            screen_clip,
            ZSR_RGBA(255, 255, 255, 22)
        );
    }

    /* System tray */
    zsr_emit_text(
        list,
        frame.x + frame.width - 100,
        bottom_y - 1,
        "12:30 PM",
        1,
        screen_clip,
        ZSR_RGBA(255, 255, 255, 185)
    );
}

/* ============================================================
   12) WINDOW INTERACTION
   ============================================================ */

typedef struct {
    zsr_rect_t bounds;
    zsr_rect_t previous_bounds;

    bool dragging;
    bool resize_right;
    bool resize_bottom;

    int drag_offset_x;
    int drag_offset_y;

    int start_mouse_x;
    int start_mouse_y;
    int start_width;
    int start_height;
} zsr_window_state_t;

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

static bool zsr_point_in_rect(
    int x,
    int y,
    zsr_rect_t rect
)
{
    return x >= rect.x &&
           y >= rect.y &&
           x < rect.x + rect.width &&
           y < rect.y + rect.height;
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

    window->resize_right =
        mouse_x >= r.x + r.width - ZSR_RESIZE_MARGIN;

    window->resize_bottom =
        mouse_y >= r.y + r.height - ZSR_RESIZE_MARGIN;

    if (!window->resize_right &&
        !window->resize_bottom &&
        mouse_y >= r.y &&
        mouse_y < r.y + ZSR_TITLEBAR_HEIGHT) {

        window->dragging = true;
        window->drag_offset_x = mouse_x - r.x;
        window->drag_offset_y = mouse_y - r.y;
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
        int new_width =
            window->start_width +
            (mouse_x - window->start_mouse_x);

        if (new_width < ZSR_MIN_WINDOW_WIDTH)
            new_width = ZSR_MIN_WINDOW_WIDTH;

        window->bounds.width = new_width;
    }

    if (window->resize_bottom) {
        int new_height =
            window->start_height +
            (mouse_y - window->start_mouse_y);

        if (new_height < ZSR_MIN_WINDOW_HEIGHT)
            new_height = ZSR_MIN_WINDOW_HEIGHT;

        window->bounds.height = new_height;
    }

    /*
     * الأهم:
     * نضيف المكان القديم والجديد للضرر.
     * المكان القديم يحتاج الخلفية من جديد.
     * المكان الجديد يحتاج رسم النافذة.
     */
    zsr_damage_add(damage, window->previous_bounds);
    zsr_damage_add(damage, window->bounds);
}

static void zsr_window_mouse_up(zsr_window_state_t *window)
{
    window->dragging = false;
    window->resize_right = false;
    window->resize_bottom = false;
}

/* ============================================================
   13) SMART COMPOSITOR
   ============================================================ */

typedef struct {
    zsr_surface_t *backbuffer;

    zsr_window_state_t main_window;

    zsr_damage_t damage;
    zsr_command_list_t commands;
} zsr_compositor_t;

static void zsr_compositor_init(
    zsr_compositor_t *compositor,
    zsr_surface_t *backbuffer
)
{
    compositor->backbuffer = backbuffer;

    zsr_window_init(
        &compositor->main_window,
        backbuffer->width,
        backbuffer->height
    );

    zsr_damage_reset(&compositor->damage);

    zsr_damage_add(
        &compositor->damage,
        zsr_rect_make(
            0,
            0,
            backbuffer->width,
            backbuffer->height
        )
    );
}

static void zsr_compositor_build_commands(
    zsr_compositor_t *compositor
)
{
    zsr_commands_reset(&compositor->commands);

    /*
     * الخلفية نفسها ستُرسم مباشرة في region
     * لأنها radial glows. في النسخة المتقدمة:
     *
     * wallpaper_surface -> cached once
     * ثم blit region فقط.
     */

    zsr_build_zarqaa_window(
        &compositor->commands,
        compositor->main_window.bounds
    );
}

static void zsr_compositor_render_region(
    zsr_compositor_t *compositor,
    zsr_rect_t region
)
{
    zsr_surface_t *surface = compositor->backbuffer;

    zsr_rect_t screen = zsr_rect_make(
        0,
        0,
        surface->width,
        surface->height
    );

    region = zsr_rect_clip(region, screen);

    if (zsr_rect_empty(region))
        return;

    /*
     * 1. استعادة الخلفية فقط في المنطقة المتضررة.
     * 2. نفذ قائمة الأوامر ولكن مع clip=region.
     * 3. لا يتم لمس بقية الشاشة.
     */

    zsr_draw_zarqaa_background_region(surface, region);

    zsr_execute_region(
        surface,
        &compositor->commands,
        region
    );
}

static void zsr_compositor_frame(
    zsr_compositor_t *compositor
)
{
    zsr_damage_t *damage = &compositor->damage;
    zsr_surface_t *surface = compositor->backbuffer;

    if (damage->count == 0 && !damage->full_redraw)
        return;

    zsr_compositor_build_commands(compositor);

    if (damage->full_redraw) {
        zsr_compositor_render_region(
            compositor,
            zsr_rect_make(
                0,
                0,
                surface->width,
                surface->height
            )
        );
    } else {
        for (uint32_t i = 0; i < damage->count; ++i) {
            zsr_compositor_render_region(
                compositor,
                damage->rects[i]
            );
        }
    }

    zsr_present(surface);
    zsr_damage_reset(damage);
}

/* ============================================================
   14) OPTIONAL ZML DEMO DOCUMENT
   ============================================================ */

/*
 * هذا الملف يوضح شكل ZML الذي يفهمه parser الحالي.
 * في هذا الإصدار، الـ renderer يرسم تصميم Zarqaa ثابتًا،
 * لكن parser جاهز كي تربط كل node لاحقًا بالـ render builder.
 */

static const char zsr_demo_zml[] =
    "<desktop background=\"#0F172A\">"
    "  <window id=\"zarqaa-main\""
    "          width=\"900\""
    "          height=\"600\""
    "          radius=\"16\""
    "          material=\"glass\""
    "          opacity=\"0.14\">"
    "    <titlebar height=\"44\">"
    "      <text value=\"Zarqaa OS\" />"
    "    </titlebar>"
    "    <tabs>"
    "      <tab title=\"Desktop\" active=\"true\" />"
    "      <tab title=\"Apps\" />"
    "      <tab title=\"Files\" />"
    "      <tab title=\"Settings\" />"
    "    </tabs>"
    "    <column>"
    "      <text value=\"Welcome to Zarqaa\" size=\"30\" />"
    "      <text value=\"Fast. Simple. Yours.\" size=\"13\" />"
    "    </column>"
    "    <dock>"
    "      <dock-item icon=\"folder\" />"
    "      <dock-item icon=\"globe\" />"
    "      <dock-item icon=\"terminal\" />"
    "      <dock-item icon=\"gear\" />"
    "    </dock>"
    "  </window>"
    "</desktop>";

/* ============================================================
   15) PUBLIC API FOR ZARQAA OS
   ============================================================ */

/*
 * استدعها مرة بعد تهيئة framebuffer/backbuffer:
 *
 * zsr_compositor_t zarqaa_ui;
 *
 * zsr_init(&zarqaa_ui, fb_get_backbuffer());
 *
 * داخل main loop:
 *
 * zsr_handle_mouse_move(&zarqaa_ui, mouse_x, mouse_y);
 * zsr_render(&zarqaa_ui);
 */

void zsr_init(
    zsr_compositor_t *compositor,
    zsr_surface_t *backbuffer
)
{
    zsr_compositor_init(compositor, backbuffer);

    /*
     * اختبار parser اختياري:
     * لا يؤثر على الـ renderer الحالي.
     */
    zsr_document_t document;
    zsr_node_t *root;
    zsr_parse_error_t error;

    (void)zsr_parse_zml(
        zsr_demo_zml,
        sizeof(zsr_demo_zml) - 1,
        &document,
        &root,
        &error
    );
}

void zsr_render(zsr_compositor_t *compositor)
{
    zsr_compositor_frame(compositor);
}

void zsr_handle_mouse_down(
    zsr_compositor_t *compositor,
    int mouse_x,
    int mouse_y
)
{
    zsr_window_mouse_down(
        &compositor->main_window,
        mouse_x,
        mouse_y
    );
}

void zsr_handle_mouse_move(
    zsr_compositor_t *compositor,
    int mouse_x,
    int mouse_y
)
{
    zsr_window_mouse_move(
        &compositor->main_window,
        mouse_x,
        mouse_y,
        &compositor->damage
    );
}

void zsr_handle_mouse_up(zsr_compositor_t *compositor)
{
    zsr_window_mouse_up(
        &compositor->main_window
    );
}

/*
 * عند تغيير theme، فتح نافذة، إغلاق نافذة، أو تغيير محتوى:
 *
 * zsr_request_full_redraw(&zarqaa_ui);
 */
void zsr_request_full_redraw(zsr_compositor_t *compositor)
{
    compositor->damage.count = 0;
    compositor->damage.full_redraw = true;
}