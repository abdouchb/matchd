#include "desktop/taskbar/widgets.h"
#include "drivers/network.h"
#include "gui/graphics.h"
#include "drivers/gpu_renderer.h"

static int wifi_get_width(void) {
    return 20; // 4 bars = 4 * 4 + spacing
}

static void wifi_draw(int x, int y) {
    int signal = network_get_signal();
    int bars = 0;
    color_t color;

    if (signal > 80) { bars = 4; color = COLOR_SUCCESS; }
    else if (signal > 60) { bars = 3; color = COLOR_ACCENT; }
    else if (signal > 40) { bars = 2; color = COLOR_WARNING; }
    else if (signal > 0) { bars = 1; color = COLOR_ERROR; }
    else { bars = 0; color = COLOR_TEXT_SECONDARY; }

    for (int i = 0; i < 4; i++) {
        int bh = 3 + i * 3;
        int bx = x + i * 4;
        int by = y + 14 - bh;
        color_t c = (i < bars) ? color : 0xFF334455; // Dim color if not filled
        gpu_renderer_fill_rect(bx, by, 3, bh, c);
    }

    if (bars == 0) {
        gpu_renderer_draw_line(x + 2, y + 2, x + 12, y + 12, COLOR_ERROR);
        gpu_renderer_draw_line(x + 12, y + 2, x + 2, y + 12, COLOR_ERROR);
    }
}

static void wifi_on_click(int x, int y) {
    (void)x; (void)y;
    // Launch WiFi Manager app later
}

static taskbar_widget_t wifi_widget = {
    .name = "WiFi",
    .get_width = wifi_get_width,
    .draw = wifi_draw,
    .on_click = wifi_on_click
};

void widget_wifi_init(void) {
    taskbar_widget_register(20, &wifi_widget); // Priority 20
}
