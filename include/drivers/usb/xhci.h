/*
 * Zarqaa OS 0.1 - xHCI (USB 3.0) Controller Driver
 */

#ifndef DRIVERS_USB_XHCI_H
#define DRIVERS_USB_XHCI_H

#include "kernel/pci.h"

#define TRB_TYPE_NORMAL               1
#define TRB_TYPE_SETUP_STAGE          2
#define TRB_TYPE_DATA_STAGE           3
#define TRB_TYPE_STATUS_STAGE         4
#define TRB_TYPE_ENABLE_SLOT_CMD      9
#define TRB_TYPE_DISABLE_SLOT_CMD     10
#define TRB_TYPE_ADDRESS_DEVICE_CMD   11
#define TRB_TYPE_CONFIGURE_EP_CMD     12
#define TRB_TYPE_EVALUATE_CTX_CMD     13
#define TRB_TYPE_TRANSFER_EVENT       32
#define TRB_TYPE_CMD_COMPLETION_EVENT 33
#define TRB_TYPE_PORT_STATUS_EVENT    34

#include "zarqaa/types.h"

// xHCI Capability Registers offsets
#define XHCI_CAP_CAPLENGTH  0x00
#define XHCI_CAP_HCIVERSION 0x02
#define XHCI_CAP_HCSPARAMS1 0x04
#define XHCI_CAP_HCSPARAMS2 0x08
#define XHCI_CAP_HCSPARAMS3 0x0C
#define XHCI_CAP_HCCPARAMS1 0x10
#define XHCI_CAP_DBOFF      0x14
#define XHCI_CAP_RTSOFF     0x18

// xHCI Operational Registers offsets (from CAPLENGTH)
#define XHCI_OP_USBCMD      0x00
#define XHCI_OP_USBSTS      0x04
#define XHCI_OP_PAGESIZE    0x08
#define XHCI_OP_DNCTRL      0x14
#define XHCI_OP_CRCR        0x18
#define XHCI_OP_DCBAAP      0x30
#define XHCI_OP_CONFIG      0x38
#define XHCI_OP_PORTSC_BASE 0x40

// xHCI Runtime Registers offsets (from RTSOFF)
#define XHCI_RT_MFINDEX     0x00
#define XHCI_RT_IR0         0x20

#define XHCI_IR_IMAN        0x00
#define XHCI_IR_IMOD        0x04
#define XHCI_IR_ERSTSZ      0x08
#define XHCI_IR_ERSTBA      0x10
#define XHCI_IR_ERDP        0x18

// USBCMD Bits
#define USBCMD_RUN          (1 << 0)
#define USBCMD_HCRST        (1 << 1)
#define USBCMD_INTE         (1 << 2)

// USBSTS Bits
#define USBSTS_HCH          (1 << 0)
#define USBSTS_CNR          (1 << 11)

// TRB (Transfer Request Block) Types
#define TRB_NORMAL          1
#define TRB_SETUP           2
#define TRB_DATA            3
#define TRB_STATUS          4
#define TRB_LINK            6
#define TRB_ENABLE_SLOT     9
#define TRB_ADDRESS_DEVICE  11
#define TRB_CONFIGURE_EP    12
#define TRB_EVAL_CONTEXT    13
#define TRB_RESET_EP        14
#define TRB_TRANSFER_EVENT  32
#define TRB_CMD_COMPLETION  33
#define TRB_PORT_STATUS     34

struct xhci_trb {
    uint32_t param1;
    uint32_t param2;
    uint32_t status;
    uint32_t control;
} __attribute__((packed));

struct xhci_erst_entry {
    uint64_t ring_segment_base;
    uint16_t ring_segment_size;
    uint16_t reserved[3];
} __attribute__((packed));

// Basic context definition (assumes 32-byte contexts)
struct xhci_slot_context {
    uint32_t info1;
    uint32_t info2;
    uint32_t tt_info;
    uint32_t state;
    uint32_t reserved[4];
} __attribute__((packed));

struct xhci_endpoint_context {
    uint32_t info1;
    uint32_t info2;
    uint64_t tr_dequeue_ptr;
    uint32_t tx_info;
    uint32_t reserved[3];
} __attribute__((packed));

struct xhci_input_control_context {
    uint32_t drop_context_flags;
    uint32_t add_context_flags;
    uint32_t reserved1[5];
    uint8_t  configuration_value;
    uint8_t  interface_number;
    uint8_t  alternate_setting;
    uint8_t  reserved2;
} __attribute__((packed));

struct xhci_device_context {
    struct xhci_slot_context slot;
    struct xhci_endpoint_context ep[31];
} __attribute__((packed));

struct xhci_input_context {
    struct xhci_input_control_context control;
    struct xhci_device_context dev;
} __attribute__((packed));

void xhci_init(void);
void xhci_poll_events(void);

#endif /* DRIVERS_USB_XHCI_H */
