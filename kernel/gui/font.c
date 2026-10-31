#include "drivers/font.h"
#include "drivers/font_atlas.h"
#include "drivers/gpu_renderer.h"

void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = font_get_glyph(c);
    for (int cy = 0; cy < FONT_HEIGHT; cy++) {
        uint8_t row = glyph[cy];
        for (int cx = 0; cx < FONT_WIDTH; cx++) {
            if (row & (1 << (FONT_WIDTH - 1 - cx))) {
                gpu_renderer_putpixel(x + cx, y + cy, fg);
            } else {
                gpu_renderer_putpixel(x + cx, y + cy, bg);
            }
        }
    }
}

void gfx_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
    int start_x = x;
    while (*s) {
        if (*s == '\n') {
            y += FONT_HEIGHT;
            x = start_x;
        } else {
            gfx_draw_char(x, y, *s, fg, bg);
            x += FONT_WIDTH;
        }
        s++;
    }
}

void gfx_draw_string_transparent(int x, int y, const char *s, uint32_t fg) {
    int cx = x;
    int cy = y + FONT_SIZE; /* Approximate TTF baseline */
    
    uint32_t r_fg = (fg >> 16) & 0xFF;
    uint32_t g_fg = (fg >> 8) & 0xFF;
    uint32_t b_fg = fg & 0xFF;

    while (*s) {
        if (*s == '\n') {
            cy += FONT_SIZE;
            cx = x;
        } else {
            char c = *s;
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
                            if (alpha == 255) {
                                gpu_renderer_putpixel(px + dx, py + dy, fg);
                            } else {
                                uint32_t bg_col = gpu_renderer_getpixel(px + dx, py + dy);
                                uint32_t bg_r = (bg_col >> 16) & 0xFF;
                                uint32_t bg_g = (bg_col >> 8) & 0xFF;
                                uint32_t bg_b = bg_col & 0xFF;
                                
                                uint32_t inv_alpha = 255 - alpha;
                                uint32_t mix_r = (r_fg * alpha + bg_r * inv_alpha) / 255;
                                uint32_t mix_g = (g_fg * alpha + bg_g * inv_alpha) / 255;
                                uint32_t mix_b = (b_fg * alpha + bg_b * inv_alpha) / 255;
                                
                                uint32_t mixed = (mix_r << 16) | (mix_g << 8) | mix_b;
                                gpu_renderer_putpixel(px + dx, py + dy, mixed);
                            }
                        }
                    }
                }
                cx += bchar->xadvance;
            }
        }
        s++;
    }
}
