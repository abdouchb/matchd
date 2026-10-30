/*
 * Zarqaa OS - ZML Smart Renderer Public Header
 * --------------------------------------------
 * Official Public API, Enums, Structs, and Types
 */

#ifndef GUI_ZML_SMART_RENDERER_H
#define GUI_ZML_SMART_RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include "zarqaa/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
   1) CONFIGURATION CONSTANTS & MACROS
   ============================================================ */

#define ZSR_MAX_NODES            1024
#define ZSR_MAX_ATTRS            12
#define ZSR_MAX_DEPTH            24
#define ZSR_MAX_TAG              24
#define ZSR_MAX_ATTR_NAME        24
#define ZSR_MAX_ATTR_VALUE       96
#define ZSR_MAX_DAMAGE           32
#define ZSR_MAX_COMMANDS         4096
#define ZSR_MAX_TEXT             96

#define ZSR_MIN_WINDOW_WIDTH     500
#define ZSR_MIN_WINDOW_HEIGHT    400

#define ZSR_TITLEBAR_HEIGHT      44
#define ZSR_RESIZE_MARGIN        8

#define ZSR_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | \
     ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) << 8)  | \
     ((uint32_t)(b)))

#define ZSR_RED(c)   (((c) >> 16) & 0xFF)
#define ZSR_GREEN(c) (((c) >> 8) & 0xFF)
#define ZSR_BLUE(c)  ((c) & 0xFF)
#define ZSR_ALPHA(c) (((c) >> 24) & 0xFF)

/* ============================================================
   2) BASIC TYPES & GEOMETRY
   ============================================================ */

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

/* ============================================================
   3) NODE TYPES & SCENE TREE
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
    ZSR_NODE_STATUS_DOT,
    ZSR_NODE_APP_SURFACE
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

/* ============================================================
   4) DAMAGE TRACKER
   ============================================================ */

typedef struct {
    zsr_rect_t rects[ZSR_MAX_DAMAGE];
    uint32_t count;
    bool full_redraw;
} zsr_damage_t;

/* ============================================================
   5) COMMAND LIST & OPCODES
   ============================================================ */

typedef enum {
    ZSR_CMD_FILL_RECT,
    ZSR_CMD_ROUNDED_RECT,
    ZSR_CMD_BORDER,
    ZSR_CMD_LINE,
    ZSR_CMD_TRIANGLE,
    ZSR_CMD_TEXT,
    ZSR_CMD_BLIT_BUFFER
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

    uint32_t *buffer;
    int buffer_width;
    int buffer_height;
    int buffer_pitch;
} zsr_command_t;

typedef struct {
    zsr_command_t commands[ZSR_MAX_COMMANDS];
    uint32_t count;
} zsr_command_list_t;

/* ============================================================
   6) WINDOW STATE & COMPOSITOR
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

typedef struct {
    zsr_surface_t *backbuffer;

    zsr_window_state_t main_window;

    zsr_damage_t damage;
    zsr_command_list_t commands;

    zsr_document_t document;
    zsr_node_t *root;
    zsr_parse_error_t error;
} zsr_compositor_t;

/* ============================================================
   7) PUBLIC FUNCTION PROTOTYPES
   ============================================================ */

void zsr_present(zsr_surface_t *surface);

void zsr_init(
    zsr_compositor_t *compositor,
    zsr_surface_t *backbuffer
);

void zsr_render(zsr_compositor_t *compositor);

void zsr_handle_mouse_down(
    zsr_compositor_t *compositor,
    int mouse_x,
    int mouse_y
);

void zsr_handle_mouse_move(
    zsr_compositor_t *compositor,
    int mouse_x,
    int mouse_y
);

void zsr_handle_mouse_up(zsr_compositor_t *compositor);

void zsr_request_full_redraw(zsr_compositor_t *compositor);

zsr_compositor_t *zsr_get_desktop_compositor(void);

void zsr_add_window(
    zsr_compositor_t *compositor,
    const char *title,
    int x, int y, int width, int height,
    uint32_t *buffer, int buffer_pitch
);

#ifdef __cplusplus
}
#endif

#endif /* GUI_ZML_SMART_RENDERER_H */