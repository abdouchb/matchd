#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool present;
    uint32_t percentage;
    bool is_charging;
    bool is_critical;
} battery_status_t;

void battery_service_init(void);
battery_status_t battery_service_get_status(void);
