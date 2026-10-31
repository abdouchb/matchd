/*
 * Zarqaa OS - Render Queue (Phase 2)
 */

#ifndef GRAPHICS_RENDER_QUEUE_H
#define GRAPHICS_RENDER_QUEUE_H

#include "zarqaa/types.h"
#include "graphics/graphics_api.h"

/* Opcodes for RenderCommands */
#define RENDER_OP_NONE                  0
#define RENDER_OP_FILL_RECT             1
#define RENDER_OP_DRAW_RECT             2
#define RENDER_OP_DRAW_LINE             3
#define RENDER_OP_DRAW_STRING           4
#define RENDER_OP_DRAW_STRING_TRANS     5
#define RENDER_OP_DRAW_IMAGE            6

#define CHUNK_SIZE 256

typedef struct {
    int x, y, w, h;
    color_t color;
} RectCommand;

typedef struct {
    int x0, y0, x1, y1;
    color_t color;
} LineCommand;

typedef struct {
    int x, y;
    char text[64]; /* Simplified for now: fixed buffer for string commands */
    color_t fg;
    color_t bg;
} TextCommand;

typedef struct {
    int x, y, w, h;
    uint32_t *pixels;
} ImageCommand;

typedef struct RenderCommand {
    uint16_t opcode;
    uint16_t layer;
    uint32_t flags;
    rect_t clip;
    matrix_t transform;
    union {
        RectCommand rect;
        LineCommand line;
        TextCommand text;
        ImageCommand image;
    };
} RenderCommand;

typedef struct RenderQueueChunk {
    RenderCommand commands[CHUNK_SIZE];
    int count;
    struct RenderQueueChunk *next;
} RenderQueueChunk;

typedef struct RenderQueue {
    RenderQueueChunk *head;
    RenderQueueChunk *tail;
    int total_commands;
} RenderQueue;

/* Core Queue Functions */
void render_queue_init(RenderQueue *queue);
void render_queue_destroy(RenderQueue *queue);
RenderCommand* render_queue_push(RenderQueue *queue);
void render_queue_clear(RenderQueue *queue);

/* Global default queue for Phase 2 compatibility */
extern RenderQueue g_default_render_queue;

#endif /* GRAPHICS_RENDER_QUEUE_H */
