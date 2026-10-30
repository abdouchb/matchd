#include "kernel/driver.h"
#include "zarqaa/types.h"

extern void kprintf(const char *fmt, ...);

bool buggy_driver_init(const pci_device_t *dev) {
    kprintf("[BuggyDriver] Intentionally causing a Page Fault to test Sandbox...\n");
    
    // Read from an unmapped address in the upper half to trigger #PF
    volatile uint32_t *bad_ptr = (volatile uint32_t *)0xFFFFFFFF00000000;
    uint32_t bad_val = *bad_ptr;
    
    // This should never execute!
    kprintf("[BuggyDriver] Survived? Value: %x\n", bad_val);
    return true;
}

static pci_driver_t buggy_driver = {
    .name = "Test Buggy Driver (Causes #PF)",
    .match_type = 2, // DRIVER_MATCH_CLASS
    .class_code = 0x06, // Bridge
    .subclass = 0x00, // Host Bridge
    .init = buggy_driver_init
};

void buggy_driver_register(void) {
    driver_register(&buggy_driver);
}
