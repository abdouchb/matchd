#include "zarqaa/types.h"
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>
#include "drivers/power/battery.h"

static battery_info_t g_battery_info = {0};

const battery_info_t* battery_get_info(void) {
    return &g_battery_info;
}

static uacpi_iteration_decision battery_callback(void *user, uacpi_namespace_node *node, uacpi_u32 depth) {
    (void)user;
    (void)depth;
    
    if (!node) return UACPI_ITERATION_DECISION_CONTINUE;
    
    kprintf("[Battery] Found Battery Device: %s\n", uacpi_namespace_node_name(node).text);
    
    // Evaluate _BIF (Battery Information)
    uacpi_object *bif = NULL;
    uacpi_status st = uacpi_eval(node, "_BIF", NULL, &bif);
    
    if (st == UACPI_STATUS_OK && bif != NULL) {
        uacpi_object_array arr;
        if (uacpi_object_get_package(bif, &arr) == UACPI_STATUS_OK && arr.count >= 13) {
            uacpi_u64 design_capacity = 0;
            uacpi_u64 full_capacity = 0;
            uacpi_object_get_integer(arr.objects[1], &design_capacity);
            uacpi_object_get_integer(arr.objects[2], &full_capacity);
            
            uacpi_data_view model_view = {0};
            uacpi_object_get_string(arr.objects[9], &model_view);
            const char *model = model_view.text ? model_view.text : "Unknown";
            
            g_battery_info.full_capacity = (uint32_t)full_capacity;
            
            kprintf("[Battery] Model: %s\n", model);
            kprintf("[Battery] Design Capacity: %d\n", (uint32_t)design_capacity);
            kprintf("[Battery] Full Charge Capacity: %d\n", (uint32_t)full_capacity);
        } else {
            kprintf("[Battery] _BIF returned invalid package.\n");
        }
    } else {
        kprintf("[Battery] _BIF evaluation failed.\n");
    }
    
    if (bif) uacpi_object_unref(bif);
    
    // Evaluate _BST (Battery Status)
    uacpi_object *bst = NULL;
    st = uacpi_eval(node, "_BST", NULL, &bst);
    
    if (st == UACPI_STATUS_OK && bst != NULL) {
        uacpi_object_array arr;
        if (uacpi_object_get_package(bst, &arr) == UACPI_STATUS_OK && arr.count >= 4) {
            uacpi_u64 state = 0;
            uacpi_u64 present_rate = 0;
            uacpi_u64 remaining_capacity = 0;
            uacpi_u64 present_voltage = 0;
            
            uacpi_object_get_integer(arr.objects[0], &state);
            uacpi_object_get_integer(arr.objects[1], &present_rate);
            uacpi_object_get_integer(arr.objects[2], &remaining_capacity);
            uacpi_object_get_integer(arr.objects[3], &present_voltage);
            
            const char *state_str = "Idle";
            g_battery_info.is_charging = false;
            g_battery_info.is_critical = false;
            
            if (state & 1) state_str = "Discharging";
            else if (state & 2) {
                state_str = "Charging";
                g_battery_info.is_charging = true;
            }
            
            if (state & 4) {
                state_str = "Critical";
                g_battery_info.is_critical = true;
            }
            
            g_battery_info.remaining_capacity = (uint32_t)remaining_capacity;
            if (g_battery_info.full_capacity > 0) {
                g_battery_info.percentage = (g_battery_info.remaining_capacity * 100) / g_battery_info.full_capacity;
                if (g_battery_info.percentage > 100) g_battery_info.percentage = 100;
            } else {
                g_battery_info.percentage = 0;
            }
            g_battery_info.present = true;
            
            kprintf("[Battery] State: %s\n", state_str);
            kprintf("[Battery] Remaining Capacity: %d (%d%%)\n", (uint32_t)remaining_capacity, g_battery_info.percentage);
            kprintf("[Battery] Present Voltage: %d mV\n", (uint32_t)present_voltage);
            kprintf("[Battery] Present Rate: %d mW/mA\n", (uint32_t)present_rate);
        } else {
            kprintf("[Battery] _BST returned invalid package.\n");
        }
    } else {
        kprintf("[Battery] _BST evaluation failed.\n");
    }
    
    if (bst) uacpi_object_unref(bst);
    
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void battery_init(void) {
    kprintf("[Battery] Probing for ACPI Batteries (PNP0C0A)...\n");
    
    uacpi_status st = uacpi_find_devices("PNP0C0A", battery_callback, NULL);
    if (st != UACPI_STATUS_OK && st != UACPI_STATUS_NOT_FOUND) {
        kprintf("[Battery] Error while finding batteries: %s\n", uacpi_status_to_string(st));
    }
}
