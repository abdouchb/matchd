#include "kernel/pci.h"
#include "kernel/vmm.h"
#include "kernel/driver.h"
#include "kernel/early_boot_log.h"

extern uint64_t hhdm_offset;
extern uint64_t *kernel_pml4;
extern void kprintf(const char *fmt, ...);

/* ================== EHCI Handoff ================== */
static void ehci_handoff(const pci_device_t *dev) {
    uint32_t bar0 = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x10);
    uint32_t phys_base = bar0 & 0xFFFFFFF0;
    if (!phys_base) return;

    vmm_map_mmio(kernel_pml4, phys_base + hhdm_offset, phys_base, PTE_PRESENT | PTE_WRITABLE | 0x18);
    volatile uint32_t *regs = (volatile uint32_t *)(phys_base + hhdm_offset);
    
    // HCCPARAMS is at offset 0x08
    uint32_t hccparams = regs[2];
    uint32_t eecp = (hccparams >> 8) & 0xFF; // EHCI Extended Capabilities Pointer
    
    if (eecp >= 0x40) { // Valid EECP in PCI config space
        uint32_t legsup = pci_read_config_dword(dev->bus, dev->device, dev->function, eecp);
        
        if ((legsup & 0xFF) == 1) { // Capability 1 is Legacy Support
            if (legsup & (1 << 16)) { // BIOS Owned Semaphore is set
                kprintf("[USB] EHCI BIOS owns controller. Requesting handoff...\n");
                
                // Set OS Owned Semaphore
                legsup |= (1 << 24);
                pci_write_config_dword(dev->bus, dev->device, dev->function, eecp, legsup);
                
                // Wait for BIOS to release (bit 16 should clear)
                int timeout = 0;
                while ((pci_read_config_dword(dev->bus, dev->device, dev->function, eecp) & (1 << 16)) && timeout < 1000) {
                    timeout++;
                    for (volatile int i = 0; i < 10000; i++) __asm__ volatile("pause");
                }
                
                if (timeout >= 1000) {
                    kprintf("[USB] WARN: EHCI BIOS Handoff timed out! Forcing OS ownership...\n");
                    // Force clear BIOS ownership
                    legsup = pci_read_config_dword(dev->bus, dev->device, dev->function, eecp);
                    legsup &= ~(1 << 16);
                    pci_write_config_dword(dev->bus, dev->device, dev->function, eecp, legsup);
                } else {
                    kprintf("[USB] EHCI BIOS Handoff successful.\n");
                }
            }
            
            // Disable SMIs
            uint32_t legctlsts = pci_read_config_dword(dev->bus, dev->device, dev->function, eecp + 4);
            legctlsts &= 0xFFFF0000; // Clear SMI enables
            legctlsts |= 0xE0000000; // Clear SMI status
            pci_write_config_dword(dev->bus, dev->device, dev->function, eecp + 4, legctlsts);
        }
    }
}

/* ================== xHCI Handoff ================== */
static void xhci_handoff(const pci_device_t *dev) {
    uint32_t bar0_low = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x10);
    uint32_t bar0_high = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x14);
    uint64_t phys_base = bar0_low & 0xFFFFFFF0;
    if ((bar0_low & 0x04) != 0) phys_base |= ((uint64_t)bar0_high << 32);
    if (!phys_base) return;

    vmm_map_mmio(kernel_pml4, phys_base + hhdm_offset, phys_base, PTE_PRESENT | PTE_WRITABLE | 0x18);
    volatile uint32_t *regs = (volatile uint32_t *)(phys_base + hhdm_offset);
    
    // HCCPARAMS1 is at offset 0x10
    uint32_t hccparams1 = regs[4];
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFF;
    
    if (xecp) {
        // xecp is in DWORDs (multiply by 4 to get byte offset from base)
        volatile uint32_t *ext_cap = (volatile uint32_t *)((uint8_t *)regs + (xecp * 4));
        
        while (1) {
            uint32_t cap = *ext_cap;
            uint8_t cap_id = cap & 0xFF;
            
            if (cap_id == 1) { // USB Legacy Support Capability
                if (cap & (1 << 16)) { // BIOS Owned
                    kprintf("[USB] xHCI BIOS owns controller. Requesting handoff...\n");
                    
                    // Set OS Owned Semaphore
                    *ext_cap = cap | (1 << 24);
                    
                    int timeout = 0;
                    while ((*ext_cap & (1 << 16)) && timeout < 1000) {
                        timeout++;
                        for (volatile int i = 0; i < 10000; i++) __asm__ volatile("pause");
                    }
                    
                    if (timeout >= 1000) {
                        kprintf("[USB] WARN: xHCI BIOS Handoff timed out! Forcing OS ownership...\n");
                        *ext_cap = (*ext_cap & ~(1 << 16));
                    } else {
                        kprintf("[USB] xHCI BIOS Handoff successful.\n");
                    }
                }
                
                // Disable SMIs
                volatile uint32_t *legctlsts = ext_cap + 1; // +4 bytes
                uint32_t val = *legctlsts;
                val &= 0xFFFF0000; // Clear SMI enables
                val |= 0xE0000000; // Clear SMI status
                *legctlsts = val;
                
                break;
            }
            
            uint8_t next = (cap >> 8) & 0xFF;
            if (!next) break;
            ext_cap = (volatile uint32_t *)((uint8_t *)ext_cap + (next * 4));
        }
    }
}

static bool usb_driver_init(const pci_device_t *dev) {
    if (dev->class_code != 0x0C || dev->subclass != 0x03) return false;
    
    if (dev->prog_if == 0x20) {
        ehci_handoff(dev);
    } else if (dev->prog_if == 0x30) {
        xhci_handoff(dev);
    }
    
    // We don't actually manage the controller, we just do the handoff to stop SMM
    return false; // Return false so driver manager doesn't "claim" it indefinitely
}

static pci_driver_t usb_handoff_driver = {
    .name = "USB BIOS Handoff",
    .match_type = DRIVER_MATCH_CLASS,
    .class_code = 0x0C,
    .subclass = 0x03,
    .init = usb_driver_init
};

bool usb_handoff_init(void) {
    driver_register(&usb_handoff_driver);
    return true;
}
