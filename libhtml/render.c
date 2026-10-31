#include "libhtml/render.h"
#include "gui/renderer.h"
#include "gui/graphics.h"
#include "drivers/font_atlas.h"
#include "zarqaa/string.h"

static void render_node(surface_t *surf, dom_node_t *node, int off_x, int off_y, int clip_y, int clip_h) {
    if (!node || !surf) return;
    
    css_style_t *s = &node->style;
    
    int abs_y = s->y + off_y;
    int abs_x = s->x + off_x;
    
    /* Clipping optimization */
    if (abs_y + s->height < clip_y || abs_y > clip_y + clip_h) {
        if (node->type != TAG_TEXT) {
            /* If it's a container and it's outside, we can skip its children too
               if we assume they don't overflow out of parent. But for now, let's just 
               skip rendering THIS node and proceed to children, or better: 
               wait, if parent is totally outside, children are too, but only if overflow:hidden.
               Let's just skip drawing this node to save time. */
        }
    }
    
    /* 1. Draw Background */
    if (s->bg_color && node->type != TAG_TEXT && node->type != TAG_PATH && node->type != TAG_CIRCLE) {
        if (abs_y + s->height >= clip_y && abs_y <= clip_y + clip_h) {
            if (s->border_radius > 0) {
                renderer_execute_fill_rect(surf, abs_x + s->border_radius, abs_y, s->width - 2*s->border_radius, s->height, s->bg_color);
                renderer_execute_fill_rect(surf, abs_x, abs_y + s->border_radius, s->width, s->height - 2*s->border_radius, s->bg_color);
            } else {
                renderer_execute_fill_rect(surf, abs_x, abs_y, s->width, s->height, s->bg_color);
            }
        }
    }
    
    /* 2. Draw Borders / Glows (simplified box shadow) */
    if (s->border_width > 0 && s->border_color) {
        if (abs_y + s->height >= clip_y && abs_y <= clip_y + clip_h) {
            renderer_execute_draw_line(surf, abs_x, abs_y, abs_x + s->width, abs_y, s->border_color);
            renderer_execute_draw_line(surf, abs_x, abs_y + s->height, abs_x + s->width, abs_y + s->height, s->border_color);
            renderer_execute_draw_line(surf, abs_x, abs_y, abs_x, abs_y + s->height, s->border_color);
            renderer_execute_draw_line(surf, abs_x + s->width, abs_y, abs_x + s->width, abs_y + s->height, s->border_color);
        }
    }
    
    /* 3. Handle Specific Tags */
    if (node->type == TAG_TEXT) {
        /* Render Text */
        int cx = abs_x;
        int cy = abs_y + FONT_SIZE; /* Baseline approximation */
        color_t color = s->text_color ? s->text_color : 0xFFFFFFFF;
        
        uint32_t r = (color >> 16) & 0xFF;
        uint32_t g = (color >> 8) & 0xFF;
        uint32_t b = color & 0xFF;
        
        for (int i = 0; node->text[i]; i++) {
            char c = node->text[i];
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
                            if (py + dy >= clip_y && py + dy < clip_y + clip_h && px + dx >= 0 && px + dx < (int)surf->width) {
                                if (alpha == 255) {
                                    renderer_execute_fill_rect(surf, px + dx, py + dy, 1, 1, color);
                                } else {
                                uint32_t *buf = surf->pixels ? surf->pixels : (uint32_t *)surf->cpu_pointer;
                                color_t bg_col = 0;
                                if (buf) {
                                    uint32_t *bg_ptr = (uint32_t *)((uint8_t *)buf + (py + dy) * surf->pitch + (px + dx) * 4);
                                    bg_col = *bg_ptr;
                                }
                                uint32_t bg_r = (bg_col >> 16) & 0xFF;
                                uint32_t bg_g = (bg_col >> 8) & 0xFF;
                                uint32_t bg_b = bg_col & 0xFF;
                                
                                uint32_t inv_alpha = 255 - alpha;
                                uint32_t mix_r = (r * alpha + bg_r * inv_alpha) / 255;
                                uint32_t mix_g = (g * alpha + bg_g * inv_alpha) / 255;
                                uint32_t mix_b = (b * alpha + bg_b * inv_alpha) / 255;
                                
                                color_t mixed = (mix_r << 16) | (mix_g << 8) | mix_b;
                                renderer_execute_fill_rect(surf, px + dx, py + dy, 1, 1, mixed);
                            }
                        }
                    }
                    }
                }
                cx += bchar->xadvance;
            }
        }
    } else if (node->type == TAG_CIRCLE) {
        color_t col = s->bg_color ? s->bg_color : s->text_color;
        if (col) {
            int cx = abs_x + node->cx;
            int cy = abs_y + node->cy;
            int r = node->r;
            /* Simple filled circle algorithm for surface */
            for (int y = -r; y <= r; y++) {
                int x_max = 0;
                int r2 = r * r;
                int y2 = y * y;
                while (x_max * x_max + y2 <= r2) x_max++;
                x_max--;
                if (x_max >= 0 && cy + y >= clip_y && cy + y < clip_y + clip_h) {
                    renderer_execute_fill_rect(surf, cx - x_max, cy + y, x_max * 2 + 1, 1, col);
                }
            }
        }
    } else if (node->type == TAG_PATH) {
        /* Basic SVG path parser stub (Draws nothing yet, just a placeholder) */
        /* To draw complex paths, we need a vector rasterizer. For now, skip. */
    }
    
    /* 4. Render Children */
    for (int i = 0; i < node->child_count; i++) {
        render_node(surf, node->children[i], off_x, off_y, clip_y, clip_h);
    }
}

void html_render(surface_t *surf, dom_node_t *root, int offset_x, int offset_y, int clip_y, int clip_h) {
    if (!surf || !root) return;
    render_node(surf, root, offset_x, offset_y, clip_y, clip_h);
}
