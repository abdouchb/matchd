#include "drivers/usb/xhci.h"
#include "kernel/pci.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/driver.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"

extern uint64_t hhdm_offset;
extern uint64_t *kernel_pml4;
extern void kprintf(const char *fmt, ...);

static volatile uint8_t *cap_regs = NULL;
static volatile uint8_t *op_regs = NULL;
static volatile uint8_t *rt_regs = NULL;
static volatile uint32_t *db_regs = NULL;

static uint8_t max_slots;
static uint8_t num_ports;
static uint64_t *dcbaa_virt;
static struct xhci_trb *cmd_ring_virt;
static struct xhci_trb *evt_ring_virt;
static uint64_t evt_ring_phys = 0;
static struct xhci_erst_entry *erst_virt;

static struct xhci_device_context *device_contexts_virt[32];
static uint64_t device_contexts_phys[32];
static struct xhci_input_context *input_contexts_virt[32];
static uint64_t input_contexts_phys[32];
static struct xhci_trb *ep0_rings_virt[32];
static uint64_t ep0_rings_phys[32];
static struct xhci_trb *ep1_in_rings_virt[32];
static uint64_t ep1_in_rings_phys[32];
static uint8_t *kb_buffers_virt[32];
static uint64_t kb_buffers_phys[32];

static void xhci_alloc_slot_structs(uint8_t slot) {
    if (device_contexts_virt[slot]) return;
    
    device_contexts_phys[slot] = pmm_alloc_low32();
    device_contexts_virt[slot] = (struct xhci_device_context *)(device_contexts_phys[slot] + hhdm_offset);
    memset(device_contexts_virt[slot], 0, PAGE_SIZE);
    
    input_contexts_phys[slot] = pmm_alloc_low32();
    input_contexts_virt[slot] = (struct xhci_input_context *)(input_contexts_phys[slot] + hhdm_offset);
    memset(input_contexts_virt[slot], 0, PAGE_SIZE);
    
    ep0_rings_phys[slot] = pmm_alloc_low32();
    ep0_rings_virt[slot] = (struct xhci_trb *)(ep0_rings_phys[slot] + hhdm_offset);
    memset(ep0_rings_virt[slot], 0, PAGE_SIZE);
    
    ep1_in_rings_phys[slot] = pmm_alloc_low32();
    ep1_in_rings_virt[slot] = (struct xhci_trb *)(ep1_in_rings_phys[slot] + hhdm_offset);
    memset(ep1_in_rings_virt[slot], 0, PAGE_SIZE);
    
    kb_buffers_phys[slot] = pmm_alloc_low32();
    kb_buffers_virt[slot] = (uint8_t *)(kb_buffers_phys[slot] + hhdm_offset);
    memset(kb_buffers_virt[slot], 0, PAGE_SIZE);
}

static uint32_t xhci_read32(volatile uint8_t *base, uint32_t offset) {
    return *(volatile uint32_t *)(base + offset);
}

static void xhci_write32(volatile uint8_t *base, uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(base + offset) = val;
}

static void xhci_write64(volatile uint8_t *base, uint32_t offset, uint64_t val) {
    *(volatile uint64_t *)(base + offset) = val;
}

static void xhci_bios_handoff(volatile uint8_t *cap_base) {
    uint32_t hccparams1 = xhci_read32(cap_base, XHCI_CAP_HCCPARAMS1);
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFF;
    
    if (xecp == 0) return;
    
    volatile uint32_t *ext_cap = (volatile uint32_t *)(cap_base + (xecp << 2));
    
    while (ext_cap) {
        uint32_t cap = *ext_cap;
        uint8_t cap_id = cap & 0xFF;
        
        if (cap_id == 1) { // USB Legacy Support Capability (USBLEGSUP)
            if (cap & (1 << 16)) { // BIOS Owned Semaphore
                kprintf("[xHCI] BIOS owns USB controller. Requesting OS handoff...\n");
                
                // Request OS Ownership (bit 24)
                *ext_cap = cap | (1 << 24);
                
                // Wait for BIOS to clear BIOS Owned Semaphore (bit 16)
                int timeout = 0;
                while ((*ext_cap & (1 << 16)) && timeout < 1000) {
                    timeout++;
                    for (volatile int i = 0; i < 10000; i++) __asm__ volatile("pause");
                }
                
                if (timeout >= 1000) {
                    kprintf("[xHCI] WARN: BIOS handoff timed out! Forcing OS ownership...\n");
                    *ext_cap = (*ext_cap & ~(1 << 16)) | (1 << 24);
                } else {
                    kprintf("[xHCI] BIOS handoff completed successfully.\n");
                }
            } else {
                *ext_cap = cap | (1 << 24);
            }
            
            // Disable all BIOS SMIs in USBLEGCTLSTS (offset +4)
            volatile uint32_t *legctlsts = ext_cap + 1;
            uint32_t val = *legctlsts;
            val &= 0xFFFF0000; // Clear SMI enable bits (bits 0:15)
            val |= 0xE0000000; // Clear SMI status bits (bits 29:31)
            *legctlsts = val;
            break;
        }
        
        uint8_t next = (cap >> 8) & 0xFF;
        if (!next) break;
        ext_cap = (volatile uint32_t *)((uint8_t *)ext_cap + (next << 2));
    }
}

static bool xhci_driver_init(const pci_device_t *dev) {
    if (dev->class_code != 0x0C || dev->subclass != 0x03 || dev->prog_if != 0x30) {
        return false;
    }
    
    kprintf("[xHCI] Found Controller at %02x:%02x.%d\n", dev->bus, dev->device, dev->function);

    uint16_t cmd = pci_read_config_word(dev->bus, dev->device, dev->function, 0x04);
    cmd |= (1 << 2) | (1 << 1) | (1 << 0); // Bus Master, Memory Space, I/O Space
    pci_write_config_word(dev->bus, dev->device, dev->function, 0x04, cmd);

    uint32_t bar0_low = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x10);
    uint32_t bar0_high = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x14);
    uint64_t phys_base = bar0_low & 0xFFFFFFF0;
    if ((bar0_low & 0x04) != 0) phys_base |= ((uint64_t)bar0_high << 32);

    if (!phys_base) {
        early_blog(BLOG_FAIL, "xHCI", "Invalid MMIO BAR");
        return false;
    }
    
    char msg[64];
    sprintf(msg, "xHCI MMIO at 0x%x", (uint32_t)phys_base);
    early_blog(BLOG_INFO, "xHCI", msg);

    for (uint64_t offset = 0; offset < 65536; offset += PAGE_SIZE) {
        vmm_map_mmio(kernel_pml4, phys_base + hhdm_offset + offset, phys_base + offset, PTE_PRESENT | PTE_WRITABLE | 0x18);
    }
    
    cap_regs = (volatile uint8_t *)(phys_base + hhdm_offset);
    uint8_t cap_length = cap_regs[XHCI_CAP_CAPLENGTH];
    op_regs = cap_regs + cap_length;
    
    // 1. Perform BIOS-to-OS Handoff BEFORE stopping or resetting controller
    xhci_bios_handoff(cap_regs);
    
    uint32_t hcsparams1 = xhci_read32(cap_regs, XHCI_CAP_HCSPARAMS1);
    max_slots = hcsparams1 & 0xFF;
    num_ports = (hcsparams1 >> 24) & 0xFF;
    
    uint32_t dboff = xhci_read32(cap_regs, XHCI_CAP_DBOFF);
    db_regs = (volatile uint32_t *)(cap_regs + dboff);
    
    uint32_t rtsoff = xhci_read32(cap_regs, XHCI_CAP_RTSOFF);
    rt_regs = cap_regs + rtsoff;
    
    kprintf("[xHCI] Version 0x%x, MaxSlots=%d, Ports=%d\n", xhci_read32(cap_regs, XHCI_CAP_HCIVERSION) >> 16, max_slots, num_ports);
    
    uint32_t usbcmd = xhci_read32(op_regs, XHCI_OP_USBCMD);
    if (usbcmd & USBCMD_RUN) {
        xhci_write32(op_regs, XHCI_OP_USBCMD, usbcmd & ~USBCMD_RUN);
        for (int i=0; i<10000; i++) {
            if (xhci_read32(op_regs, XHCI_OP_USBSTS) & USBSTS_HCH) break;
            for (volatile int d=0; d<1000; d++) __asm__ volatile("pause");
        }
    }
    
    xhci_write32(op_regs, XHCI_OP_USBCMD, USBCMD_HCRST);
    for (int i=0; i<10000; i++) {
        if ((xhci_read32(op_regs, XHCI_OP_USBCMD) & USBCMD_HCRST) == 0) break;
        for (volatile int d=0; d<1000; d++) __asm__ volatile("pause");
    }
    for (int i=0; i<10000; i++) {
        if ((xhci_read32(op_regs, XHCI_OP_USBSTS) & USBSTS_CNR) == 0) break;
        for (volatile int d=0; d<1000; d++) __asm__ volatile("pause");
    }
    
    uint64_t dcbaa_phys = pmm_alloc_low32(); 
    dcbaa_virt = (uint64_t *)(dcbaa_phys + hhdm_offset);
    memset(dcbaa_virt, 0, PAGE_SIZE);
    xhci_write64(op_regs, XHCI_OP_DCBAAP, dcbaa_phys);
    
    uint64_t cmd_ring_phys = pmm_alloc_low32();
    cmd_ring_virt = (struct xhci_trb *)(cmd_ring_phys + hhdm_offset);
    memset(cmd_ring_virt, 0, PAGE_SIZE);
    xhci_write64(op_regs, XHCI_OP_CRCR, cmd_ring_phys | 1); 
    
    uint64_t erst_phys = pmm_alloc_low32();
    erst_virt = (struct xhci_erst_entry *)(erst_phys + hhdm_offset);
    memset(erst_virt, 0, PAGE_SIZE);
    
    evt_ring_phys = pmm_alloc_low32();
    evt_ring_virt = (struct xhci_trb *)(evt_ring_phys + hhdm_offset);
    memset(evt_ring_virt, 0, PAGE_SIZE);
    
    erst_virt[0].ring_segment_base = evt_ring_phys;
    erst_virt[0].ring_segment_size = PAGE_SIZE / sizeof(struct xhci_trb);
    
    volatile uint8_t *ir0 = rt_regs + XHCI_RT_IR0;
    xhci_write32(ir0, XHCI_IR_ERSTSZ, 1);
    xhci_write64(ir0, XHCI_IR_ERDP, evt_ring_phys | (1 << 3));
    xhci_write64(ir0, XHCI_IR_ERSTBA, erst_phys);
    
    uint32_t config = xhci_read32(op_regs, XHCI_OP_CONFIG);
    config &= ~0xFF;
    config |= max_slots;
    xhci_write32(op_regs, XHCI_OP_CONFIG, config);
    
    xhci_write32(op_regs, XHCI_OP_USBCMD, USBCMD_RUN);
    for (int i=0; i<10000; i++) {
        if ((xhci_read32(op_regs, XHCI_OP_USBSTS) & USBSTS_HCH) == 0) break;
        for (volatile int d=0; d<1000; d++) __asm__ volatile("pause");
    }
    
    early_blog(BLOG_OK, "xHCI", "Host Controller started successfully!");
    
    // 2. Power on all root hub ports (Port Power - PP bit 9 in PORTSC)
    // Ensures physical 5V VBUS power is delivered regardless of motherboard jumper state
    for (uint8_t i = 1; i <= num_ports; i++) {
        uint32_t portsc_offset = XHCI_OP_PORTSC_BASE + (i - 1) * 16;
        uint32_t portsc = xhci_read32(op_regs, portsc_offset);
        if (!(portsc & (1 << 9))) {
            // Write 1 to PP (bit 9), preserve control bits, mask write-1-to-clear status bits
            uint32_t write_val = (portsc & ~0x00FE0000) | (1 << 9);
            xhci_write32(op_regs, portsc_offset, write_val);
        }
    }
    
    // Wait ~20ms for VBUS power to stabilize
    for (volatile int d = 0; d < 200000; d++) __asm__ volatile("pause");
    
    // Scan root hub ports
    for (uint8_t i = 1; i <= num_ports; i++) {
        uint32_t portsc_offset = XHCI_OP_PORTSC_BASE + (i - 1) * 16;
        uint32_t portsc = xhci_read32(op_regs, portsc_offset);
        if (portsc & 1) {
            kprintf("[xHCI] Port %d: Device connected! (Speed=%d)\n", i, (portsc >> 10) & 0xF);
        }
    }
    
    return true;
}

void xhci_ring_doorbell(uint8_t slot, uint8_t target) {
    if (!db_regs) return;
    xhci_write32((volatile uint8_t *)db_regs, slot * 4, target);
}

void xhci_queue_cmd(struct xhci_trb *trb) {
    // In a real driver, we track the enqueue pointer and toggle the cycle bit.
    // For this minimal stub, we assume the command ring is empty.
    static int cmd_idx = 0;
    static uint32_t cycle_state = 1;
    
    trb->control |= cycle_state; // Set Cycle Bit
    cmd_ring_virt[cmd_idx] = *trb;
    
    __asm__ volatile("clflush (%0)" :: "r"(&cmd_ring_virt[cmd_idx]) : "memory");
    
    cmd_idx++;
    if (cmd_idx >= 255) { // Handle Link TRB
        cmd_idx = 0;
        cycle_state ^= 1;
    }
    
    xhci_ring_doorbell(0, 0); // Ring Host Controller Doorbell (Slot 0, Target 0)
}

void xhci_poll_events(void) {
    if (!evt_ring_virt) return; // Prevent Page Fault if xHCI is not present

    static int event_idx = 0;
    static uint32_t cycle_state = 1;

    while (1) {
        struct xhci_trb *trb = &evt_ring_virt[event_idx];
        
        if ((trb->control & 1) != cycle_state) {
            break; // No new events
        }

        uint32_t trb_type = (trb->control >> 10) & 0x3F;

        if (trb_type == 34) { // Port Status Change Event
            uint8_t port_id = (trb->param1 >> 24) & 0xFF;
            kprintf("[xHCI] Event: Port %d status changed\n", port_id);
            // Issue Enable Slot Command
            struct xhci_trb cmd = {0};
            cmd.control = (TRB_TYPE_ENABLE_SLOT_CMD << 10);
            xhci_queue_cmd(&cmd);
        } else if (trb_type == TRB_TYPE_TRANSFER_EVENT) {
            extern void xhci_handle_transfer_event(struct xhci_trb *trb);
            xhci_handle_transfer_event(trb);
        } else if (trb_type == 33) { // Command Completion Event
            uint32_t slot_id = (trb->control >> 24) & 0xFF;
            uint32_t comp_code = (trb->status >> 24) & 0xFF;
            uint64_t cmd_ptr = ((uint64_t)trb->param2 << 32) | trb->param1;
            (void)cmd_ptr;
            kprintf("[xHCI] Event: Command Completed (Slot %d, Code %d)\n", slot_id, comp_code);
            
            if (comp_code == 1 && slot_id > 0 && slot_id < 32) {
                uint32_t cmd_trb_type = 0;
                if (cmd_ptr >= (uint64_t)cmd_ring_virt && cmd_ptr < (uint64_t)cmd_ring_virt + 4096) {
                    struct xhci_trb *cmd_trb = (struct xhci_trb *)cmd_ptr;
                    cmd_trb_type = (cmd_trb->control >> 10) & 0x3F;
                }
                
                if (cmd_trb_type == TRB_TYPE_ENABLE_SLOT_CMD) {
                    extern void xhci_address_device(uint8_t slot_id);
                    xhci_address_device(slot_id);
                } else if (cmd_trb_type == TRB_TYPE_ADDRESS_DEVICE_CMD) {
                    extern void xhci_configure_endpoint(uint8_t slot_id);
                    xhci_configure_endpoint(slot_id);
                } else if (cmd_trb_type == TRB_TYPE_CONFIGURE_EP_CMD) {
                    extern void xhci_queue_keyboard_transfer(uint8_t slot_id);
                    xhci_queue_keyboard_transfer(slot_id);
                }
            }
        } else {
            kprintf("[xHCI] Event: Unknown TRB Type %d\n", trb_type);
        }

        event_idx++;
        if (event_idx >= 255) {
            event_idx = 0;
            cycle_state ^= 1;
        }

        // Update ERDP (Event Ring Dequeue Pointer) with physical address
        if (evt_ring_phys) {
            uint64_t erdp_phys = evt_ring_phys + (uint64_t)event_idx * sizeof(struct xhci_trb);
            volatile uint8_t *ir0 = rt_regs + XHCI_RT_IR0;
            // The ERDP needs to have the "Dequeue ERDP" bit (bit 3) set to tell HW we consumed it
            xhci_write64(ir0, XHCI_IR_ERDP, erdp_phys | (1 << 3));
        }
    }
}

void xhci_address_device(uint8_t slot_id) {
    if (slot_id == 0 || slot_id >= 32) return;
    
    xhci_alloc_slot_structs(slot_id);
    
    struct xhci_input_context *ictx = input_contexts_virt[slot_id];
    struct xhci_device_context *octx = device_contexts_virt[slot_id];
    
    // Clear contexts
    memset(ictx, 0, sizeof(struct xhci_input_context));
    memset(octx, 0, sizeof(struct xhci_device_context));
    
    // Set Output Context in DCBAA
    dcbaa_virt[slot_id] = device_contexts_phys[slot_id];
    
    // Setup Input Control Context: Add Slot Context (Bit 0) and EP0 Context (Bit 1)
    ictx->control.add_context_flags = 3;
    
    // Setup Slot Context
    // 1 Context Entry (EP0), Speed = 2 (Full Speed for now, ideally read from port)
    ictx->dev.slot.info1 = (1 << 27); 
    
    // Setup EP0 Context
    // EP Type = Control (4), Max Packet Size = 8 (for low speed) or 64. Let's use 8.
    // Error Count = 3
    ictx->dev.ep[0].info2 = (3 << 1) | (4 << 16) | (8 << 16); 
    
    // Set EP0 TR Dequeue Pointer
    uint64_t ep0_ring_phys = ep0_rings_phys[slot_id];
    ictx->dev.ep[0].tr_dequeue_ptr = ep0_ring_phys | 1; // DCS = 1
    
    // Issue Address Device Command
    struct xhci_trb cmd = {0};
    uint64_t ictx_phys = input_contexts_phys[slot_id];
    cmd.param1 = (uint32_t)ictx_phys;
    cmd.param2 = (uint32_t)(ictx_phys >> 32);
    // Block Set Address (BSR) = 0 so it actually sets the address
    cmd.control = (TRB_TYPE_ADDRESS_DEVICE_CMD << 10) | (slot_id << 24);
    
    kprintf("[xHCI] Issuing Address Device for Slot %d\n", slot_id);
    xhci_queue_cmd(&cmd);
}

static pci_driver_t xhci_driver = {
    .name = "xHCI (USB 3.0) Controller",
    .match_type = DRIVER_MATCH_CLASS,
    .class_code = 0x0C,
    .subclass = 0x03,
    .init = xhci_driver_init
};

void xhci_init(void) {
    driver_register(&xhci_driver);
}

void xhci_configure_endpoint(uint8_t slot_id) {
    if (slot_id == 0 || slot_id >= 32) return;
    
    struct xhci_input_context *ictx = input_contexts_virt[slot_id];
    
    memset(ictx, 0, sizeof(struct xhci_input_context));
    
    // Setup Input Control Context: Add EP1 IN Context (Bit 3) and Slot Context (Bit 0)
    ictx->control.add_context_flags = (1 << 3) | (1 << 0);
    ictx->control.drop_context_flags = 0;
    
    ictx->dev.slot.info1 = (1 << 27); // 1 Context Entry
    
    // Setup EP1 IN Context (Endpoint ID 3 = index 2)
    // EP Type = Interrupt IN (7), Max Packet Size = 8, Error Count = 3
    ictx->dev.ep[2].info2 = (3 << 1) | (7 << 16) | (8 << 16);
    
    uint64_t ep1_ring_phys = ep1_in_rings_phys[slot_id];
    ictx->dev.ep[2].tr_dequeue_ptr = ep1_ring_phys | 1; // DCS = 1
    
    // Issue Configure Endpoint Command
    struct xhci_trb cmd = {0};
    uint64_t ictx_phys = input_contexts_phys[slot_id];
    cmd.param1 = (uint32_t)ictx_phys;
    cmd.param2 = (uint32_t)(ictx_phys >> 32);
    cmd.control = (TRB_TYPE_CONFIGURE_EP_CMD << 10) | (slot_id << 24);
    
    kprintf("[xHCI] Issuing Configure Endpoint for Slot %d\n", slot_id);
    xhci_queue_cmd(&cmd);
}

static const uint8_t kb_usb_to_ps2[256] = {
    // 0x00 - 0x03: Reserved
    [0x04] = 0x1E, // 'a'
    [0x05] = 0x30, // 'b'
    [0x06] = 0x2E, // 'c'
    [0x07] = 0x20, // 'd'
    [0x08] = 0x12, // 'e'
    [0x09] = 0x21, // 'f'
    [0x0A] = 0x22, // 'g'
    [0x0B] = 0x23, // 'h'
    [0x0C] = 0x17, // 'i'
    [0x0D] = 0x24, // 'j'
    [0x0E] = 0x25, // 'k'
    [0x0F] = 0x26, // 'l'
    [0x10] = 0x32, // 'm'
    [0x11] = 0x31, // 'n'
    [0x12] = 0x18, // 'o'
    [0x13] = 0x19, // 'p'
    [0x14] = 0x10, // 'q'
    [0x15] = 0x13, // 'r'
    [0x16] = 0x1F, // 's'
    [0x17] = 0x14, // 't'
    [0x18] = 0x16, // 'u'
    [0x19] = 0x2F, // 'v'
    [0x1A] = 0x11, // 'w'
    [0x1B] = 0x2D, // 'x'
    [0x1C] = 0x15, // 'y'
    [0x1D] = 0x2C, // 'z'
    [0x1E] = 0x02, // '1'
    [0x1F] = 0x03, // '2'
    [0x20] = 0x04, // '3'
    [0x21] = 0x05, // '4'
    [0x22] = 0x06, // '5'
    [0x23] = 0x07, // '6'
    [0x24] = 0x08, // '7'
    [0x25] = 0x09, // '8'
    [0x26] = 0x0A, // '9'
    [0x27] = 0x0B, // '0'
    [0x28] = 0x1C, // Enter
    [0x29] = 0x01, // Escape
    [0x2A] = 0x0E, // Backspace
    [0x2B] = 0x0F, // Tab
    [0x2C] = 0x39, // Space
    [0x2D] = 0x0C, // -
    [0x2E] = 0x0D, // =
    [0x2F] = 0x1A, // [
    [0x30] = 0x1B, // ]
    [0x31] = 0x2B, // \
    [0x33] = 0x27, // ;
    [0x34] = 0x28, // '
    [0x35] = 0x29, // `
    [0x36] = 0x33, // ,
    [0x37] = 0x34, // .
    [0x38] = 0x35, // /
    [0x39] = 0x3A, // CapsLock
    [0x3A] = 0x3B, // F1
    [0x3B] = 0x3C, // F2
    [0x3C] = 0x3D, // F3
    [0x3D] = 0x3E, // F4
    [0x3E] = 0x3F, // F5
    [0x3F] = 0x40, // F6
    [0x40] = 0x41, // F7
    [0x41] = 0x42, // F8
    [0x42] = 0x43, // F9
    [0x43] = 0x44, // F10
    [0x44] = 0x57, // F11
    [0x45] = 0x58, // F12
    [0x49] = 0x52, // Insert
    [0x4A] = 0x47, // Home
    [0x4B] = 0x49, // Page Up
    [0x4C] = 0x53, // Delete
    [0x4D] = 0x4F, // End
    [0x4E] = 0x51, // Page Down
    [0x4F] = 0x4D, // Right Arrow
    [0x50] = 0x4B, // Left Arrow
    [0x51] = 0x50, // Down Arrow
    [0x52] = 0x48, // Up Arrow
};

void xhci_queue_keyboard_transfer(uint8_t slot_id) {
    if (slot_id == 0 || slot_id >= 32) return;
    
    uint64_t buf_phys = kb_buffers_phys[slot_id];
    
    struct xhci_trb trb = {0};
    trb.param1 = (uint32_t)buf_phys;
    trb.param2 = (uint32_t)(buf_phys >> 32);
    trb.status = 8; // length = 8 bytes
    trb.control = (TRB_NORMAL << 10) | (1 << 5); // IOC = 1
    
    static int ring_idx[32] = {0};
    static uint32_t cycle[32] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    
    trb.control |= cycle[slot_id];
    
    ep1_in_rings_virt[slot_id][ring_idx[slot_id]] = trb;
    __asm__ volatile("clflush (%0)" :: "r"(&ep1_in_rings_virt[slot_id][ring_idx[slot_id]]) : "memory");
    
    ring_idx[slot_id]++;
    if (ring_idx[slot_id] >= 255) {
        ring_idx[slot_id] = 0;
        cycle[slot_id] ^= 1;
    }
    
    xhci_ring_doorbell(slot_id, 3);
}

void xhci_handle_transfer_event(struct xhci_trb *trb) {
    uint8_t slot_id = (trb->control >> 24) & 0xFF;
    uint8_t endpoint_id = (trb->control >> 16) & 0x1F;
    
    if (endpoint_id == 3 && slot_id < 32) { // EP1 IN
        uint8_t *buf = kb_buffers_virt[slot_id];
        static uint8_t prev_modifiers[32] = {0};
        static uint8_t prev_keys[32][6] = {{0}};

        uint8_t mod = buf[0];
        uint8_t prev_mod = prev_modifiers[slot_id];
        extern void input_keyboard_push(uint8_t scancode, bool is_release);

        // Check modifiers
        // Bit 0: Left Ctrl (0x1D)
        if ((mod & 0x01) != (prev_mod & 0x01)) input_keyboard_push(0x1D, !(mod & 0x01));
        // Bit 1: Left Shift (0x2A)
        if ((mod & 0x02) != (prev_mod & 0x02)) input_keyboard_push(0x2A, !(mod & 0x02));
        // Bit 2: Left Alt (0x38)
        if ((mod & 0x04) != (prev_mod & 0x04)) input_keyboard_push(0x38, !(mod & 0x04));
        // Bit 4: Right Ctrl (0x1D)
        if ((mod & 0x10) != (prev_mod & 0x10)) input_keyboard_push(0x1D, !(mod & 0x10));
        // Bit 5: Right Shift (0x36)
        if ((mod & 0x20) != (prev_mod & 0x20)) input_keyboard_push(0x36, !(mod & 0x20));
        // Bit 6: Right Alt (0x38)
        if ((mod & 0x40) != (prev_mod & 0x40)) input_keyboard_push(0x38, !(mod & 0x40));

        prev_modifiers[slot_id] = mod;

        // Process released keys
        for (int p = 0; p < 6; p++) {
            uint8_t old_k = prev_keys[slot_id][p];
            if (old_k >= 4) {
                bool still_pressed = false;
                for (int cur = 2; cur < 8; cur++) {
                    if (buf[cur] == old_k) { still_pressed = true; break; }
                }
                if (!still_pressed) {
                    uint8_t scancode = kb_usb_to_ps2[old_k];
                    if (scancode) input_keyboard_push(scancode, true);
                }
            }
        }

        // Process newly pressed keys
        for (int cur = 2; cur < 8; cur++) {
            uint8_t new_k = buf[cur];
            if (new_k >= 4) {
                bool was_pressed = false;
                for (int p = 0; p < 6; p++) {
                    if (prev_keys[slot_id][p] == new_k) { was_pressed = true; break; }
                }
                if (!was_pressed) {
                    uint8_t scancode = kb_usb_to_ps2[new_k];
                    if (scancode) input_keyboard_push(scancode, false);
                }
            }
        }

        // Save current key report
        for (int i = 0; i < 6; i++) {
            prev_keys[slot_id][i] = buf[2 + i];
        }

        memset(buf, 0, 8);
        xhci_queue_keyboard_transfer(slot_id);
    }
}
