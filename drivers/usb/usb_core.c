#include "drivers/usb/usb.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"

extern void kprintf(const char *fmt, ...);

void usb_core_init(void) {
    early_blog(BLOG_INFO, "USB", "Initializing USB Core Subsystem");
    // In a full implementation, this would spawn a kernel thread to process
    // hub events and enumerate devices.
    early_blog(BLOG_OK, "USB", "USB Core Initialized");
}
