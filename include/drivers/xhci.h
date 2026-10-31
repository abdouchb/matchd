#ifndef XHCI_H
#define XHCI_H

#include "zarqaa/types.h"
#include "kernel/pci.h"

static inline void flush_cache(void* addr, size_t size) {
    uintptr_t start = ((uintptr_t)addr) & ~63;
    uintptr_t end = ((uintptr_t)addr) + size;
    for (uintptr_t p = start; p < end; p += 64) {
        __asm__ volatile("clflush (%0)" :: "r"(p) : "memory");
    }
}

#define XHCI_MAX_SLOTS   64
#define XHCI_MAX_EPS     31   // EP 0 + 15 IN + 15 OUT
#define XHCI_TRBS_PER_RING (4096 / 16)  // 256 TRBs per page

// TRB Types
#define TRB_TYPE_NORMAL         1
#define TRB_TYPE_SETUP          2
#define TRB_TYPE_DATA           3
#define TRB_TYPE_STATUS         4
#define TRB_TYPE_LINK           6
#define TRB_TYPE_ENABLE_SLOT    9
#define TRB_TYPE_DISABLE_SLOT   10
#define TRB_TYPE_ADDRESS_DEVICE 11
#define TRB_TYPE_CONFIGURE_EP   12
#define TRB_TYPE_EVAL_CONTEXT   13
#define TRB_TYPE_RESET_EP       14
#define TRB_TYPE_NOOP_CMD       23
#define TRB_TYPE_TRANSFER_EVENT 32
#define TRB_TYPE_CMD_COMPLETION 33
#define TRB_TYPE_PORT_STATUS    34

// Completion Codes
#define TRB_CC_SUCCESS          1
#define TRB_CC_SHORT_PACKET     13

// TRB Structure (Transfer Request Block) - 16 bytes
typedef struct {
    uint32_t param1;
    uint32_t param2;
    uint32_t status;
    uint32_t control;
} __attribute__((packed)) xhci_trb_t;

// ERST Entry (Event Ring Segment Table)
typedef struct {
    uint64_t ring_segment_base_address;
    uint16_t ring_segment_size;
    uint16_t reserved[3];
} __attribute__((packed)) xhci_erst_entry_t;

// Slot Context (32 bytes)
typedef struct {
    uint32_t field1;    // Route String, Speed, MTT, Hub, Context Entries
    uint32_t field2;    // Max Exit Latency, Root Hub Port Number, Number of Ports
    uint32_t field3;    // TT Hub Slot ID, TT Port Number, TTT, Interrupter Target
    uint32_t field4;    // USB Device Address, Slot State
    uint32_t reserved[4];
} __attribute__((packed)) xhci_slot_ctx_t;

// Endpoint Context (32 bytes)
typedef struct {
    uint32_t field1;    // EP State, Mult, MaxPStreams, LSA, Interval, MaxESITPayloadHi
    uint32_t field2;    // MaxPacketSize, MaxBurstSize, HID, EPType, CErr
    uint32_t dequeue_lo; // TR Dequeue Pointer Low (+ DCS bit 0)
    uint32_t dequeue_hi; // TR Dequeue Pointer High
    uint32_t field4;    // Average TRB Length, Max ESIT Payload Lo
    uint32_t reserved[3];
} __attribute__((packed)) xhci_ep_ctx_t;

// Device Context (Slot + 31 EPs = 32 * 32 bytes = 1024 bytes)
typedef struct {
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t   ep[31]; // ep[0] = Control EP (EP0 IN/OUT)
} __attribute__((packed)) xhci_dev_ctx_t;

// Input Control Context (32 bytes)
typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[6];
} __attribute__((packed)) xhci_input_ctrl_ctx_t;

// Input Context = Input Control Context + Device Context
typedef struct {
    xhci_input_ctrl_ctx_t ctrl;
    xhci_slot_ctx_t       slot;
    xhci_ep_ctx_t         ep[31];
} __attribute__((packed)) xhci_input_ctx_t;

// xHCI Capability Registers
typedef struct {
    uint8_t  caplength;
    uint8_t  reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;
} __attribute__((packed)) xhci_cap_regs_t;

// xHCI Port Registers
typedef struct {
    volatile uint32_t portsc;
    volatile uint32_t portpmsc;
    volatile uint32_t portli;
    volatile uint32_t porthlpmc;
} __attribute__((packed)) xhci_port_regs_t;

// xHCI Operational Registers
typedef struct {
    volatile uint32_t usbcmd;
    volatile uint32_t usbsts;
    volatile uint32_t pagesize;
    uint8_t  reserved1[8];
    volatile uint32_t dnctrl;
    volatile uint64_t crcr;
    uint8_t  reserved2[16];
    volatile uint64_t dcbaap;
    volatile uint32_t config;
    uint8_t  reserved3[964];
    xhci_port_regs_t port_regs[256];
} __attribute__((packed)) xhci_op_regs_t;

// Runtime Registers
typedef struct {
    volatile uint32_t iman;
    volatile uint32_t imod;
    volatile uint32_t erstsz;
    volatile uint32_t reserved;
    volatile uint64_t erstba;
    volatile uint64_t erdp;
} __attribute__((packed)) xhci_intr_reg_t;

typedef struct {
    volatile uint32_t mfindex;
    uint8_t reserved[28];
    xhci_intr_reg_t ir[1024];
} __attribute__((packed)) xhci_run_regs_t;

// ---- Driver API ----
void     xhci_init(void);
void     xhci_poll(void);

uint32_t xhci_port_status(uint8_t port);
void     xhci_port_reset(uint8_t port);

// Command Ring
uint8_t  xhci_cmd_enable_slot(void);
bool     xhci_cmd_address_device(uint8_t slot_id, uint8_t port, uint8_t speed, uint16_t max_packet_size);
bool     xhci_cmd_configure_endpoint(uint8_t slot_id, uint8_t ep_num, uint8_t ep_dir, uint8_t ep_type, uint16_t max_packet_size, uint8_t interval);

// Transfer Ring
int32_t  xhci_control_transfer(uint8_t slot_id, uint8_t const setup[8], uint8_t *data_buf, uint16_t data_len);
bool     xhci_ep_xfer(uint8_t slot_id, uint8_t ep_dci, uint8_t *buffer, uint16_t length);
// Slot management (maps TinyUSB dev_addr -> xHCI slot)
uint8_t  xhci_get_slot_for_addr(uint8_t dev_addr);
void     xhci_set_slot_for_addr(uint8_t dev_addr, uint8_t slot_id);

#endif
