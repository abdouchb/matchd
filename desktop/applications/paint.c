/*
 * Zarqaa Studio - Paint/Photoshop Application
 * Creator: Abdelhak Chabbi & AI
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "filesystem/vfs.h"
#include "gui/dirty_rect.h"
#include "zarqaa/string.h"
#include "drivers/usb/usb_hid.h"

#define CANVAS_W 700
#define CANVAS_H 400
#define UI_LEFT_BAR_W 60
#define UI_TOP_BAR_H 70
#define CANVAS_X (UI_LEFT_BAR_W + 20)
#define CANVAS_Y (UI_TOP_BAR_H + 20)
#define MAX_UNDO 5

enum TOOL {
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_CIRCLE,
    TOOL_FILL
};

struct paint_state {
    uint32_t *canvas_buffer;
    uint32_t *preview_buffer;
    uint32_t *history[MAX_UNDO];
    int history_count;
    
    enum TOOL current_tool;
    color_t current_color;
    int brush_size;
    
    bool is_drawing;
    int last_x, last_y;
    int start_x, start_y;
};

static color_t palette[] = {
    0xFF11111B, 0xFFFFFFFF, 0xFFF38BA8, 0xFFA6E3A1, 0xFF89B4FA, 
    0xFFF9E2AF, 0xFFCBA6F7, 0xFF94E2D5
};

static void save_history(struct paint_state *state) {
    // Shift history right
    if (state->history[MAX_UNDO - 1]) {
        kfree(state->history[MAX_UNDO - 1]);
    }
    for (int i = MAX_UNDO - 1; i > 0; i--) {
        state->history[i] = state->history[i - 1];
    }
    // Save current canvas
    state->history[0] = kmalloc(CANVAS_W * CANVAS_H * 4);
    memcpy(state->history[0], state->canvas_buffer, CANVAS_W * CANVAS_H * 4);
    
    if (state->history_count < MAX_UNDO) state->history_count++;
}

static void undo(struct paint_state *state, struct window *win) {
    if (state->history_count > 0) {
        memcpy(state->canvas_buffer, state->history[0], CANVAS_W * CANVAS_H * 4);
        uint32_t *old = state->history[0];
        for (int i = 0; i < MAX_UNDO - 1; i++) {
            state->history[i] = state->history[i + 1];
        }
        state->history[MAX_UNDO - 1] = NULL;
        kfree(old);
        state->history_count--;
        win->needs_redraw = true;
    }
}

static void flood_fill(struct paint_state *state, int start_x, int start_y, color_t fill_color) {
    if (start_x < 0 || start_y < 0 || start_x >= CANVAS_W || start_y >= CANVAS_H) return;
    
    color_t target_color = state->canvas_buffer[start_y * CANVAS_W + start_x];
    if (target_color == fill_color) return;
    
    int max_q = CANVAS_W * CANVAS_H;
    short *q_x = kmalloc(max_q * sizeof(short));
    short *q_y = kmalloc(max_q * sizeof(short));
    if (!q_x || !q_y) {
        if(q_x) kfree(q_x);
        if(q_y) kfree(q_y);
        return;
    }
    
    int head = 0, tail = 0;
    q_x[tail] = start_x;
    q_y[tail] = start_y;
    tail++;
    
    while (head < tail) {
        int cx = q_x[head];
        int cy = q_y[head];
        head++;
        
        if (cx < 0 || cx >= CANVAS_W || cy < 0 || cy >= CANVAS_H) continue;
        
        int idx = cy * CANVAS_W + cx;
        if (state->canvas_buffer[idx] == target_color) {
            state->canvas_buffer[idx] = fill_color;
            
            if (tail + 4 < max_q) {
                q_x[tail] = cx + 1; q_y[tail++] = cy;
                q_x[tail] = cx - 1; q_y[tail++] = cy;
                q_x[tail] = cx; q_y[tail++] = cy + 1;
                q_x[tail] = cx; q_y[tail++] = cy - 1;
            }
        }
    }
    
    kfree(q_x);
    kfree(q_y);
}

#pragma pack(push, 1)
struct bmp_header {
    uint16_t type;
    uint32_t size;
    uint16_t res1;
    uint16_t res2;
    uint32_t offset;
    uint32_t dib_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t comp;
    uint32_t img_size;
    int32_t x_res;
    int32_t y_res;
    uint32_t colors;
    uint32_t imp_colors;
};
#pragma pack(pop)

static void export_bmp(struct paint_state *state) {
    struct bmp_header hdr = {0};
    hdr.type = 0x4D42; // 'BM'
    hdr.offset = sizeof(struct bmp_header);
    hdr.size = hdr.offset + (CANVAS_W * CANVAS_H * 4);
    hdr.dib_size = 40;
    hdr.width = CANVAS_W;
    hdr.height = -CANVAS_H; // Top-down
    hdr.planes = 1;
    hdr.bpp = 32;
    
    struct vfs_node *file = vfs_create("/hd0/images/saved.bmp", VFS_FILE);
    if (!file) {
        // If it exists, try opening and overwriting it
        file = vfs_open("/hd0/images/saved.bmp");
    }
    
    if (file) {
        char *full_buf = kmalloc(hdr.size);
        memcpy(full_buf, &hdr, sizeof(hdr));
        memcpy(full_buf + sizeof(hdr), state->canvas_buffer, CANVAS_W * CANVAS_H * 4);
        
        // Convert ARGB to BGRA for BMP
        uint32_t *pixels = (uint32_t*)(full_buf + sizeof(hdr));
        for(int i=0; i<CANVAS_W * CANVAS_H; i++) {
            uint32_t p = pixels[i];
            uint8_t a = (p >> 24) & 0xFF;
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = p & 0xFF;
            pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
        
        vfs_write(file, full_buf, hdr.size);
        kfree(full_buf);
    }
}

static void draw_on_canvas(struct paint_state *state, int x, int y, bool is_preview) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= CANVAS_W) x = CANVAS_W - 1;
    if (y >= CANVAS_H) y = CANVAS_H - 1;
    
    uint32_t *target = is_preview ? state->preview_buffer : state->canvas_buffer;
    fb_set_render_target(target, CANVAS_W, CANVAS_H, CANVAS_W * 4);
    
    color_t color = (state->current_tool == TOOL_ERASER) ? 0xFFFFFFFF : state->current_color;
    
    if (state->current_tool == TOOL_BRUSH || state->current_tool == TOOL_ERASER) {
        gfx_draw_circle_filled(x, y, state->brush_size, color);
        if (state->last_x != -1 && state->last_y != -1) {
            gfx_draw_line_aa(state->last_x, state->last_y, x, y, color);
            int dx = x - state->last_x;
            int dy = y - state->last_y;
            int steps = (dx*dx + dy*dy) > 100 ? 10 : 2;
            for(int i=0; i<=steps; i++) {
                int px = state->last_x + (dx * i) / steps;
                int py = state->last_y + (dy * i) / steps;
                gfx_draw_circle_filled(px, py, state->brush_size, color);
            }
        }
    } else if (state->current_tool == TOOL_LINE) {
        if (is_preview) {
            memcpy(state->preview_buffer, state->canvas_buffer, CANVAS_W * CANVAS_H * 4);
            fb_set_render_target(state->preview_buffer, CANVAS_W, CANVAS_H, CANVAS_W * 4);
        }
        gfx_draw_line_aa(state->start_x, state->start_y, x, y, color);
    } else if (state->current_tool == TOOL_RECT) {
        if (is_preview) {
            memcpy(state->preview_buffer, state->canvas_buffer, CANVAS_W * CANVAS_H * 4);
            fb_set_render_target(state->preview_buffer, CANVAS_W, CANVAS_H, CANVAS_W * 4);
        }
        int rx = state->start_x < x ? state->start_x : x;
        int ry = state->start_y < y ? state->start_y : y;
        int rw = (state->start_x > x ? state->start_x : x) - rx;
        int rh = (state->start_y > y ? state->start_y : y) - ry;
        fb_draw_rect(rx, ry, rw, rh, color);
    } else if (state->current_tool == TOOL_CIRCLE) {
        if (is_preview) {
            memcpy(state->preview_buffer, state->canvas_buffer, CANVAS_W * CANVAS_H * 4);
            fb_set_render_target(state->preview_buffer, CANVAS_W, CANVAS_H, CANVAS_W * 4);
        }
        int dx = x - state->start_x;
        int dy = y - state->start_y;
        int r = 0;
        while(r*r < dx*dx + dy*dy) r++;
        gfx_draw_circle_aa(state->start_x, state->start_y, r, color);
    } else if (state->current_tool == TOOL_FILL && !is_preview) {
        flood_fill(state, x, y, color);
    }
    
    fb_reset_render_target();
}

static void paint_on_draw(struct window *win) {
    struct paint_state *state = (struct paint_state *)win->user_data;
    
    // Background (Photoshop Dark Theme)
    fb_fill_rect(0, 0, win->width, win->height, 0xFF181825);
    
    // Canvas Drop Shadow
    fb_fill_rect(CANVAS_X + 5, CANVAS_Y + 5, CANVAS_W, CANVAS_H, 0xAA000000);
    
    // Blit Canvas (Fixing global coords issue)
    uint32_t *target = state->is_drawing && (state->current_tool == TOOL_LINE || state->current_tool == TOOL_RECT || state->current_tool == TOOL_CIRCLE) ? state->preview_buffer : state->canvas_buffer;
    uint32_t *fb = win->content_buffer; // Draw directly to local content_buffer
    
    for (int cy = 0; cy < CANVAS_H; cy++) {
        for (int cx = 0; cx < CANVAS_W; cx++) {
            fb[(CANVAS_Y + cy) * win->width + (CANVAS_X + cx)] = target[cy * CANVAS_W + cx];
        }
    }
    
    // Top Bar (Menu & Properties)
    gfx_draw_glass_panel(0, 0, win->width, UI_TOP_BAR_H, 0, 0xDD1E1E2E, 0xFF313244, 255);
    
    // Menu
    fb_draw_string_transparent(20, 10, "File", 0xFFCDD6F4);
    fb_draw_string_transparent(80, 10, "Edit", 0xFFCDD6F4);
    
    // Buttons (Export & Undo)
    gfx_draw_rounded_rect_filled(150, 5, 80, 22, 5, 0xFF313244);
    fb_draw_string_transparent(165, 9, "Save", 0xFFA6E3A1);
    
    gfx_draw_rounded_rect_filled(240, 5, 80, 22, 5, state->history_count > 0 ? 0xFF313244 : 0xFF181825);
    fb_draw_string_transparent(255, 9, "Undo", state->history_count > 0 ? 0xFFF9E2AF : 0xFF45475A);
    
    // Color Palette Swatches
    int px = 20;
    for (int i = 0; i < 8; i++) {
        bool active = (state->current_color == palette[i] && state->current_tool != TOOL_ERASER);
        if (active) {
            gfx_draw_rounded_rect_filled(px - 2, 35 - 2, 28, 28, 4, 0xFFFFFFFF);
        }
        gfx_draw_rounded_rect_filled(px, 35, 24, 24, 3, palette[i]);
        px += 34;
    }
    
    // Brush Size
    px += 20;
    fb_draw_string_transparent(px, 40, "Size:", 0xFFCDD6F4);
    px += 45;
    for (int s = 1; s <= 3; s++) {
        int size_val = s * 3;
        bool active = (state->brush_size == size_val);
        gfx_draw_circle_filled(px + 10, 45, size_val, active ? 0xFF89B4FA : 0xFF45475A);
        px += 30;
    }
    
    // Left Toolbar
    fb_fill_rect(0, UI_TOP_BAR_H, UI_LEFT_BAR_W, win->height - UI_TOP_BAR_H, 0xFF1E1E2E);
    fb_fill_rect(UI_LEFT_BAR_W, UI_TOP_BAR_H, 1, win->height - UI_TOP_BAR_H, 0xFF313244);
    
    // Tool Icons
    const char *tools[] = {"Brush", "Erase", "Line", "Rect", "Circ", "Fill"}; 
    int ty = UI_TOP_BAR_H + 20;
    for (int i = 0; i < 6; i++) {
        bool active = ((int)state->current_tool == i);
        if (active) {
            gfx_draw_rounded_rect_filled(5, ty, 50, 40, 8, 0xFF313244);
        }
        fb_draw_string_transparent(12, ty + 12, tools[i], active ? 0xFF89B4FA : 0xFFCDD6F4);
        ty += 50;
    }
}

static void paint_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    struct paint_state *state = (struct paint_state *)win->user_data;
    
    if (left && my < UI_TOP_BAR_H) {
        if (my < 30) {
            // Save Button
            if (mx >= 150 && mx <= 230) {
                export_bmp(state);
                win->needs_redraw = true;
                return;
            }
            // Undo Button
            if (mx >= 240 && mx <= 320) {
                undo(state, win);
                return;
            }
        } else {
            // Palette click
            int px = 20;
            for (int i = 0; i < 8; i++) {
                if (mx >= px && mx <= px + 24) {
                    state->current_color = palette[i];
                    if (state->current_tool == TOOL_ERASER) state->current_tool = TOOL_BRUSH;
                    win->needs_redraw = true;
                    return;
                }
                px += 34;
            }
            // Size click
            px += 20 + 45;
            for (int s = 1; s <= 3; s++) {
                if (mx >= px && mx <= px + 24) {
                    state->brush_size = s * 3;
                    win->needs_redraw = true;
                    return;
                }
                px += 30;
            }
        }
    }
    
    if (left && mx < UI_LEFT_BAR_W && my > UI_TOP_BAR_H) {
        int ty = UI_TOP_BAR_H + 20;
        for (int i = 0; i < 6; i++) {
            if (my >= ty && my <= ty + 40) {
                state->current_tool = i;
                win->needs_redraw = true;
                return;
            }
            ty += 50;
        }
    }
    
    int cx = mx - CANVAS_X;
    int cy = my - CANVAS_Y;
    
    if (state->current_tool == TOOL_FILL) {
        if (left && cx >= 0 && cy >= 0 && cx < CANVAS_W && cy < CANVAS_H) {
            save_history(state);
            draw_on_canvas(state, cx, cy, false);
            win->needs_redraw = true;
        }
    } else {
        bool is_pressed = mouse_left_pressed();
        
        if (is_pressed) {
            if (!state->is_drawing && cx >= 0 && cy >= 0 && cx < CANVAS_W && cy < CANVAS_H) {
                save_history(state);
                state->is_drawing = true;
                state->start_x = cx;
                state->start_y = cy;
                state->last_x = cx;
                state->last_y = cy;
                
                if (state->current_tool == TOOL_BRUSH || state->current_tool == TOOL_ERASER) {
                    draw_on_canvas(state, cx, cy, false);
                    
                    // Copy ONLY the brush stroke area to the window buffer
                    int min_x = cx - state->brush_size - 2;
                    int min_y = cy - state->brush_size - 2;
                    int max_x = cx + state->brush_size + 2;
                    int max_y = cy + state->brush_size + 2;
                    if (min_x < 0) min_x = 0; if (min_y < 0) min_y = 0;
                    if (max_x > CANVAS_W) max_x = CANVAS_W; if (max_y > CANVAS_H) max_y = CANVAS_H;
                    
                    uint32_t *fb = win->content_buffer;
                    for (int y = min_y; y < max_y; y++) {
                        for (int x = min_x; x < max_x; x++) {
                            fb[(CANVAS_Y + y) * win->width + (CANVAS_X + x)] = state->canvas_buffer[y * CANVAS_W + x];
                        }
                    }
                    dirty_rect_add(win->x + CANVAS_X + min_x, win->y + CANVAS_Y + min_y, max_x - min_x, max_y - min_y);
                } else {
                    win->needs_redraw = true;
                }
            } else if (state->is_drawing) {

            if (state->current_tool == TOOL_BRUSH || state->current_tool == TOOL_ERASER) {
                int min_x = cx - state->brush_size - 2;
                int min_y = cy - state->brush_size - 2;
                int max_x = cx + state->brush_size + 2;
                int max_y = cy + state->brush_size + 2;
                
                if (state->last_x != -1 && state->last_y != -1) {
                    if (state->last_x - state->brush_size - 2 < min_x) min_x = state->last_x - state->brush_size - 2;
                    if (state->last_y - state->brush_size - 2 < min_y) min_y = state->last_y - state->brush_size - 2;
                    if (state->last_x + state->brush_size + 2 > max_x) max_x = state->last_x + state->brush_size + 2;
                    if (state->last_y + state->brush_size + 2 > max_y) max_y = state->last_y + state->brush_size + 2;
                }
                
                draw_on_canvas(state, cx, cy, false);
                state->last_x = cx;
                state->last_y = cy;
                
                if (min_x < 0) min_x = 0; if (min_y < 0) min_y = 0;
                if (max_x > CANVAS_W) max_x = CANVAS_W; if (max_y > CANVAS_H) max_y = CANVAS_H;
                
                uint32_t *fb = win->content_buffer;
                for (int y = min_y; y < max_y; y++) {
                    for (int x = min_x; x < max_x; x++) {
                        fb[(CANVAS_Y + y) * win->width + (CANVAS_X + x)] = state->canvas_buffer[y * CANVAS_W + x];
                    }
                }
                dirty_rect_add(win->x + CANVAS_X + min_x, win->y + CANVAS_Y + min_y, max_x - min_x, max_y - min_y);
            } else {
                draw_on_canvas(state, cx, cy, true);
                win->needs_redraw = true;
            }
        }
    } else {
        if (state->is_drawing) {
                if (state->current_tool == TOOL_LINE || state->current_tool == TOOL_RECT || state->current_tool == TOOL_CIRCLE) {
                    draw_on_canvas(state, cx, cy, false);
                }
                state->is_drawing = false;
                win->needs_redraw = true;
            }
        }
    }
}

static void paint_on_close(struct window *win) {
    struct paint_state *state = (struct paint_state *)win->user_data;
    if (state) {
        if (state->canvas_buffer) kfree(state->canvas_buffer);
        if (state->preview_buffer) kfree(state->preview_buffer);
        for(int i=0; i<MAX_UNDO; i++) {
            if(state->history[i]) kfree(state->history[i]);
        }
        kfree(state);
    }
}

struct window *paint_open(void) {
    struct window *win = wm_create_window("Zarqaa Studio", 50, 20, 800, 510);
    if (!win) return NULL;

    struct paint_state *state = kcalloc(1, sizeof(struct paint_state));
    if (!state) {
        wm_destroy_window(win);
        return NULL;
    }

    state->canvas_buffer = kcalloc(CANVAS_W * CANVAS_H, 4);
    state->preview_buffer = kcalloc(CANVAS_W * CANVAS_H, 4);
    
    if (!state->canvas_buffer || !state->preview_buffer) {
        if (state->canvas_buffer) kfree(state->canvas_buffer);
        if (state->preview_buffer) kfree(state->preview_buffer);
        kfree(state);
        wm_destroy_window(win);
        return NULL;
    }
    
    for(int i=0; i<CANVAS_W * CANVAS_H; i++) {
        state->canvas_buffer[i] = 0xFFFFFFFF;
    }
    
    state->current_tool = TOOL_BRUSH;
    state->current_color = palette[0];
    state->brush_size = 3;
    state->is_drawing = false;
    state->history_count = 0;

    win->user_data = state;
    win->on_draw = paint_on_draw;
    win->on_mouse = paint_on_mouse;
    win->on_close = paint_on_close;
    
    return win;
}
