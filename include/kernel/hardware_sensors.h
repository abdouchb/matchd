/*
 * Zarqaa OS 0.1 - Hardware Sensors Header
 * Creator: Abdelhak Chabbi
 */

#ifndef KERNEL_HARDWARE_SENSORS_H
#define KERNEL_HARDWARE_SENSORS_H

#include "zarqaa/types.h"

void sensors_init(void);
uint32_t get_cpu_temp_celsius(void);
uint32_t get_cpu_fan_rpm(void);
uint32_t get_cpu_freq_mhz(void);
uint32_t get_cpu_voltage_mv(void);

#endif /* KERNEL_HARDWARE_SENSORS_H */
