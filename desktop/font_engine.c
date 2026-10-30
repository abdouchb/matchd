/*
 * Zarqaa OS 0.1 - Font Engine
 *
 * Provides text rendering using gpu_renderer primitives.
 */

#include "desktop/font_engine.h"
#include "drivers/gpu_renderer.h"
#include "drivers/font_atlas.h"

void font_engine_init(void) {
    /* Fonts are preloaded in font_atlas.c. Nothing to initialize for now. */
}

void font_engine_draw_string(int x, int y, const char *text, uint32_t color) {
    if (!text) return;
    
    int cx = x;
    int cy = y + FONT_SIZE; /* Approximate TTF baseline */
    
    while (*text) {
        if (*text == '\n') {
            cy += FONT_SIZE;
            cx = x;
        } else {
            char c = *text;
            if (c >= 32 && c < 127) {
                const baked_char_t *bchar = &font_cdata[c - 32];
                int w = bchar->x1 - bchar->x0;
                int h = bchar->y1 - bchar->y0;
                int px = cx + bchar->xoff;
                int py = cy + bchar->yoff;
                
                for (int dy = 0; dy < h; dy++) {
                    for (int dx = 0; dx < w; dx++) {
                        int bmp_x = bchar->x0 + dx;
                        int bmp_y = bchar->y0 + dy;
                        uint8_t alpha = font_bitmap[bmp_y * FONT_ATLAS_WIDTH + bmp_x];
                        
                        if (alpha > 0) {
                            int draw_x = px + dx;
                            int draw_y = py + dy;
                            
                            if (alpha == 255) {
                                gpu_renderer_putpixel(draw_x, draw_y, color | 0xFF000000);
                            } else {
                                uint32_t dc = gpu_renderer_getpixel(draw_x, draw_y);
                                uint32_t inv_a = 255 - alpha;
                                uint32_t r = (((color >> 16) & 0xFF) * alpha + ((dc >> 16) & 0xFF) * inv_a) >> 8;
                                uint32_t g = (((color >> 8) & 0xFF) * alpha + ((dc >> 8) & 0xFF) * inv_a) >> 8;
                                uint32_t b = ((color & 0xFF) * alpha + (dc & 0xFF) * inv_a) >> 8;
                                gpu_renderer_putpixel(draw_x, draw_y, (0xFF << 24) | (r << 16) | (g << 8) | b);
                            }
                        }
                    }
                }
                cx += bchar->xadvance;
            }
        }
        text++;
    }
}

void font_engine_draw_string_transparent(int x, int y, const char *text, uint32_t color) {
    /* For now, our implementation handles transparency by default using gpu_renderer_getpixel */
    font_engine_draw_string(x, y, text, color);
}

int font_engine_measure_text(const char *text) {
    if (!text) return 0;
    int w = 0;
    while (*text) {
        char c = *text;
        if (c >= 32 && c < 127) {
            w += font_cdata[c - 32].xadvance;
        }
        text++;
    }
    return w;
}
