/*
 * Zarqaa OS - Unified Graphics API (Phase 1)
 */

#include "graphics/graphics_api.h"
#include "graphics/render_queue.h"
#include "drivers/framebuffer.h"
#include "drivers/font.h"
#include "zarqaa/string.h"

void gapi_context_init(render_context_t *ctx) {
    if (!ctx) return;
    ctx->queue = NULL;
    ctx->clip.x = 0;
    ctx->clip.y = 0;
    ctx->clip.width = 0;
    ctx->clip.height = 0;
    
    // Identity transform
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            ctx->transform.m[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    
    ctx->color = 0xFFFFFFFF;
    ctx->font = NULL;
    ctx->opacity = 1.0f;
    ctx->flags = 0;
}

static render_context_t *g_active_context = NULL;

void gapi_set_current_context(render_context_t *ctx) {
    g_active_context = ctx;
}

/* Immediate mode: draw straight into the current render target instead of queueing */
static bool g_immediate = false;
void gapi_set_immediate(bool on) { g_immediate = on; }

render_context_t *gapi_get_current_context(void) {
    return g_active_context;
}

void gapi_fill_rect(render_context_t *ctx, int x, int y, int w, int h, color_t color) {
    if (g_immediate) { fb_fill_rect_internal(x, y, w, h, color); return; }
    if (!ctx) ctx = g_active_context;
    RenderQueue *q = ctx ? ctx->queue : &g_default_render_queue;
    RenderCommand *cmd = render_queue_push(q);
    if (!cmd) return;
    cmd->opcode = RENDER_OP_FILL_RECT;
    int tx = ctx ? (int)ctx->transform.m[0][2] : 0;
    int ty = ctx ? (int)ctx->transform.m[1][2] : 0;
    cmd->rect.x = x + tx; cmd->rect.y = y + ty; cmd->rect.w = w; cmd->rect.h = h;
    cmd->rect.color = color;
}

void gapi_draw_rect(render_context_t *ctx, int x, int y, int w, int h, color_t color) {
    if (g_immediate) { fb_draw_rect_internal(x, y, w, h, color); return; }
    if (!ctx) ctx = g_active_context;
    RenderQueue *q = ctx ? ctx->queue : &g_default_render_queue;
    RenderCommand *cmd = render_queue_push(q);
    if (!cmd) return;
    cmd->opcode = RENDER_OP_DRAW_RECT;
    int tx = ctx ? (int)ctx->transform.m[0][2] : 0;
    int ty = ctx ? (int)ctx->transform.m[1][2] : 0;
    cmd->rect.x = x + tx; cmd->rect.y = y + ty; cmd->rect.w = w; cmd->rect.h = h;
    cmd->rect.color = color;
}

void gapi_draw_line(render_context_t *ctx, int x0, int y0, int x1, int y1, color_t color) {
    if (g_immediate) { fb_draw_line_internal(x0, y0, x1, y1, color); return; }
    if (!ctx) ctx = g_active_context;
    RenderQueue *q = ctx ? ctx->queue : &g_default_render_queue;
    RenderCommand *cmd = render_queue_push(q);
    if (!cmd) return;
    cmd->opcode = RENDER_OP_DRAW_LINE;
    int tx = ctx ? (int)ctx->transform.m[0][2] : 0;
    int ty = ctx ? (int)ctx->transform.m[1][2] : 0;
    cmd->line.x0 = x0 + tx; cmd->line.y0 = y0 + ty; cmd->line.x1 = x1 + tx; cmd->line.y1 = y1 + ty;
    cmd->line.color = color;
}

void gapi_draw_string(render_context_t *ctx, int x, int y, const char *s, color_t fg, color_t bg) {
    if (g_immediate) { fb_draw_string_aa_clipped(x, y, s, fg, NULL); (void)bg; return; }
    if (!ctx) ctx = g_active_context;
    RenderQueue *q = ctx ? ctx->queue : &g_default_render_queue;
    RenderCommand *cmd = render_queue_push(q);
    if (!cmd) return;
    cmd->opcode = RENDER_OP_DRAW_STRING;
    int tx = ctx ? (int)ctx->transform.m[0][2] : 0;
    int ty = ctx ? (int)ctx->transform.m[1][2] : 0;
    cmd->text.x = x + tx; cmd->text.y = y + ty;
    cmd->text.fg = fg; cmd->text.bg = bg;
    strncpy(cmd->text.text, s, sizeof(cmd->text.text) - 1);
    cmd->text.text[sizeof(cmd->text.text) - 1] = '\0';
}

void gapi_draw_string_transparent(render_context_t *ctx, int x, int y, const char *s, color_t fg) {
    if (g_immediate) { fb_draw_string_aa_clipped(x, y, s, fg, NULL); return; }
    if (!ctx) ctx = g_active_context;
    RenderQueue *q = ctx ? ctx->queue : &g_default_render_queue;
    RenderCommand *cmd = render_queue_push(q);
    if (!cmd) return;
    cmd->opcode = RENDER_OP_DRAW_STRING_TRANS;
    int tx = ctx ? (int)ctx->transform.m[0][2] : 0;
    int ty = ctx ? (int)ctx->transform.m[1][2] : 0;
    cmd->text.x = x + tx; cmd->text.y = y + ty;
    cmd->text.fg = fg;
    strncpy(cmd->text.text, s, sizeof(cmd->text.text) - 1);
    cmd->text.text[sizeof(cmd->text.text) - 1] = '\0';
}

void gapi_draw_image(render_context_t *ctx, int x, int y, int w, int h, uint32_t *pixels) {
    if (g_immediate) {
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                uint32_t px = pixels[j * w + i];
                uint8_t a = px >> 24;
                if (a == 255) fb_putpixel(x + i, y + j, px);
                else if (a) fb_putpixel(x + i, y + j, ((px & 0xFF) * a + (fb_getpixel(x + i, y + j) & 0xFF) * (255 - a)) / 255
                    | ((((px >> 8) & 0xFF) * a + ((fb_getpixel(x + i, y + j) >> 8) & 0xFF) * (255 - a)) / 255) << 8
                    | ((((px >> 16) & 0xFF) * a + ((fb_getpixel(x + i, y + j) >> 16) & 0xFF) * (255 - a)) / 255) << 16);
            }
        return;
    }
    if (!ctx) ctx = g_active_context;
    RenderQueue *q = ctx ? ctx->queue : &g_default_render_queue;
    RenderCommand *cmd = render_queue_push(q);
    if (!cmd) return;
    cmd->opcode = RENDER_OP_DRAW_IMAGE;
    int tx = ctx ? (int)ctx->transform.m[0][2] : 0;
    int ty = ctx ? (int)ctx->transform.m[1][2] : 0;
    cmd->image.x = x + tx; cmd->image.y = y + ty; cmd->image.w = w; cmd->image.h = h;
    cmd->image.pixels = pixels;
}
