/*
 * Zarqaa OS 0.1 - Serial Port Driver Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DRIVERS_SERIAL_H
#define DRIVERS_SERIAL_H

#include "zarqaa/types.h"

#define COM1 0x3F8
#define COM2 0x2F8

void serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *s);

void telemetry_emit(const char *json_string);

#endif /* DRIVERS_SERIAL_H */
