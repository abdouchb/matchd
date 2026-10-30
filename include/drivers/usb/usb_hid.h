#ifndef DRIVERS_USB_HID_H
#define DRIVERS_USB_HID_H

#include "zarqaa/types.h"

struct key_event {
    char c;
    uint8_t scancode;
    bool is_release;
    bool shift;
    bool ctrl;
    bool alt;
};

// Initialize the generic input subsystem
void input_init(void);

// Hook functions for USB core to push data to the input subsystem
void input_keyboard_push(uint8_t scancode, bool is_release);
void input_mouse_push(int dx, int dy, int dz, bool left, bool right, bool middle);

// Event-driven API
bool keyboard_has_event(void);
int keyboard_poll_event(struct key_event *event);

// Legacy API compatibility (so GUI doesn't need to change)
char keyboard_getchar(void);
int keyboard_poll(char *c);
bool keyboard_key_pressed(uint8_t scancode);
bool keyboard_shift_pressed(void);
bool keyboard_ctrl_pressed(void);
bool keyboard_alt_pressed(void);
bool keyboard_caps_locked(void);

int mouse_get_x(void);
int mouse_get_y(void);
int mouse_get_z(void);
bool mouse_left_pressed(void);
bool mouse_right_pressed(void);
bool mouse_middle_pressed(void);
bool mouse_left_clicked(void);
bool mouse_right_clicked(void);
void mouse_set_bounds(int max_x, int max_y);

#endif /* DRIVERS_USB_HID_H */
