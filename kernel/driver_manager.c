#include "kernel/driver.h"
#include "zarqaa/types.h"
#include "kernel/setjmp.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"

extern void kprintf(const char *fmt, ...);

jmp_buf driver_recovery_buf;
volatile bool g_driver_loading = false;
const char *g_current_driver_name = NULL;

#define MAX_DRIVERS 32
static pci_driver_t *registered_drivers[MAX_DRIVERS];
static int num_drivers = 0;

void driver_register(pci_driver_t *driver) {
    if (num_drivers < MAX_DRIVERS) {
        registered_drivers[num_drivers++] = driver;
    } else {
        kprintf("[DriverMgr] Error: Driver registry full!\n");
    }
}

/* Devices whose drivers locate them via pci_find_device() rather than registering here. */
static bool probed_by_subsystem(const pci_device_t *dev) {
    if (dev->vendor_id == 0x10EC)
        return dev->device_id == 0x8139 || dev->device_id == 0x8168 || dev->device_id == 0x8136;
    return dev->vendor_id == 0x8086 && dev->device_id == 0x2415; /* AC'97 */
}

void driver_manager_init(void) {
    kprintf("[DriverMgr] Initializing smart driver subsystem...\n");

    const pci_device_t *devices = pci_get_devices();
    uint32_t count = pci_get_device_count();

    for (uint32_t i = 0; i < count; i++) {
        const pci_device_t *dev = &devices[i];
        bool claimed = false;

        // Try to find a matching driver
        for (int j = 0; j < num_drivers; j++) {
            pci_driver_t *drv = registered_drivers[j];
            
            bool match = false;
            if (drv->match_type == DRIVER_MATCH_EXACT) {
                if (drv->vendor_id == dev->vendor_id && drv->device_id == dev->device_id) {
                    match = true;
                }
            } else if (drv->match_type == DRIVER_MATCH_CLASS) {
                if (drv->class_code == dev->class_code && drv->subclass == dev->subclass) {
                    if (drv->vendor_id == 0 || drv->vendor_id == dev->vendor_id) {
                        match = true;
                    }
                }
            } else if (drv->match_type == DRIVER_MATCH_CLASS_PROGIF) {
                if (drv->class_code == dev->class_code && drv->subclass == dev->subclass && drv->prog_if == dev->prog_if) {
                    match = true;
                }
            }

            if (match) {
                kprintf("[DriverMgr] Device %04x:%04x matched driver '%s'\n", 
                        dev->vendor_id, dev->device_id, drv->name);
                
                kprintf("[DriverMgr] Initializing '%s'...\n", drv->name);
                
                g_current_driver_name = drv->name;
                g_driver_loading = true;
                
                char log_msg[64];
                sprintf(log_msg, "Initializing driver: %s", drv->name);
                early_blog(BLOG_INFO, "DRVMGR", log_msg);
                
                int jmp_res = setjmp(driver_recovery_buf);
                bool success = false;
                
                if (jmp_res == 0) {
                    success = (drv->init != NULL) ? drv->init(dev) : false;
                } else {
                    // We arrived here via longjmp from the exception handler!
                    // Interrupts were disabled by the CPU when it entered the exception handler.
                    // We must re-enable them since we skipped the 'iretq' instruction.
                    __asm__ volatile("sti");
                    kprintf("\n[DriverMgr] WARNING: Driver '%s' caused a fatal exception and was isolated!\n", drv->name);
                    success = false;
                }
                
                g_driver_loading = false;
                g_current_driver_name = NULL;
                
                kprintf("[DriverMgr] Driver '%s' init returned: %s\n", drv->name, success ? "SUCCESS" : "FAILED");
                if (success) {
                    early_blog_ok();
                } else {
                    early_blog_fail(NULL);
                }

                if (success) {
                    claimed = true;
                    break;
                }
            }
        }

        if (!claimed && probed_by_subsystem(dev)) {
            kprintf("[DriverMgr] INFO: %04x:%04x is handled by its subsystem probe (no PCI-manager driver)\n",
                    dev->vendor_id, dev->device_id);
        } else if (!claimed) {
            // Provide smart fallback messages for common classes
            if (dev->class_code == 0x02 && dev->subclass == 0x00) {
                kprintf("[DriverMgr] INFO: Unsupported Ethernet Controller found (Vendor: %04x, Device: %04x)\n", dev->vendor_id, dev->device_id);
            } else if (dev->class_code == 0x04 && dev->subclass == 0x01) {
                kprintf("[DriverMgr] INFO: Unsupported AC97 Audio Controller found (Vendor: %04x, Device: %04x)\n", dev->vendor_id, dev->device_id);
            } else if (dev->class_code == 0x04 && dev->subclass == 0x03) {
                kprintf("[DriverMgr] INFO: Unsupported High Definition Audio (HDA) Controller found (Vendor: %04x, Device: %04x)\n", dev->vendor_id, dev->device_id);
            }
        }
    }
}
