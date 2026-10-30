#ifndef EHCI_H
#define EHCI_H

#include "zarqaa/types.h"
#include "kernel/pci.h"

// EHCI Operational Register Bits
#define EHCI_USBCMD_RS (1 << 0)
#define EHCI_USBCMD_HCRESET (1 << 1)

// EHCI Registers
struct ehci_cap_regs {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hciversion;
    uint32_t hcsparams;
    uint32_t hccparams;
    uint64_t hcsp_portroute;
} __attribute__((packed));

struct ehci_op_regs {
    volatile uint32_t usbcmd;
    volatile uint32_t usbsts;
    volatile uint32_t usbintr;
    volatile uint32_t frindex;
    volatile uint32_t ctrldssegment;
    volatile uint32_t periodiclistbase;
    volatile uint32_t asynclistaddr;
    volatile uint32_t reserved[9];
    volatile uint32_t configflag;
    volatile uint32_t portsc[0]; // Port Status and Control
} __attribute__((packed));

bool ehci_driver_init(const pci_device_t *dev);
void ehci_init(void);

#endif
