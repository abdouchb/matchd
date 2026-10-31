#include "desktop/taskbar/widgets.h"
#include "desktop/font_engine.h"
#include "drivers/font.h"
#include "drivers/pit.h"
#include "zarqaa/string.h"
#include "zarqaa/types.h"

extern int64_t boot_time;

static int clock_get_width(void) {
    return 80; // Enough for "00:00:00"
}

static void clock_draw(int x, int y) {
    uint64_t current_time = boot_time + pit_get_seconds();
    int seconds = current_time % 60;
    int minutes = (current_time / 60) % 60;
    int hours = (current_time / 3600) % 24;

    char time_str[16];
    time_str[0] = '0' + (hours / 10);
    time_str[1] = '0' + (hours % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (minutes / 10);
    time_str[4] = '0' + (minutes % 10);
    time_str[5] = ':';
    time_str[6] = '0' + (seconds / 10);
    time_str[7] = '0' + (seconds % 10);
    time_str[8] = '\0';

    font_engine_draw_string_transparent(x, y, time_str, COLOR_TEXT_PRIMARY);
}

static void clock_on_click(int x, int y) {
    (void)x; (void)y;
    // Maybe open a calendar application later
}

static taskbar_widget_t clock_widget = {
    .name = "Clock",
    .get_width = clock_get_width,
    .draw = clock_draw,
    .on_click = clock_on_click
};

void widget_clock_init(void) {
    taskbar_widget_register(100, &clock_widget); // High priority = rightmost
}
