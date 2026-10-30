/*
 * Zarqaa OS - 3D Wireframe Cube Benchmark
 * Tests fixed-point 3D math and line rasterization performance.
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "drivers/pit.h"
#include "zarqaa/string.h"

static const int sin_table_256[91] = {
    0, 4, 8, 13, 17, 22, 26, 31, 35, 40, 44, 48, 53, 57, 61, 65,
    70, 74, 78, 83, 87, 91, 95, 99, 103, 107, 111, 115, 119, 123,
    127, 131, 135, 138, 142, 146, 149, 153, 156, 160, 163, 167, 170,
    173, 177, 180, 183, 186, 189, 192, 195, 198, 200, 203, 206, 208,
    211, 213, 216, 218, 220, 223, 225, 227, 229, 231, 232, 234, 236,
    238, 239, 241, 242, 243, 245, 246, 247, 248, 249, 250, 251, 252,
    253, 253, 254, 254, 255, 255, 255, 255, 256
};

static int fast_sin(int deg) {
    deg = (deg % 360 + 360) % 360;
    if (deg <= 90) return sin_table_256[deg];
    if (deg <= 180) return sin_table_256[180 - deg];
    if (deg <= 270) return -sin_table_256[deg - 180];
    return -sin_table_256[360 - deg];
}

static int fast_cos(int deg) {
    return fast_sin(deg + 90);
}

typedef struct {
    int x, y, z;
} vec3_t;

static const vec3_t cube_vertices[8] = {
    {-100, -100, -100}, { 100, -100, -100}, { 100,  100, -100}, {-100,  100, -100},
    {-100, -100,  100}, { 100, -100,  100}, { 100,  100,  100}, {-100,  100,  100}
};

static const int cube_edges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0}, // Back face
    {4,5}, {5,6}, {6,7}, {7,4}, // Front face
    {0,4}, {1,5}, {2,6}, {3,7}  // Connecting edges
};

struct bench_state {
    int angle_x, angle_y, angle_z;
    uint64_t last_fps_time;
    int frames;
    int current_fps;
};

static void benchmark_on_draw(struct window *win) {
    struct bench_state *state = (struct bench_state *)win->user_data;
    if (!state) return;

    fb_set_render_target(win->content_buffer, win->width, win->height, win->width * 4);

    /* Fill background */
    fb_fill_rect(0, 0, win->width, win->height, 0xFF1E1E2E);
    
    int cx = win->width / 2;
    int cy = win->height / 2;

    int sx = fast_sin(state->angle_x), cx_ang = fast_cos(state->angle_x);
    int sy = fast_sin(state->angle_y), cy_ang = fast_cos(state->angle_y);
    int sz = fast_sin(state->angle_z), cz_ang = fast_cos(state->angle_z);

    vec3_t proj[8];
    for (int i = 0; i < 8; i++) {
        int x = cube_vertices[i].x;
        int y = cube_vertices[i].y;
        int z = cube_vertices[i].z;

        // Rotate X
        int xy = (y * cx_ang - z * sx) / 256;
        int xz = (y * sx + z * cx_ang) / 256;
        y = xy; z = xz;

        // Rotate Y
        int yx = (x * cy_ang + z * sy) / 256;
        int yz = (-x * sy + z * cy_ang) / 256;
        x = yx; z = yz;

        // Rotate Z
        int zx = (x * cz_ang - y * sz) / 256;
        int zy = (x * sz + y * cz_ang) / 256;
        x = zx; y = zy;

        // Perspective Projection
        int z_offset = z + 400; // Push it forward
        if (z_offset < 10) z_offset = 10;
        
        proj[i].x = cx + (x * 400) / z_offset;
        proj[i].y = cy + (y * 400) / z_offset;
    }

    // Draw multiple cubes with an offset to stress test the rendering
    uint32_t colors[] = {0xFFF38BA8, 0xFF89B4FA, 0xFFA6E3A1, 0xFFF9E2AF};
    
    for (int count = 0; count < 15; count++) { // Draw 15 cubes scaled differently
        int scale = 256 - (count * 15);
        int c_idx = count % 4;
        
        for (int i = 0; i < 12; i++) {
            int p1 = cube_edges[i][0];
            int p2 = cube_edges[i][1];
            
            int x1 = cx + ((proj[p1].x - cx) * scale) / 256;
            int y1 = cy + ((proj[p1].y - cy) * scale) / 256;
            int x2 = cx + ((proj[p2].x - cx) * scale) / 256;
            int y2 = cy + ((proj[p2].y - cy) * scale) / 256;

            fb_draw_line(x1, y1, x2, y2, colors[c_idx]);
        }
    }

    state->angle_x = (state->angle_x + 1) % 360;
    state->angle_y = (state->angle_y + 2) % 360;
    state->angle_z = (state->angle_z + 1) % 360;

    /* FPS Calculation */
    uint64_t current_time = pit_get_ticks();
    state->frames++;
    
    if (current_time - state->last_fps_time >= 1000) {
        state->current_fps = state->frames;
        state->frames = 0;
        state->last_fps_time = current_time;
    }

    /* Draw UI Overlay */
    fb_fill_rect(10, 10, 180, 50, 0xAA000000); 
    fb_draw_rect(10, 10, 180, 50, 0xFF89B4FA);
    
    char fps_str[32] = "3D FPS: ";
    char num_buf[16];
    itoa(state->current_fps, num_buf, 10);
    strcat(fps_str, num_buf);
    
    fb_draw_string_transparent(20, 20, fps_str, 0xFFA6E3A1);
    fb_draw_string_transparent(20, 40, "Polygons: 180 Lines", 0xFFCDD6F4);

    fb_reset_render_target();

    /* Request next frame */
    win->needs_redraw = true;
}

static void benchmark_on_close(struct window *win) {
    struct bench_state *state = (struct bench_state *)win->user_data;
    if (state) kfree(state);
}

struct window *benchmark_open(void) {
    struct window *win = wm_create_window("3D Cube Benchmark", 100, 100, 600, 500);
    if (!win) return NULL;

    struct bench_state *state = kcalloc(1, sizeof(struct bench_state));
    state->last_fps_time = pit_get_ticks();
    
    win->user_data = state;
    win->on_draw = benchmark_on_draw;
    win->on_close = benchmark_on_close;
    
    return win;
}
