#include "desktop/taskbar/widgets.h"
#include "desktop/taskbar.h"
#include "desktop/desktop.h"
#include "desktop/font_engine.h"
#include "gui/graphics.h"

static int power_get_width(void) {
    return 40; // 30px width + 10px spacing
}

static void power_draw(int x, int y) {
    gfx_draw_rounded_rect_filled(x, y - 3, 30, 22, 5, COLOR_ERROR);
    font_engine_draw_string_transparent(x + 5, y, "PWR", COLOR_WHITE);
}

static void power_on_click(int x, int y) {
    (void)x; (void)y;
    desktop_launch_app(APP_POWER);
}

static taskbar_widget_t power_widget = {
    .name = "Power",
    .get_width = power_get_width,
    .draw = power_draw,
    .on_click = power_on_click
};

void widget_power_init(void) {
    taskbar_widget_register(1, &power_widget); // Priority 1 (Absolute Rightmost)
}
