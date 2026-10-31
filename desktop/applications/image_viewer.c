/*
 * Zarqaa OS 0.1 - Modern Image Viewer / Gallery
 * Creator: Abdelhak Chabbi
 *
 * Uses stb_image to decode images and renders them with aspect-ratio-preserving
 * scaling in a modern frameless/dark window, with 1-click Wallpaper Setting.
 */

#include "desktop/window_manager.h"
#include "desktop/desktop.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

extern uint32_t *gfx_load_image(const char *path, int *out_w, int *out_h);

struct image_viewer_state {
    char filepath[256];
    uint32_t *img_pixels;
    int img_w;
    int img_h;
    bool hover_set_wp;
    bool wp_set_toast;
};

static void iv_on_draw(struct window *win) {
    struct image_viewer_state *st = (struct image_viewer_state *)win->user_data;
    
    fb_set_render_target(win->content_buffer, win->width, win->height, win->width * 4);
    
    // Modern Dark Background
    fb_fill_rect(0, 0, win->width, win->height, 0xFF0B1017);
    
    if (!st->img_pixels) {
        if (st->filepath[0] == '\0') {
            fb_draw_string_transparent(win->width / 2 - 60, win->height / 2 - 10, "Zarqaa Gallery", 0xFF00E5FF);
            fb_draw_string_transparent(win->width / 2 - 110, win->height / 2 + 15, "Open an image from the File Manager", 0xFF8098B3);
        } else {
            fb_draw_string_transparent(win->width / 2 - 70, win->height / 2, "Failed to load image", 0xFFFA5C5C);
        }
        fb_reset_render_target();
        return;
    }

    // Top Action Toolbar
    int tb_h = 36;
    gfx_draw_glass_panel(0, 0, win->width, tb_h, 0, 0xFF121B26, 0xFF2A3A4C, 230);

    // Image resolution badge
    char dim_str[64];
    // Simple integer-to-string conversion for width & height
    char w_buf[16], h_buf[16];
    int tw = st->img_w, th = st->img_h;
    int wi = 0, hi = 0;
    if (tw == 0) w_buf[wi++] = '0';
    else { char temp[16]; int ti=0; while(tw>0){temp[ti++]='0'+(tw%10); tw/=10;} while(ti>0) w_buf[wi++]=temp[--ti]; }
    w_buf[wi] = '\0';
    if (th == 0) h_buf[hi++] = '0';
    else { char temp[16]; int ti=0; while(th>0){temp[ti++]='0'+(th%10); th/=10;} while(ti>0) h_buf[hi++]=temp[--ti]; }
    h_buf[hi] = '\0';

    strncpy(dim_str, w_buf, sizeof(dim_str) - 1);
    strncat(dim_str, " x ", sizeof(dim_str) - strlen(dim_str) - 1);
    strncat(dim_str, h_buf, sizeof(dim_str) - strlen(dim_str) - 1);
    fb_draw_string_transparent(15, 11, dim_str, 0xFF8098B3);

    // "Set as Wallpaper" Button
    int btn_w = 170;
    int btn_h = 24;
    int btn_x = win->width - btn_w - 15;
    int btn_y = 6;

    color_t btn_bg = st->wp_set_toast ? 0xFF1F5C3E : (st->hover_set_wp ? 0xFF00A0C0 : 0xFF00E5FF);
    color_t btn_fg = st->wp_set_toast ? 0xFF5CFA92 : (st->hover_set_wp ? 0xFFFFFFFF : 0xFF000000);

    gfx_draw_rounded_rect_filled(btn_x, btn_y, btn_w, btn_h, 6, btn_bg);
    const char *btn_text = st->wp_set_toast ? "[ Set as Wallpaper! ]" : "[ Set as Wallpaper ]";
    fb_draw_string_transparent(btn_x + 12, btn_y + 5, btn_text, btn_fg);
    
    // Viewport area for the image
    int view_y = tb_h + 10;
    int view_h = win->height - view_y - 10;
    int view_w = win->width - 20;
    if (view_h <= 0 || view_w <= 0) {
        fb_reset_render_target();
        return;
    }

    // Calculate scaling maintaining aspect ratio
    int scale_x = (view_w * 1000) / st->img_w;
    int scale_y = (view_h * 1000) / st->img_h;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale > 1000) scale = 1000; // Don't upscale past 1:1
    
    int draw_w = (st->img_w * scale) / 1000;
    int draw_h = (st->img_h * scale) / 1000;
    if (draw_w <= 0) draw_w = 1;
    if (draw_h <= 0) draw_h = 1;
    
    int offset_x = (win->width - draw_w) / 2;
    int offset_y = view_y + (view_h - draw_h) / 2;
    
    // Draw the image
    for (int y = 0; y < draw_h; y++) {
        int src_y = (y * st->img_h) / draw_h;
        for (int x = 0; x < draw_w; x++) {
            int src_x = (x * st->img_w) / draw_w;
            uint32_t color = st->img_pixels[src_y * st->img_w + src_x];
            uint8_t a = (color >> 24) & 0xFF;
            if (a == 255) {
                fb_putpixel(offset_x + x, offset_y + y, color);
            } else if (a > 0) {
                color_t bg = fb_getpixel(offset_x + x, offset_y + y);
                uint32_t r = ((color >> 16) & 0xFF) * a / 255 + ((bg >> 16) & 0xFF) * (255 - a) / 255;
                uint32_t g = ((color >> 8) & 0xFF) * a / 255 + ((bg >> 8) & 0xFF) * (255 - a) / 255;
                uint32_t b = (color & 0xFF) * a / 255 + (bg & 0xFF) * (255 - a) / 255;
                fb_putpixel(offset_x + x, offset_y + y, 0xFF000000 | (r << 16) | (g << 8) | b);
            }
        }
    }
    
    fb_reset_render_target();
}

static void iv_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    struct image_viewer_state *st = (struct image_viewer_state *)win->user_data;
    if (!st || !st->img_pixels) return;

    int btn_w = 170;
    int btn_h = 24;
    int btn_x = win->width - btn_w - 15;
    int btn_y = 6;

    bool hover = (mx >= btn_x && mx <= btn_x + btn_w && my >= btn_y && my <= btn_y + btn_h);
    bool needs_redraw = false;

    if (st->hover_set_wp != hover) {
        st->hover_set_wp = hover;
        needs_redraw = true;
    }

    if (hover && left) {
        desktop_set_wallpaper_pixels(st->img_pixels, st->img_w, st->img_h);
        st->wp_set_toast = true;
        needs_redraw = true;
    }

    if (needs_redraw) {
        win->needs_redraw = true;
    }
}

static void iv_on_close(struct window *win) {
    if (win->user_data) {
        struct image_viewer_state *st = (struct image_viewer_state *)win->user_data;
        if (st->img_pixels) {
            kfree(st->img_pixels);
        }
        kfree(win->user_data);
    }
}

struct window *image_viewer_open_file(const char *path) {
    int w = 800;
    int h = 530;
    if (fb_get_width() < 800) w = fb_get_width() - 40;
    if (fb_get_height() < 600) h = fb_get_height() - 40;

    struct window *win = wm_create_window("Zarqaa Gallery", 
                                          (fb_get_width() - w) / 2, 
                                          (fb_get_height() - h) / 2, 
                                          w, h);
    if (!win) return NULL;
    
    win->frameless = false; 

    struct image_viewer_state *st = kcalloc(1, sizeof(struct image_viewer_state));
    if (path) {
        strncpy(st->filepath, path, sizeof(st->filepath) - 1);
        st->img_pixels = gfx_load_image(path, &st->img_w, &st->img_h);
        
        if (st->img_pixels) {
            char new_title[128] = "Gallery - ";
            const char *filename = path;
            for (int i = strlen(path) - 1; i >= 0; i--) {
                if (path[i] == '/') {
                    filename = &path[i + 1];
                    break;
                }
            }
            strncat(new_title, filename, 64);
            strncpy(win->title, new_title, 63);
            win->title[63] = '\0';
        }
    }

    win->user_data = st;
    win->on_draw = iv_on_draw;
    win->on_mouse = iv_on_mouse;
    win->on_close = iv_on_close;
    return win;
}

struct window *image_viewer_open(void) {
    return image_viewer_open_file(NULL);
}
