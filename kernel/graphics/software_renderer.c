#include "graphics/renderer.h"
#include "drivers/framebuffer.h"

renderer_backend_t *g_current_renderer = &g_software_renderer;

void renderer_set_backend(renderer_backend_t *backend) {
    if (backend) {
        g_current_renderer = backend;
    }
}

/* Software Renderer Implementation */

static bool soft_begin_frame(void) {
    return true; /* Always ready */
}

static void soft_execute_queue(RenderQueue *queue) {
    if (!queue || !queue->head) return;
    
    RenderQueueChunk *curr = queue->head;
    while (curr) {
        for (int i = 0; i < curr->count; i++) {
            RenderCommand *cmd = &curr->commands[i];
            
            /* TODO: Apply cmd->clip and cmd->transform to the graphics context here */
            
            switch (cmd->opcode) {
                case RENDER_OP_FILL_RECT:
                    fb_fill_rect_internal(cmd->rect.x, cmd->rect.y, cmd->rect.w, cmd->rect.h, cmd->rect.color);
                    break;
                case RENDER_OP_DRAW_RECT:
                    fb_draw_rect_internal(cmd->rect.x, cmd->rect.y, cmd->rect.w, cmd->rect.h, cmd->rect.color);
                    break;
                case RENDER_OP_DRAW_LINE:
                    fb_draw_line_internal(cmd->line.x0, cmd->line.y0, cmd->line.x1, cmd->line.y1, cmd->line.color);
                    break;
                case RENDER_OP_DRAW_STRING:
                    fb_draw_string_internal(cmd->text.x, cmd->text.y, cmd->text.text, cmd->text.fg, cmd->text.bg);
                    break;
                case RENDER_OP_DRAW_STRING_TRANS:
                    fb_draw_string_transparent_internal(cmd->text.x, cmd->text.y, cmd->text.text, cmd->text.fg);
                    break;
                default:
                    /* Ignored */
                    break;
            }
        }
        curr = curr->next;
    }
}

static void soft_end_frame(void) {
    /* Nothing to do for software */
}

renderer_backend_t g_software_renderer = {
    .begin_frame = soft_begin_frame,
    .execute_queue = soft_execute_queue,
    .end_frame = soft_end_frame,
    .present = NULL
};
