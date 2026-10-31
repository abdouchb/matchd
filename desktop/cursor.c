/*
 * Zarqaa OS 0.1 - Mouse Cursor
 * Creator: Abdelhak Chabbi
 */

#include "desktop/cursor.h"
#include "drivers/framebuffer.h"

static uint32_t bg_buffer[CURSOR_WIDTH * CURSOR_HEIGHT];
static int saved_x = -1;
static int saved_y = -1;
static uint32_t cursor_image_buffer[CURSOR_WIDTH * CURSOR_HEIGHT];

// 0: Transparent, 1: Black (Outline), 2: White (Fill)
static const uint8_t cursor_bitmap[CURSOR_HEIGHT][CURSOR_WIDTH] = {
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,1,1,1,1,1},
    {1,2,2,2,1,2,2,1,0,0,0,0},
    {1,2,2,1,0,1,2,2,1,0,0,0},
    {1,2,1,0,0,1,2,2,1,0,0,0},
    {1,1,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,0,1,1,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
};

void cursor_init(void) {
    for(int i=0; i<CURSOR_WIDTH*CURSOR_HEIGHT; i++) bg_buffer[i] = 0;
    
    // Generate 32-bit ARGB cursor image
    for (int cy = 0; cy < CURSOR_HEIGHT; cy++) {
        for (int cx = 0; cx < CURSOR_WIDTH; cx++) {
            uint8_t pixel = cursor_bitmap[cy][cx];
            if (pixel == 1) {
                cursor_image_buffer[cy * CURSOR_WIDTH + cx] = 0xFF000000; // Black Outline
            } else if (pixel == 2) {
                cursor_image_buffer[cy * CURSOR_WIDTH + cx] = 0xFFFFFFFF; // White Fill
            } else {
                cursor_image_buffer[cy * CURSOR_WIDTH + cx] = 0x00000000; // Transparent
            }
        }
    }
}

void cursor_save_background(int x, int y) {
    saved_x = x;
    saved_y = y;
    uint32_t *fb = fb_get_backbuffer();
    int pitch = fb_get_pitch() / 4;
    int w = fb_get_width();
    int h = fb_get_height();

    for (int cy = 0; cy < CURSOR_HEIGHT; cy++) {
        for (int cx = 0; cx < CURSOR_WIDTH; cx++) {
            if (x + cx < w && y + cy < h) {
                bg_buffer[cy * CURSOR_WIDTH + cx] = fb[(y + cy) * pitch + (x + cx)];
            }
        }
    }
}

void cursor_restore_background(void) {
    if (saved_x == -1 || saved_y == -1) return;

    uint32_t *fb = fb_get_backbuffer();
    int pitch = fb_get_pitch() / 4;
    int w = fb_get_width();
    int h = fb_get_height();

    for (int cy = 0; cy < CURSOR_HEIGHT; cy++) {
        for (int cx = 0; cx < CURSOR_WIDTH; cx++) {
            if (saved_x + cx < w && saved_y + cy < h) {
                fb[(saved_y + cy) * pitch + (saved_x + cx)] = bg_buffer[cy * CURSOR_WIDTH + cx];
            }
        }
    }
}

void cursor_draw(int x, int y) {
    extern void gapi_draw_image(void *ctx, int x, int y, int w, int h, uint32_t *pixels);
    gapi_draw_image(NULL, x, y, CURSOR_WIDTH, CURSOR_HEIGHT, cursor_image_buffer);
}

