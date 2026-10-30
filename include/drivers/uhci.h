#ifndef UHCI_H
#define UHCI_H

#include "zarqaa/types.h"
#include "kernel/pci.h"

// UHCI USBCMD Register bits
#define UHCI_USBCMD_RUN (1 << 0)
#define UHCI_USBCMD_HCRESET (1 << 1)
#define UHCI_USBCMD_GLOBAL_RESET (1 << 2)
#define UHCI_USBCMD_ENTER_GLOBAL_SUSPEND (1 << 3)
#define UHCI_USBCMD_FORCE_GLOBAL_RESUME (1 << 4)
#define UHCI_USBCMD_SW_DEBUG (1 << 5)
#define UHCI_USBCMD_CONFIG_FLAG (1 << 6)
#define UHCI_USBCMD_MAX_PACKET_64 (1 << 7)

// UHCI USBSTS Register bits
#define UHCI_USBSTS_USBINT (1 << 0)
#define UHCI_USBSTS_ERROR_INT (1 << 1)
#define UHCI_USBSTS_RESUME_DETECT (1 << 2)
#define UHCI_USBSTS_SYSTEM_ERROR (1 << 3)
#define UHCI_USBSTS_PROCESS_ERROR (1 << 4)
#define UHCI_USBSTS_HCHALTED (1 << 5)

// UHCI Registers (Offsets from IO Base)
#define UHCI_REG_USBCMD 0x00
#define UHCI_REG_USBSTS 0x02
#define UHCI_REG_USBINTR 0x04
#define UHCI_REG_FRNUM 0x06
#define UHCI_REG_FLBASEADD 0x08
#define UHCI_REG_SOFMOD 0x0C
#define UHCI_REG_PORTSC1 0x10
#define UHCI_REG_PORTSC2 0x12

// UHCI Transfer Descriptor
struct uhci_td {
    uint32_t link_ptr;
    uint32_t status;
    uint32_t token;
    uint32_t buffer_ptr;
    
    // Software fields (must ensure 16-byte alignment hardware requirement, so software fields go after)
} __attribute__((packed, aligned(16)));

// UHCI Queue Head
struct uhci_qh {
    uint32_t head_link_ptr;
    uint32_t element_link_ptr;
} __attribute__((packed, aligned(16)));

bool uhci_driver_init(const pci_device_t *dev);
void uhci_init(void);

#endif
