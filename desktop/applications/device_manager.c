/*
 * Zarqaa OS 0.1 - Device Manager Application
 * Creator: Abdelhak Chabbi & AI
 */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/graphics.h"
#include "drivers/gpu_renderer.h"
#include "drivers/font.h"
#include "kernel/pci.h"
#include "drivers/framebuffer.h"

#define COLOR_TEXT_PRIMARY 0xFFE0E0E0
#define COLOR_TEXT_SECONDARY 0xFFA0A0A0
#define COLOR_ACCENT 0xFF007ACC
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_SUCCESS 0xFF28A745
#define COLOR_ERROR 0xFFDC3545
#include "zarqaa/string.h"

struct devmgr_state {
    int scroll_y;
};

static void devmgr_draw(struct window *win) {
    if (!win) return;
    struct devmgr_state *state = (struct devmgr_state *)win->user_data;

    fb_set_render_target(win->content_buffer, win->width, win->height, win->width * 4);

    int cx = 0;
    int cy = 0;
    int cw = win->width;
    int ch = win->height;

    // Background
    fb_fill_rect(cx, cy, cw, ch, 0xFF222222);

    // Header
    fb_fill_rect(cx, cy, cw, 40, 0xFF333333);
    fb_draw_string_transparent(cx + 15, cy + 12, "ZarqaaOS Device Manager", COLOR_WHITE);

    // List PCI Devices
    int y_offset = cy + 50 - state->scroll_y;
    
    uint32_t count = pci_get_device_count();
    const pci_device_t *devices = pci_get_devices();

    char buf[128];
    snprintf(buf, sizeof(buf), "Found %d PCI Devices:", count);
    if (y_offset > cy + 40 && y_offset < cy + ch - 20) {
        fb_draw_string_transparent(cx + 15, y_offset, buf, COLOR_TEXT_PRIMARY);
    }
    y_offset += 30;

    for (uint32_t i = 0; i < count; i++) {
        if (y_offset > cy + ch) break; // Out of bounds bottom
        
        if (y_offset > cy + 40) { // Out of bounds top
            snprintf(buf, sizeof(buf), "[%02x:%02x.%x] Class: %02x, Sub: %02x | Vendor: %04x, Device: %04x",
                     devices[i].bus, devices[i].device, devices[i].function,
                     devices[i].class_code, devices[i].subclass,
                     devices[i].vendor_id, devices[i].device_id);
                     
            fb_draw_string_transparent(cx + 25, y_offset, buf, COLOR_TEXT_SECONDARY);
            
            // Basic identification
            const char *desc = "Unknown Device";
            if (devices[i].class_code == 0x01) desc = "Mass Storage Controller";
            else if (devices[i].class_code == 0x02) desc = "Network Controller";
            else if (devices[i].class_code == 0x03) desc = "Display Controller";
            else if (devices[i].class_code == 0x04) desc = "Multimedia Controller";
            else if (devices[i].class_code == 0x06) desc = "Bridge Device";
            else if (devices[i].class_code == 0x0C) desc = "Serial Bus Controller (USB/Firewire)";
            
            fb_draw_string_transparent(cx + 450, y_offset, desc, 0xFFAAAAAA);
            
            fb_fill_rect(cx + 15, y_offset + 20, cw - 30, 1, 0xFF444444);
        }
        
        y_offset += 30;
    }

    fb_reset_render_target();
}

static void devmgr_close(struct window *win) {
    if (win) {
        if (win->user_data) {
            kfree(win->user_data);
            win->user_data = NULL;
        }
    }
}

struct window *device_manager_open(void) {
    struct window *win = wm_create_window("Device Manager", 100, 100, 750, 500);
    if (!win) return NULL;

    struct devmgr_state *state = (struct devmgr_state *)kmalloc(sizeof(struct devmgr_state));
    state->scroll_y = 0;

    win->user_data = state;
    win->on_draw = devmgr_draw;
    win->on_close = devmgr_close;

    return win;
}
