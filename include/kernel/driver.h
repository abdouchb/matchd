#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "kernel/pci.h"

// Driver match types
#define DRIVER_MATCH_EXACT        1
#define DRIVER_MATCH_CLASS        2
#define DRIVER_MATCH_CLASS_PROGIF 3

typedef struct pci_driver {
    const char *name;
    
    // Match criteria
    uint8_t match_type;
    
    // For EXACT match
    uint16_t vendor_id;
    uint16_t device_id;
    
    // For CLASS match
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    
    // Initialization function. Returns true if successfully claimed.
    bool (*init)(const pci_device_t *dev);
} pci_driver_t;

// Register a driver
void driver_register(pci_driver_t *driver);

// Initialize the driver manager and probe all devices
void driver_manager_init(void);

// Fallback reporting for unsupported devices
void driver_manager_report_unsupported(void);
