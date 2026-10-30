#include "services/power/battery_service.h"
#include "drivers/power/battery.h"
#include "zarqaa/types.h"

void battery_service_init(void) {
    kprintf("[Battery Service] Initialized.\n");
}

battery_status_t battery_service_get_status(void) {
    battery_status_t status = {0};
    const battery_info_t* info = battery_get_info();
    
    if (info) {
        status.present = info->present;
        status.percentage = info->percentage;
        status.is_charging = info->is_charging;
        status.is_critical = info->is_critical;
    }
    
    return status;
}
