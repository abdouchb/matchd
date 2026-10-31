/*
 * Zarqaa OS 0.1 - Smart Layer System (Compositor)
 * Creator: Abdelhak Chabbi
 */

#include "gui/layer.h"
#include "kernel/heap.h"
#include "drivers/framebuffer.h"
#include "zarqaa/string.h"

static struct layer layers[MAX_LAYERS];
static int layer_count = 0;
static int next_layer_id = 1;

void layer_init(void) {
    for (int i = 0; i < MAX_LAYERS; i++) {
        layers[i].id = 0;
    }
    layer_count = 0;
}

struct layer *layer_create(int x, int y, int w, int h, int z_index) {
    if (layer_count >= MAX_LAYERS) return NULL;
    
    int slot = -1;
    for (int i = 0; i < MAX_LAYERS; i++) {
        if (layers[i].id == 0) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return NULL;

    struct layer *l = &layers[slot];
    l->id = next_layer_id++;
    l->x = x;
    l->y = y;
    l->width = w;
    l->height = h;
    l->z_index = z_index;
    l->visible = true;
    l->alpha = 255;
    l->needs_redraw = true;
    l->buffer = kcalloc(w * h, 4);
    if (!l->buffer) {
        l->id = 0;
        return NULL;
    }
    l->user_data = NULL;
    l->on_draw = NULL;
    
    layer_count++;
    return l;
}

void layer_destroy(struct layer *l) {
    if (!l || l->id == 0) return;
    if (l->buffer) kfree(l->buffer);
    l->id = 0;
    layer_count--;
}

void layer_set_pos(struct layer *l, int x, int y) {
    if (l && (l->x != x || l->y != y)) {
        /* Add dirty rects for old and new positions */
        dirty_rect_add(l->x, l->y, l->width, l->height);
        l->x = x;
        l->y = y;
        dirty_rect_add(l->x, l->y, l->width, l->height);
    }
}

void layer_set_visible(struct layer *l, bool visible) {
    if (l && l->visible != visible) {
        l->visible = visible;
        dirty_rect_add(l->x, l->y, l->width, l->height);
    }
}

void layer_set_alpha(struct layer *l, uint8_t alpha) {
    if (l && l->alpha != alpha) {
        l->alpha = alpha;
        dirty_rect_add(l->x, l->y, l->width, l->height);
    }
}

void layer_set_zindex(struct layer *l, int z) {
    if (l && l->z_index != z) {
        l->z_index = z;
        dirty_rect_add(l->x, l->y, l->width, l->height);
    }
}

void layer_mark_dirty(struct layer *l) {
    if (l) {
        l->needs_redraw = true;
        dirty_rect_add(l->x, l->y, l->width, l->height);
    }
}

/* Fast alpha blending implementation */
void layer_blit_alpha(uint32_t *dst_buffer, int dst_pitch, int dst_x, int dst_y, 
                      uint32_t *src_buffer, int src_w, int src_h,
                      int src_x, int src_y, int blit_w, int blit_h, uint8_t global_alpha) {
    
    (void)src_h; /* not needed explicitly if bounds are checked beforehand */

    for (int y = 0; y < blit_h; y++) {
        uint32_t *dst_row = dst_buffer + (dst_y + y) * (dst_pitch / 4) + dst_x;
        uint32_t *src_row = src_buffer + (src_y + y) * src_w + src_x;

        for (int x = 0; x < blit_w; x++) {
            uint32_t src_pixel = src_row[x];
            uint8_t a = (src_pixel >> 24) & 0xFF;
            
            /* Apply global alpha */
            if (global_alpha != 255) {
                a = (a * global_alpha) / 255;
            }

            if (a == 255) {
                dst_row[x] = src_pixel | 0xFF000000; /* Force opaque for dst */
            } else if (a > 0) {
                uint32_t dst_pixel = dst_row[x];
                uint8_t inv_a = 255 - a;

                uint8_t sr = (src_pixel >> 16) & 0xFF;
                uint8_t sg = (src_pixel >> 8) & 0xFF;
                uint8_t sb = src_pixel & 0xFF;

                uint8_t dr = (dst_pixel >> 16) & 0xFF;
                uint8_t dg = (dst_pixel >> 8) & 0xFF;
                uint8_t db = dst_pixel & 0xFF;

                uint8_t rr = (sr * a + dr * inv_a) / 255;
                uint8_t rg = (sg * a + dg * inv_a) / 255;
                uint8_t rb = (sb * a + db * inv_a) / 255;

                dst_row[x] = (0xFF << 24) | (rr << 16) | (rg << 8) | rb;
            }
        }
    }
}

void layer_render_all(rect_t *clip_rect) {
    /* 1. Sort layers by Z-index (simple bubble sort since MAX_LAYERS is small) */
    struct layer *sorted[MAX_LAYERS];
    int count = 0;
    
    for (int i = 0; i < MAX_LAYERS; i++) {
        if (layers[i].id != 0 && layers[i].visible) {
            sorted[count++] = &layers[i];
            
            /* Give the layer a chance to redraw itself to its buffer if needed */
            if (layers[i].needs_redraw && layers[i].on_draw) {
                fb_set_render_target(layers[i].buffer, layers[i].width, layers[i].height, layers[i].width * 4);
                layers[i].on_draw(&layers[i]);
                fb_reset_render_target();
                layers[i].needs_redraw = false;
            }
        }
    }

    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (sorted[j]->z_index > sorted[j+1]->z_index) {
                struct layer *temp = sorted[j];
                sorted[j] = sorted[j+1];
                sorted[j+1] = temp;
            }
        }
    }

    /* 2. Composite into backbuffer over the clip rect */
    uint32_t *bb = fb_get_backbuffer();
    int bb_pitch = fb_get_pitch();
    
    int cx1 = clip_rect->x;
    int cy1 = clip_rect->y;
    int cx2 = cx1 + clip_rect->width;
    int cy2 = cy1 + clip_rect->height;

    for (int i = 0; i < count; i++) {
        struct layer *l = sorted[i];
        
        int x1 = l->x;
        int y1 = l->y;
        int x2 = l->x + l->width;
        int y2 = l->y + l->height;

        /* Intersection with clip rect */
        int ix1 = (x1 > cx1) ? x1 : cx1;
        int iy1 = (y1 > cy1) ? y1 : cy1;
        int ix2 = (x2 < cx2) ? x2 : cx2;
        int iy2 = (y2 < cy2) ? y2 : cy2;

        if (ix1 < ix2 && iy1 < iy2) {
            int src_x = ix1 - l->x;
            int src_y = iy1 - l->y;
            int blit_w = ix2 - ix1;
            int blit_h = iy2 - iy1;

            layer_blit_alpha(bb, bb_pitch, ix1, iy1, 
                             l->buffer, l->width, l->height, 
                             src_x, src_y, blit_w, blit_h, l->alpha);
        }
    }
}
