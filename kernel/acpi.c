#include "kernel/acpi.h"
#include "zarqaa/types.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "limine.h"
#include "zarqaa/string.h"
#include <uacpi/uacpi.h>
#include <uacpi/sleep.h>
#include <uacpi/utilities.h>

extern volatile struct limine_rsdp_request rsdp_request;
extern uint64_t hhdm_offset;
uint64_t g_rsdp_address = 0;

void acpi_init(void) {
    kprintf("[ACPI] Initializing uACPI subsystem...\n");
    
    if (rsdp_request.response == NULL) {
        kprintf("[ACPI] Limine did not provide RSDP!\n");
        return;
    }
    
    g_rsdp_address = (uint64_t)rsdp_request.response->address;
    kprintf("[ACPI] RSDP Address: 0x%llx\n", g_rsdp_address);
    
    // Initialize uACPI (Level 1)
    uacpi_status st = uacpi_initialize(0);
    if (st != UACPI_STATUS_OK) {
        kprintf("[ACPI] uacpi_initialize failed: %s\n", uacpi_status_to_string(st));
        return;
    }
    
    kprintf("[ACPI] uACPI Early Init Successful.\n");
    
    // Load Namespace (Level 2)
    st = uacpi_namespace_load();
    if (st != UACPI_STATUS_OK) {
        kprintf("[ACPI] uacpi_namespace_load failed: %s\n", uacpi_status_to_string(st));
        return;
    }
    
    kprintf("[ACPI] uACPI Namespace Loaded.\n");
    
    // Initialize Namespace (Level 3)
    st = uacpi_namespace_initialize();
    if (st != UACPI_STATUS_OK) {
        kprintf("[ACPI] uacpi_namespace_initialize failed: %s\n", uacpi_status_to_string(st));
        return;
    }
    
    kprintf("[ACPI] uACPI Fully Initialized!\n");
}

void acpi_power_off(void) {
    kprintf("[ACPI] Initiating Hardware Shutdown via uACPI...\n");
    
    uacpi_status st = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (st == UACPI_STATUS_OK) {
        st = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    }
    
    if (st != UACPI_STATUS_OK) {
        kprintf("[ACPI] S5 sleep transition failed: %s\n", uacpi_status_to_string(st));
    }
    
    // Fallback if uACPI fails
    outw(0x604, 0x2000); // QEMU
    outw(0xB004, 0x2000); // Bochs/VirtualBox
    
    kprintf("[ACPI] Shutdown command sent, waiting for power off...\n");
    // Removed the cli; pause loop here so the OS doesn't completely freeze
    // if the hardware doesn't support ACPI S5 shutdown.
}
