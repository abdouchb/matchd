/*
 * Zarqaa OS 0.1 - Smart Layer System (Compositor)
 * Creator: Abdelhak Chabbi
 */

#ifndef GUI_LAYER_H
#define GUI_LAYER_H

#include "zarqaa/types.h"
#include "gui/dirty_rect.h"

#define MAX_LAYERS 64

struct layer {
    int id;
    int x;
    int y;
    int width;
    int height;
    int z_index;
    bool visible;
    uint8_t alpha;           /* 0-255 global opacity */
    bool needs_redraw;       /* If true, on_draw will be called before composition */
    uint32_t *buffer;        /* The backing store for this layer */
    
    void *user_data;
    void (*on_draw)(struct layer *l);
};

void layer_init(void);
struct layer *layer_create(int x, int y, int w, int h, int z_index);
void layer_destroy(struct layer *l);

void layer_set_pos(struct layer *l, int x, int y);
void layer_set_visible(struct layer *l, bool visible);
void layer_set_alpha(struct layer *l, uint8_t alpha);
void layer_set_zindex(struct layer *l, int z);
void layer_mark_dirty(struct layer *l);

/* The core compositor function. Blits all visible layers overlapping clip_rect to backbuffer. */
void layer_render_all(rect_t *clip_rect);

/* Helper function for fast alpha blending of a buffer onto the backbuffer */
void layer_blit_alpha(uint32_t *dst_buffer, int dst_pitch, int dst_x, int dst_y, 
                      uint32_t *src_buffer, int src_w, int src_h,
                      int src_x, int src_y, int blit_w, int blit_h, uint8_t global_alpha);

#endif /* GUI_LAYER_H */
