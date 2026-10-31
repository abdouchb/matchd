/*
 * Zarqaa OS - GPU Hardware Renderer Backend
 */

#include "graphics/renderer.h"
#include "drivers/gpu.h"
#include "drivers/framebuffer.h"

extern renderer_backend_t g_software_renderer;

/* Forward declaration for text rendering fallback */
extern void fb_draw_string_internal(int x, int y, const char *s, color_t fg, color_t bg);
extern void fb_draw_string_transparent_internal(int x, int y, const char *s, color_t fg);
extern void fb_draw_rect_internal(int x, int y, int w, int h, color_t color);
extern void fb_draw_line_internal(int x0, int y0, int x1, int y1, color_t color);

extern rect_t fb_clip_rect;

static bool hw_begin_frame(void) {
    return true; /* Always ready */
}

static void hw_execute_queue(RenderQueue *queue) {
    if (!queue || !queue->head) return;
    
    uint32_t *backbuffer = fb_get_backbuffer();
    int pitch = fb_get_pitch();
    
    RenderQueueChunk *curr = queue->head;
    while (curr) {
        for (int i = 0; i < curr->count; i++) {
            RenderCommand *cmd = &curr->commands[i];
            
            // Resolve clipping
            rect_t clip = cmd->clip.width > 0 ? cmd->clip : fb_clip_rect;
            
            switch (cmd->opcode) {
                case RENDER_OP_FILL_RECT: {
                    int x = cmd->rect.x;
                    int y = cmd->rect.y;
                    int w = cmd->rect.w;
                    int h = cmd->rect.h;
                    
                    // Hardware clip bounds
                    if (clip.width > 0) {
                        int cx1 = clip.x, cy1 = clip.y;
                        int cx2 = cx1 + clip.width, cy2 = cy1 + clip.height;
                        int x1 = x, y1 = y, x2 = x + w, y2 = y + h;
                        
                        if (x1 < cx1) x1 = cx1;
                        if (y1 < cy1) y1 = cy1;
                        if (x2 > cx2) x2 = cx2;
                        if (y2 > cy2) y2 = cy2;
                        
                        if (x1 >= x2 || y1 >= y2) continue; // Clipped
                        
                        x = x1;
                        y = y1;
                        w = x2 - x1;
                        h = y2 - y1;
                    }
                    gpu_hal_fill_rect(backbuffer, pitch, x, y, w, h, cmd->rect.color);
                    break;
                }
                case RENDER_OP_DRAW_LINE:
                    fb_draw_line_internal(cmd->line.x0, cmd->line.y0, cmd->line.x1, cmd->line.y1, cmd->line.color);
                    break;
                case RENDER_OP_DRAW_RECT:
                    fb_draw_rect_internal(cmd->rect.x, cmd->rect.y, cmd->rect.w, cmd->rect.h, cmd->rect.color);
                    break;
                case RENDER_OP_DRAW_STRING:
                    fb_draw_string_internal(cmd->text.x, cmd->text.y, cmd->text.text, cmd->text.fg, cmd->text.bg);
                    break;
                case RENDER_OP_DRAW_STRING_TRANS:
                    fb_draw_string_transparent_internal(cmd->text.x, cmd->text.y, cmd->text.text, cmd->text.fg);
                    break;
                case RENDER_OP_DRAW_IMAGE: {
                    int px = cmd->image.x, py = cmd->image.y;
                    int pw = cmd->image.w, ph = cmd->image.h;
                    // Apply clipping to image dimensions if needed
                    if (clip.width > 0) {
                        int cx1 = clip.x, cy1 = clip.y;
                        int cx2 = cx1 + clip.width, cy2 = cy1 + clip.height;
                        int x1 = px, y1 = py, x2 = px + pw, y2 = py + ph;
                        if (x1 < cx1) x1 = cx1;
                        if (y1 < cy1) y1 = cy1;
                        if (x2 > cx2) x2 = cx2;
                        if (y2 > cy2) y2 = cy2;
                        if (x1 >= x2 || y1 >= y2) break; // Clipped
                        
                        // We do a simple loop for clipping instead of modifying image pointer to keep it simple,
                        // or just fall back to a software blit for now.
                        for (int sy = y1; sy < y2; sy++) {
                            for (int sx = x1; sx < x2; sx++) {
                                uint32_t c = cmd->image.pixels[(sy - py) * pw + (sx - px)];
                                if (c & 0xFF000000) { // Simple alpha test
                                    fb_putpixel(sx, sy, c);
                                }
                            }
                        }
                    } else {
                        // Fast path without hardware clip
                        for (int sy = 0; sy < ph; sy++) {
                            for (int sx = 0; sx < pw; sx++) {
                                uint32_t c = cmd->image.pixels[sy * pw + sx];
                                if (c & 0xFF000000) {
                                    fb_putpixel(px + sx, py + sy, c);
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
        curr = curr->next;
    }
}

static void hw_end_frame(void) {
    // Optional GPU flush
}

renderer_backend_t g_hardware_renderer = {
    .begin_frame = hw_begin_frame,
    .execute_queue = hw_execute_queue,
    .end_frame = hw_end_frame,
    .present = NULL
};
