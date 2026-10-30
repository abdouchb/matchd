/*
 * Zarqaa OS 0.1 - Window Manager
 * Creator: Abdelhak Chabbi
 */

#include "desktop/window_manager.h"
#include "drivers/framebuffer.h"
#include "drivers/usb/usb_hid.h"
#include "gui/graphics.h"
#include "gui/dirty_rect.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"
#include "gui/zml_smart_renderer.h"

// --- Custom Geometry Drawing Helpers for Theme ---
static void gfx_draw_triangle_up(int cx, int cy, color_t fill, color_t stroke) {
    for (int y = 0; y < 13; y++) {
        int w = (y * 14) / 13;
        fb_draw_line(cx - w/2, cy - 6 + y, cx + w/2, cy - 6 + y, fill);
    }
    fb_draw_line(cx, cy - 7, cx + 7, cy + 6, stroke);
    fb_draw_line(cx, cy - 7, cx - 7, cy + 6, stroke);
    fb_draw_line(cx - 7, cy + 6, cx + 7, cy + 6, stroke);
}

static void gfx_draw_triangle_down(int cx, int cy, color_t fill, color_t stroke) {
    for (int y = 0; y < 13; y++) {
        int w = (y * 14) / 13;
        fb_draw_line(cx - w/2, cy + 6 - y, cx + w/2, cy + 6 - y, fill);
    }
    fb_draw_line(cx, cy + 7, cx + 7, cy - 6, stroke);
    fb_draw_line(cx, cy + 7, cx - 7, cy - 6, stroke);
    fb_draw_line(cx - 7, cy - 6, cx + 7, cy - 6, stroke);
}


static struct window windows[MAX_WINDOWS];
static int z_order[MAX_WINDOWS];
static int window_count = 0;
static int next_win_id = 1;

static char wm_clipboard_data[8192] = {0};

void wm_set_clipboard(const char *text) {
    if (!text) return;
    strncpy(wm_clipboard_data, text, sizeof(wm_clipboard_data) - 1);
    wm_clipboard_data[sizeof(wm_clipboard_data) - 1] = '\0';
}

const char *wm_get_clipboard(void) {
    return wm_clipboard_data;
}

bool wm_enable_shadows = false; // Disable slow shadows to fix FPS drops

void wm_init(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        windows[i].id = 0;
        z_order[i] = -1;
    }
}

struct window *wm_create_window(const char *title, int x, int y, int w, int h) {
    if (window_count >= MAX_WINDOWS) return NULL;

    int slot = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].id == 0) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return NULL;

    struct window *win = &windows[slot];
    win->id = next_win_id++;
    strncpy(win->title, title, 63);
    win->title[63] = '\0';
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->visible = true;
    win->minimized = false;
    win->maximized = false;
    win->focused = true;
    win->dragging = false;
    win->needs_redraw = true;
    win->frameless = false;
    win->content_buffer = (uint32_t *)kmalloc(w * h * 4);
    if (win->content_buffer) {
        memset(win->content_buffer, 0, w * h * 4);
    }
    surface_init(&win->surface, x, y, w, h);
    win->on_draw = NULL;
    win->on_key = NULL;
    win->on_mouse = NULL;
    win->on_close = NULL;
    win->user_data = NULL;

    z_order[window_count++] = slot;
    kprintf("[WM] Window Created: '%s' (ID: %d) at X:%d Y:%d W:%d H:%d\n", title, win->id, x, y, w, h);
    
    dirty_rect_add(x - 10, y - TITLEBAR_HEIGHT - 10, w + 20, h + TITLEBAR_HEIGHT + 20);
    
    wm_focus_window(win);
    return win;
}

void wm_destroy_window(struct window *win) {
    if (!win || win->id == 0) return;

    kprintf("[WM] Window Destroyed: '%s' (ID: %d)\n", win->title, win->id);

    // Add dirty rect before destroying so the background redraws over it
    dirty_rect_add(win->x - 5, win->y - TITLEBAR_HEIGHT - 5, win->width + 10, win->height + TITLEBAR_HEIGHT + 10);

    kprintf("[WM] Destroy calling on_close\n");
    if (win->on_close) win->on_close(win);
    kprintf("[WM] Destroy calling surface_destroy\n");
    surface_destroy(&win->surface);
    if (win->content_buffer) {
        kfree(win->content_buffer);
        win->content_buffer = NULL;
    }
    kprintf("[WM] Destroy finishing\n");

    win->id = 0;
    
    // Remove from z_order
    int target = -1;
    for (int i = 0; i < window_count; i++) {
        if (&windows[z_order[i]] == win) {
            target = i;
            break;
        }
    }
    
    if (target != -1) {
        for (int i = target; i < window_count - 1; i++) {
            z_order[i] = z_order[i + 1];
        }
        window_count--;
    }
}

void wm_focus_window(struct window *win) {
    if (!win || win->id == 0) return;
    
    // Only log if it wasn't already focused to avoid spam
    if (!win->focused) {
        kprintf("[WM] Window Focused: '%s' (ID: %d)\n", win->title, win->id);
    }
    
    for (int i = 0; i < window_count; i++) {
        windows[z_order[i]].focused = false;
    }
    win->focused = true;
    
    // Move to front (highest z-order)
    int target = -1;
    for (int i = 0; i < window_count; i++) {
        if (&windows[z_order[i]] == win) {
            target = i;
            break;
        }
    }
    if (target != -1) {
        int slot = z_order[target];
        for (int i = target; i < window_count - 1; i++) {
            z_order[i] = z_order[i + 1];
        }
        z_order[window_count - 1] = slot;
    }
    
    dirty_rect_add(win->x - 10, win->y - TITLEBAR_HEIGHT - 10, win->width + 20, win->height + TITLEBAR_HEIGHT + 20);
}

struct window *wm_get_focused(void) {
    if (window_count == 0) return NULL;
    return &windows[z_order[window_count - 1]];
}

struct window *wm_find_window_by_title(const char *title) {
    for (int i = 0; i < window_count; i++) {
        struct window *win = &windows[z_order[i]];
        if (strcmp(win->title, title) == 0) {
            return win;
        }
    }
    return NULL;
}

bool wm_has_maximized_window(void) {
    for (int i = 0; i < window_count; i++) {
        struct window *win = &windows[i];
        if (win->id != 0 && win->visible && !win->minimized && win->maximized) {
            return true;
        }
    }
    return false;
}

bool wm_update(bool left_clicked) {
    int mx = mouse_get_x();
    int my = mouse_get_y();
    bool left_pressed = mouse_left_pressed();
    bool right_clicked = mouse_right_clicked();

    // Check keyboard input for focused window
    struct window *focused = wm_get_focused();
    if (focused && focused->on_key) {
        struct key_event ev;
        while (keyboard_poll_event(&ev)) {
            if (!ev.is_release) {
                focused->on_key(focused, ev.c, ev.scancode);
                focused->needs_redraw = true;
                dirty_rect_add(focused->x - 5, focused->y - TITLEBAR_HEIGHT - 5, focused->width + 10, focused->height + TITLEBAR_HEIGHT + 10);
            }
        }
    }

    // Process mouse from top to bottom
    bool handled_mouse = false;
    
    // Check dragging
    for (int i = window_count - 1; i >= 0; i--) {
        struct window *win = &windows[z_order[i]];
        if (!win->visible || win->minimized) continue;
        
        if (win->dragging) {
            if (!left_pressed) {
                win->dragging = false;
                
                // Aero Snap Logic
                int sw = fb_get_width();
                int sh = fb_get_height();
                
                if (mx <= 5 || mx >= sw - 5 || my <= 5) {
                    int old_x = win->x;
                    int old_y = win->y;
                    int old_w = win->width;
                    int old_h = win->height;
                    
                    win->saved_x = win->x;
                    win->saved_y = win->y;
                    win->saved_w = win->width;
                    win->saved_h = win->height;
                    
                    if (mx <= 5) { // Left Snap
                        win->x = 0;
                        win->y = TITLEBAR_HEIGHT + 30;
                        win->width = sw / 2;
                        win->height = sh - win->y - 50;
                    } else if (mx >= sw - 5) { // Right Snap
                        win->x = sw / 2;
                        win->y = TITLEBAR_HEIGHT + 30;
                        win->width = sw / 2;
                        win->height = sh - win->y - 50;
                    } else if (my <= 5) { // Top Snap
                        win->x = 0;
                        win->y = TITLEBAR_HEIGHT + 30;
                        win->width = sw;
                        win->height = sh - win->y - 50;
                    }
                    
                    win->maximized = true;
                    dirty_rect_add(old_x - 5, old_y - TITLEBAR_HEIGHT - 5, old_w + 10, old_h + TITLEBAR_HEIGHT + 10);
                    dirty_rect_add(win->x - 5, win->y - TITLEBAR_HEIGHT - 5, win->width + 10, win->height + TITLEBAR_HEIGHT + 10);
                    surface_destroy(&win->surface);
                    if (win->content_buffer) kfree(win->content_buffer);
                    win->content_buffer = (uint32_t *)kmalloc(win->width * win->height * 4);
                    if (win->content_buffer) memset(win->content_buffer, 0, win->width * win->height * 4);
                    surface_init(&win->surface, win->x, win->y, win->width, win->height);
                    win->needs_redraw = true;
                }
            } else {
                int new_x = mx - win->drag_offset_x;
                int new_y = my - win->drag_offset_y;
                if (new_y < TITLEBAR_HEIGHT) new_y = TITLEBAR_HEIGHT; // Clamp to top bar
                
                if (win->x != new_x || win->y != new_y) {
                    // Add dirty rect for old position
                    dirty_rect_add(win->x - 5, win->y - TITLEBAR_HEIGHT - 5, win->width + 10, win->height + TITLEBAR_HEIGHT + 10);
                    win->x = new_x;
                    win->y = new_y;
                    // Add dirty rect for new position
                    dirty_rect_add(win->x - 5, win->y - TITLEBAR_HEIGHT - 5, win->width + 10, win->height + TITLEBAR_HEIGHT + 10);
                }
            }
            handled_mouse = true;
            break;
        }
    }

    if (!handled_mouse) {
        for (int i = window_count - 1; i >= 0; i--) {
            struct window *win = &windows[z_order[i]];
            if (!win->visible || win->minimized) continue;

            // Check decorations (only if clicked and not frameless)
            if (left_clicked && !win->frameless && mx >= win->x && mx < win->x + win->width &&
                my >= win->y - TITLEBAR_HEIGHT && my < win->y) {
                
                wm_focus_window(win);
                
                // 1. Minimize button (X: -101)
                if (mx >= win->x + win->width - 110 && mx <= win->x + win->width - 90) {
                    win->minimized = true;
                    // Add dirty rect for the whole window so it clears
                    dirty_rect_add(win->x - 10, win->y - TITLEBAR_HEIGHT - 10, win->width + 20, win->height + TITLEBAR_HEIGHT + 20);
                }
                // 2. Close button (X: -75)
                else if (mx >= win->x + win->width - 85 && mx <= win->x + win->width - 65) {
                    wm_destroy_window(win);
                } 
                // 3. Maximize button (X: -49)
                else if (mx >= win->x + win->width - 59 && mx <= win->x + win->width - 39) {
                    int old_x = win->x;
                    int old_y = win->y;
                    int old_w = win->width;
                    int old_h = win->height;

                    if (win->maximized) {
                        win->x = win->saved_x;
                        win->y = win->saved_y;
                        win->width = win->saved_w;
                        win->height = win->saved_h;
                        win->maximized = false;
                    } else {
                        win->saved_x = win->x;
                        win->saved_y = win->y;
                        win->saved_w = win->width;
                        win->saved_h = win->height;
                        win->x = 0;
                        win->y = TITLEBAR_HEIGHT + 30; // Below top bar
                        win->width = fb_get_width();
                        win->height = fb_get_height() - (TITLEBAR_HEIGHT + 30); // Fill to bottom
                        win->maximized = true;
                    }

                    // Add dirty rects for both old and new states
                    dirty_rect_add(old_x - 5, old_y - TITLEBAR_HEIGHT - 5, old_w + 10, old_h + TITLEBAR_HEIGHT + 10);
                    dirty_rect_add(win->x - 5, win->y - TITLEBAR_HEIGHT - 5, win->width + 10, win->height + TITLEBAR_HEIGHT + 10);

                    // Reallocate window content buffer to match new dimensions
                    surface_destroy(&win->surface);
                    if (win->content_buffer) kfree(win->content_buffer);
                    win->content_buffer = (uint32_t *)kmalloc(win->width * win->height * 4);
                    if (win->content_buffer) memset(win->content_buffer, 0, win->width * win->height * 4);
                    surface_init(&win->surface, win->x, win->y, win->width, win->height);
                    win->needs_redraw = true;
                }
                // Minimize button
                else if (mx >= win->x + win->width - 90) {
                    dirty_rect_add(win->x - 5, win->y - TITLEBAR_HEIGHT - 5, win->width + 10, win->height + TITLEBAR_HEIGHT + 10);
                    win->minimized = true;
                }
                // Drag titlebar (with un-snap restore)
                else {
                    if (win->maximized) {
                        int old_x = win->x, old_y = win->y, old_w = win->width, old_h = win->height;
                        
                        win->width = win->saved_w;
                        win->height = win->saved_h;
                        win->maximized = false;
                        
                        // Center titlebar under mouse
                        win->x = mx - (win->width / 2);
                        if (win->x < 0) win->x = 0;
                        if (win->x + win->width > fb_get_width()) win->x = fb_get_width() - win->width;
                        win->y = my; // Snap exactly under mouse
                        
                        dirty_rect_add(old_x - 5, old_y - TITLEBAR_HEIGHT - 5, old_w + 10, old_h + TITLEBAR_HEIGHT + 10);
                        dirty_rect_add(win->x - 5, win->y - TITLEBAR_HEIGHT - 5, win->width + 10, win->height + TITLEBAR_HEIGHT + 10);
                        
                        surface_destroy(&win->surface);
                        if (win->content_buffer) kfree(win->content_buffer);
                        win->content_buffer = (uint32_t *)kmalloc(win->width * win->height * 4);
                        if (win->content_buffer) memset(win->content_buffer, 0, win->width * win->height * 4);
                        surface_init(&win->surface, win->x, win->y, win->width, win->height);
                        win->needs_redraw = true;
                    }
                    
                    win->dragging = true;
                    win->drag_offset_x = mx - win->x;
                    win->drag_offset_y = my - win->y;
                }
                handled_mouse = true;
                break;
            }

            // Check window body (for both hover and click)
            if (mx >= win->x && mx < win->x + win->width &&
                my >= win->y && my < win->y + win->height) {
                if (left_clicked) wm_focus_window(win);
                if (win->on_mouse) {
                    win->on_mouse(win, mx - win->x, my - win->y, left_clicked, right_clicked);
                    win->needs_redraw = true;
                    dirty_rect_add(win->x, win->y, win->width, win->height);
                }
                handled_mouse = true;
                break;
            }
        }
    }

    // Call on_draw for all visible windows into their private buffers
    for (int i = 0; i < window_count; i++) {
        struct window *win = &windows[z_order[i]];
        if (!win->visible || win->minimized) continue;
        
        if (win->on_tick) {
            win->on_tick(win);
        }
        
        if (win->on_draw && win->needs_redraw) {
            win->needs_redraw = false;
            
            /* Update surface context transform to map window coordinates to screen (Disabled, we now do this in wm_render) */
            // win->surface.context.transform.m[0][2] = win->x;
            // win->surface.context.transform.m[1][2] = win->y;
            
            render_queue_clear(&win->surface.queue);
            
            gapi_set_current_context(&win->surface.context);
            win->on_draw(win);
            gapi_set_current_context(NULL);
            
            dirty_rect_add(win->x - 5, win->y - TITLEBAR_HEIGHT - 5, win->width + 10, win->height + TITLEBAR_HEIGHT + 10);
        }
    }
    
    return handled_mouse;
}

void wm_render(void) {
    for (int i = 0; i < window_count; i++) {
        struct window *win = &windows[z_order[i]];
        if (!win->visible || win->minimized) continue;

        // Bounding box culling: Check if window intersects the current dirty rectangle!
        if (!fb_intersects_clip(win->x - 10, win->y - TITLEBAR_HEIGHT - 10, 
                                win->width + 20, win->height + TITLEBAR_HEIGHT + 20)) {
            continue;
        }

        // Glass window base styling
        if (!win->maximized && !win->frameless && wm_enable_shadows) {
            gfx_draw_rounded_rect_filled(win->x + 4, win->y - TITLEBAR_HEIGHT + 4, 
                                         win->width, win->height + TITLEBAR_HEIGHT, 16, 0x60000000);
        }

        // Window Base (Glassmorphism + Border)
        if (!win->frameless) {
            if (win->maximized) {
                fb_fill_rect(win->x, win->y - TITLEBAR_HEIGHT, win->width, win->height + TITLEBAR_HEIGHT, 0xFF0D1826);
                fb_draw_line(win->x, win->y - 1, win->x + win->width - 1, win->y - 1, 0x1AFFFFFF);
            } else {
                // Main Glass Panel (Titlebar + Body)
                gfx_draw_glass_panel(win->x, win->y - TITLEBAR_HEIGHT, win->width, win->height + TITLEBAR_HEIGHT, 16, 
                                     0xFFFFFF, 0x33FFFFFF, 0x1A); // 0x1A ~ 10% opacity white
                // Titlebar Separator (Soft line)
                fb_draw_line(win->x, win->y - 1, win->x + win->width - 1, win->y - 1, 0x1AFFFFFF);
            }

            // Title text (Modern glowing text for focused window)
            fb_draw_string_transparent(win->x + 16, win->y - 20, win->title, win->focused ? 0xFFFFFFFF : 0xFFA0B4C8);

            // Geometric System Buttons (Right Aligned)
            int cy = win->y - (TITLEBAR_HEIGHT / 2);
            int right_edge = win->x + win->width;

            // 1. Minimize (Line) - X: 799 (relative -101)
            fb_draw_line(right_edge - 107, cy + 6, right_edge - 95, cy + 6, 0xFF1E293B);
            fb_draw_line(right_edge - 107, cy + 5, right_edge - 95, cy + 5, 0xFF1E293B); // 2px width

            // 2. Close (Red Triangle Up) - X: 825 (relative -75)
            gfx_draw_triangle_up(right_edge - 75, cy, 0xFFF87171, 0xFF7F1D1D);

            // 3. Maximize (Yellow Triangle Down) - X: 851 (relative -49)
            gfx_draw_triangle_down(right_edge - 49, cy, 0xFFFACC15, 0xFF713F12);

            // 4. Fullscreen (Green Triangle Up) - X: 877 (relative -23)
            gfx_draw_triangle_up(right_edge - 23, cy, 0xFF4ADE80, 0xFF064E3B);
        }

        // Window Body is now in win->surface.queue. We append its commands to the global queue!
        if (win->content_buffer) {
            RenderCommand *cmd = render_queue_push(&g_default_render_queue);
            if (cmd) {
                cmd->opcode = RENDER_OP_DRAW_IMAGE;
                cmd->image.x = win->x;
                cmd->image.y = win->y;
                cmd->image.w = win->width;
                cmd->image.h = win->height;
                cmd->image.pixels = win->content_buffer;
            }
        }
        
        if (win->surface.queue.head) {
            RenderQueueChunk *src_curr = win->surface.queue.head;
            while (src_curr) {
                for (int c = 0; c < src_curr->count; c++) {
                    RenderCommand *cmd = render_queue_push(&g_default_render_queue);
                    if (cmd) {
                        *cmd = src_curr->commands[c];
                        switch (cmd->opcode) {
                            case RENDER_OP_FILL_RECT:
                            case RENDER_OP_DRAW_RECT:
                                cmd->rect.x += win->x;
                                cmd->rect.y += win->y;
                                break;
                            case RENDER_OP_DRAW_LINE:
                                cmd->line.x0 += win->x;
                                cmd->line.y0 += win->y;
                                cmd->line.x1 += win->x;
                                cmd->line.y1 += win->y;
                                break;
                            case RENDER_OP_DRAW_STRING:
                            case RENDER_OP_DRAW_STRING_TRANS:
                                cmd->text.x += win->x;
                                cmd->text.y += win->y;
                                break;
                            case RENDER_OP_DRAW_IMAGE:
                                cmd->image.x += win->x;
                                cmd->image.y += win->y;
                                break;
                        }
                    }
                }
                src_curr = src_curr->next;
            }
        }

        // Draw Ultra-Smooth Anti-Aliased Window Border (Glass style)
        if (!win->maximized && !win->frameless) {
            color_t glass_border = win->focused ? 0x4DFFFFFF : 0x26FFFFFF; // 30% vs 15% white
            gfx_draw_rounded_rect(win->x, win->y - TITLEBAR_HEIGHT, win->width, win->height + TITLEBAR_HEIGHT, 16, glass_border);
            if (win->focused) {
                // Subtle inner reflection
                gfx_draw_rounded_rect(win->x + 1, win->y - TITLEBAR_HEIGHT + 1, win->width - 2, win->height + TITLEBAR_HEIGHT - 2, 15, 0x1AFFFFFF);
            }
        }
    }
}
