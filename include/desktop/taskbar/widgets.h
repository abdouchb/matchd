#pragma once

#include <stdbool.h>

#define MAX_WIDGETS 16

typedef struct {
    const char *name;
    int (*get_width)(void);
    void (*draw)(int x, int y);
    void (*on_click)(int x, int y);
} taskbar_widget_t;

typedef struct {
    bool visible;
    int priority;
    taskbar_widget_t widget;
} taskbar_widget_entry_t;

void taskbar_widgets_init(void);
void taskbar_widget_register(int priority, const taskbar_widget_t *widget);
void taskbar_widgets_draw(int max_x, int y);
bool taskbar_widgets_handle_click(int mx, int my);

// Core widgets
void widget_clock_init(void);
void widget_wifi_init(void);
void widget_battery_init(void);
void widget_usb_init(void);
void widget_power_init(void);
