/*
 * Zarqaa OS 0.1 - WiFi Manager Application
 * Creator: Abdelhak Chabbi
 *
 * Modern glassmorphism WiFi network manager with
 * scan, connect, disconnect, and signal visualization.
 */

#include "desktop/window_manager.h"
#include "drivers/framebuffer.h"
#include "drivers/network.h"
#include "gui/graphics.h"
#include "zarqaa/string.h"
#include "kernel/heap.h"
#include "drivers/pit.h"
#include "drivers/font.h"

struct wifi_state {
    int selected;
    int scroll_y;
    bool show_details;
};

/* Draw signal bars icon */
static void draw_signal_bars(int x, int y, int signal, color_t color) {
    int bars = 0;
    if (signal > 20) bars = 1;
    if (signal > 40) bars = 2;
    if (signal > 60) bars = 3;
    if (signal > 80) bars = 4;

    for (int i = 0; i < 4; i++) {
        int bh = 4 + i * 4;    /* bar height: 4, 8, 12, 16 */
        int bx = x + i * 6;
        int by = y + 16 - bh;
        color_t c = (i < bars) ? color : 0xFF334455;
        fb_fill_rect(bx, by, 4, bh, c);
    }
}

/* Draw lock icon (small padlock) */
static void draw_lock_icon(int x, int y, color_t color) {
    /* Shackle (arc) */
    fb_draw_rect(x + 2, y, 8, 5, color);
    /* Body */
    fb_fill_rect(x, y + 5, 12, 8, color);
    /* Keyhole */
    fb_fill_rect(x + 4, y + 7, 4, 4, 0xFF0A1520);
}

static void wifi_draw(struct window *win) {
    struct wifi_state *ws = (struct wifi_state *)win->user_data;
    struct network_state *ns = network_get_state();

    /* Background */
    fb_fill_rect(0, 0, win->width, win->height, 0xFF0A1520);

    /* ── Header: Connection Status ── */
    gfx_draw_glass_panel(0, 0, win->width, 70, 0, 0xFF0E1E30, 0xFF1A3550, 220);

    if (ns->status == NET_CONNECTED) {
        /* Connected indicator */
        gfx_draw_circle_filled(30, 35, 12, COLOR_SUCCESS);
        draw_signal_bars(22, 27, ns->networks[ns->connected_index].signal, 0xFF0A1520);
        fb_draw_string_transparent(55, 15, "Connected", COLOR_SUCCESS);
        fb_draw_string_transparent(55, 35, ns->networks[ns->connected_index].ssid, COLOR_TEXT_PRIMARY);
        
        /* Speed badge */
        char speed_str[16];
        itoa(ns->speed_mbps, speed_str, 10);
        strcat(speed_str, " Mbps");
        gfx_draw_rounded_rect_filled(win->width - 100, 25, 85, 22, 6, 0xFF1A3550);
        fb_draw_string_transparent(win->width - 90, 29, speed_str, COLOR_ACCENT);
    } else if (ns->status == NET_CONNECTING) {
        /* Connecting animation */
        gfx_draw_circle_filled(30, 35, 12, COLOR_WARNING);
        int dots = (pit_get_ticks() / 30) % 4;
        char conn_str[20] = "Connecting";
        for (int d = 0; d < dots; d++) strcat(conn_str, ".");
        fb_draw_string_transparent(55, 22, conn_str, COLOR_WARNING);
        win->needs_redraw = true; /* keep animating */
    } else {
        /* Disconnected */
        gfx_draw_circle_filled(30, 35, 12, 0xFF334455);
        fb_draw_string_transparent(22, 29, "X", 0xFF0A1520);
        fb_draw_string_transparent(55, 15, "Not Connected", COLOR_TEXT_SECONDARY);
        fb_draw_string_transparent(55, 35, "Select a network below", 0xFF556677);
    }

    /* ── Scan button ── */
    gfx_draw_rounded_rect_filled(win->width - 80, 78, 70, 24, 6, COLOR_ACCENT);
    fb_draw_string_transparent(win->width - 68, 82, "Scan", 0xFFFFFFFF);

    /* ── Section title ── */
    fb_draw_string_transparent(10, 82, "Available Networks", COLOR_TEXT_SECONDARY);
    fb_draw_line(10, 102, win->width - 10, 102, 0xFF1A3550);

    /* ── Network List ── */
    int list_y = 110 - ws->scroll_y;

    for (int i = 0; i < ns->network_count; i++) {
        int item_y = list_y + i * 50;

        /* Skip off-screen items */
        if (item_y + 50 < 105 || item_y > win->height) continue;

        struct wifi_network *net = &ns->networks[i];
        bool is_connected = net->connected;
        bool is_selected = (i == ws->selected);

        /* Item background */
        color_t item_bg = is_selected ? 0xFF152535 : 0xFF0D1925;
        if (is_connected) item_bg = 0xFF0D2520;
        gfx_draw_rounded_rect_filled(8, item_y, win->width - 16, 44, 8, item_bg);

        /* Signal bars */
        color_t sig_color = is_connected ? COLOR_SUCCESS : COLOR_ACCENT;
        draw_signal_bars(20, item_y + 14, net->signal, sig_color);

        /* SSID */
        color_t ssid_color = is_connected ? COLOR_SUCCESS : COLOR_TEXT_PRIMARY;
        fb_draw_string_transparent(52, item_y + 8, net->ssid, ssid_color);

        /* Signal percentage */
        char sig_str[8];
        itoa(net->signal, sig_str, 10);
        strcat(sig_str, "%");
        fb_draw_string_transparent(52, item_y + 26, sig_str, COLOR_TEXT_SECONDARY);

        /* Lock icon for encrypted */
        if (net->encrypted) {
            draw_lock_icon(win->width - 80, item_y + 14, COLOR_WARNING);
        } else {
            fb_draw_string_transparent(win->width - 80, item_y + 14, "Open", COLOR_SUCCESS);
        }

        /* Connect/Disconnect button */
        if (is_selected) {
            if (is_connected) {
                gfx_draw_rounded_rect_filled(win->width - 55, item_y + 10, 40, 22, 5, COLOR_ERROR);
                fb_draw_string_transparent(win->width - 50, item_y + 14, "Off", 0xFFFFFFFF);
            } else {
                gfx_draw_rounded_rect_filled(win->width - 55, item_y + 10, 40, 22, 5, COLOR_SUCCESS);
                fb_draw_string_transparent(win->width - 53, item_y + 14, "On", 0xFFFFFFFF);
            }
        }

        /* Connected badge */
        if (is_connected) {
            gfx_draw_circle_filled(win->width - 30, item_y + 35, 4, COLOR_SUCCESS);
        }

        /* Selection border */
        if (is_selected) {
            gfx_draw_rounded_rect(8, item_y, win->width - 16, 44, 8, COLOR_ACCENT);
        }
    }

    /* ── Details Panel (when connected) ── */
    if (ns->status == NET_CONNECTED && ws->show_details) {
        int dy = win->height - 80;
        fb_draw_line(10, dy, win->width - 10, dy, 0xFF1A3550);
        fb_draw_string_transparent(10, dy + 8, "IP:", COLOR_TEXT_SECONDARY);
        fb_draw_string_transparent(50, dy + 8, ns->ip_address, COLOR_TEXT_PRIMARY);
        fb_draw_string_transparent(10, dy + 26, "GW:", COLOR_TEXT_SECONDARY);
        fb_draw_string_transparent(50, dy + 26, ns->gateway, COLOR_TEXT_PRIMARY);

        char rx_str[16], tx_str[16];
        utoa(ns->bytes_recv / 1024, rx_str, 10);
        strcat(rx_str, " KB");
        utoa(ns->bytes_sent / 1024, tx_str, 10);
        strcat(tx_str, " KB");

        fb_draw_string_transparent(10, dy + 44, "RX:", COLOR_TEXT_SECONDARY);
        fb_draw_string_transparent(50, dy + 44, rx_str, COLOR_ACCENT);
        fb_draw_string_transparent(140, dy + 44, "TX:", COLOR_TEXT_SECONDARY);
        fb_draw_string_transparent(180, dy + 44, tx_str, COLOR_WARNING);

        fb_draw_string_transparent(10, dy + 62, "DNS:", COLOR_TEXT_SECONDARY);
        fb_draw_string_transparent(50, dy + 62, "8.8.8.8", COLOR_TEXT_PRIMARY);
    }
}

static void wifi_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    if (!left) return;
    struct wifi_state *ws = (struct wifi_state *)win->user_data;
    struct network_state *ns = network_get_state();

    /* Scan button */
    if (my >= 78 && my <= 102 && mx >= win->width - 80 && mx <= win->width - 10) {
        network_scan();
        win->needs_redraw = true;
        return;
    }

    /* Network list items */
    int list_y = 110 - ws->scroll_y;
    for (int i = 0; i < ns->network_count; i++) {
        int item_y = list_y + i * 50;
        if (my >= item_y && my < item_y + 44 && mx >= 8 && mx < win->width - 8) {
            if (ws->selected == i) {
                /* Second click: connect or disconnect */
                if (ns->networks[i].connected) {
                    network_disconnect();
                } else {
                    network_connect(i);
                }
            }
            ws->selected = i;
            ws->show_details = true;
            win->needs_redraw = true;
            return;
        }
    }
}

static void wifi_on_key(struct window *win, char key, uint8_t scancode) {
    (void)key;
    (void)scancode;
    (void)win;
}

struct window *wifi_manager_open(void) {
    struct window *win = wm_create_window("WiFi Manager", 120, 80, 350, 500);
    if (!win) return NULL;

    struct wifi_state *ws = kcalloc(1, sizeof(struct wifi_state));
    ws->selected = -1;
    ws->scroll_y = 0;
    ws->show_details = false;

    /* Rescan on open */
    network_scan();

    win->user_data = ws;
    win->on_draw = wifi_draw;
    win->on_mouse = wifi_on_mouse;
    win->on_key = wifi_on_key;

    return win;
}
