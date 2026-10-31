#include "desktop/taskbar/widgets.h"
#include "services/power/battery_service.h"
#include "gui/graphics.h"
#include "drivers/gpu_renderer.h"
#include "desktop/font_engine.h"

static int battery_get_width(void) {
    battery_status_t status = battery_service_get_status();
    if (!status.present) return 0; // Don't show if no battery
    
    // Width of battery icon (approx 24px) + text (e.g. "100%")
    return 65; 
}

static void battery_draw(int x, int y) {
    battery_status_t status = battery_service_get_status();
    if (!status.present) return;
    
    // Draw battery outline (20x10)
    int bx = x;
    int by = y + 5;
    
    gfx_draw_rect(bx, by, 20, 10, COLOR_TEXT_PRIMARY); // Main body
    gpu_renderer_fill_rect(bx + 20, by + 3, 2, 4, COLOR_TEXT_PRIMARY); // Terminal
    
    // Fill battery level
    int fill_width = (status.percentage * 18) / 100;
    if (fill_width > 18) fill_width = 18;
    if (fill_width < 0) fill_width = 0;
    
    color_t fill_color = COLOR_TEXT_PRIMARY;
    if (status.percentage <= 20 || status.is_critical) {
        fill_color = COLOR_ERROR; // Red
    } else if (status.is_charging) {
        fill_color = COLOR_SUCCESS; // Green
    }
    
    gpu_renderer_fill_rect(bx + 1, by + 1, fill_width, 8, fill_color);
    
    // Draw charging indicator (simple plus sign inside or next to it)
    if (status.is_charging) {
        // Draw a small '+' next to the terminal
        font_engine_draw_string_transparent(bx + 23, y, "+", COLOR_SUCCESS);
    }
    
    // Draw percentage text
    char pct_str[8];
    int pct = status.percentage;
    int i = 0;
    
    if (pct == 100) {
        pct_str[i++] = '1';
        pct_str[i++] = '0';
        pct_str[i++] = '0';
    } else {
        if (pct >= 10) {
            pct_str[i++] = '0' + (pct / 10);
        }
        pct_str[i++] = '0' + (pct % 10);
    }
    pct_str[i++] = '%';
    pct_str[i] = '\0';
    
    font_engine_draw_string_transparent(bx + 35, y, pct_str, COLOR_TEXT_PRIMARY);
}

static void battery_on_click(int x, int y) {
    (void)x; (void)y;
    // Launch Power Settings later
}

static taskbar_widget_t battery_widget = {
    .name = "Battery",
    .get_width = battery_get_width,
    .draw = battery_draw,
    .on_click = battery_on_click
};

void widget_battery_init(void) {
    taskbar_widget_register(10, &battery_widget); // Priority 10 (drawn to the left of WiFi)
}
