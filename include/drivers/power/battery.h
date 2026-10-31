#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool present;
    uint32_t remaining_capacity;
    uint32_t full_capacity;
    uint32_t percentage;
    int32_t temperature;
    bool is_charging;
    bool is_critical;
} battery_info_t;

void battery_init(void);
const battery_info_t* battery_get_info(void);
