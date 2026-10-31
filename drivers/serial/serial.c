/*
 * Zarqaa OS 0.1 - Serial Port Driver (COM1)
 * Creator: Abdelhak Chabbi
 */

#include "drivers/serial.h"

void serial_init(void) {
    // COM1 Initialization (kprintf)
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    
    // COM2 Initialization (Telemetry)
    outb(COM2 + 1, 0x00);
    outb(COM2 + 3, 0x80);
    outb(COM2 + 0, 0x01);
    outb(COM2 + 1, 0x00);
    outb(COM2 + 3, 0x03);
    outb(COM2 + 2, 0xC7);
    outb(COM2 + 4, 0x0B);
}

static int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_write_char(char c) {
    int timeout = 10000;
    while (serial_is_transmit_empty() == 0) {
        if (--timeout == 0) return; // Serial port doesn't exist or is stuck
    }
    outb(COM1, c);
}

void serial_write_string(const char *s) {
    while (*s) {
        serial_write_char(*s++);
    }
}

static int serial2_is_transmit_empty(void) {
    return inb(COM2 + 5) & 0x20;
}

void telemetry_emit(const char *json_string) {
    const char *s = json_string;
    while (*s) {
        while (serial2_is_transmit_empty() == 0);
        outb(COM2, *s++);
    }
    while (serial2_is_transmit_empty() == 0);
    outb(COM2, '\n');
}
