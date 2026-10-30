/*
 * Zarqaa OS 0.1 - PCI Discovery Subsystem
 * Creator: Abdelhak Chabbi & AI
 */

#include "kernel/pci.h"
#include "zarqaa/types.h"

// I/O Ports for PCI config space access
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static pci_device_t pci_devices[256];
static uint32_t     pci_device_count = 0;

// Build config address
static uint32_t pci_config_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return (uint32_t)(
        (1U << 31)            | // Enable bit
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xFC)
    );
}

uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t data = inl(PCI_CONFIG_DATA);
    return (uint16_t)((data >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t data = inl(PCI_CONFIG_DATA);
    return (uint8_t)((data >> ((offset & 3) * 8)) & 0xFF);
}

uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t val) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, val);
}

void pci_write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t val) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    // Be careful with byte enables, but typically QEMU PCI config handles unaligned/partial writes fine if address is right.
    // For safety, a read-modify-write is better if not strictly aligned, but let's assume aligned offsets.
    uint32_t current = inl(PCI_CONFIG_DATA);
    uint32_t shift = (offset & 2) * 8;
    current &= ~(0xFFFF << shift);
    current |= ((uint32_t)val << shift);
    outl(PCI_CONFIG_DATA, current);
}

uint8_t pci_find_capability(uint8_t bus, uint8_t device, uint8_t function, uint8_t cap_id) {
    uint16_t status = pci_read_config_word(bus, device, function, 0x06);
    if (!(status & 0x10)) return 0; // Capabilities List bit not set
    
    uint8_t cap_ptr = pci_read_config_byte(bus, device, function, 0x34) & 0xFC;
    while (cap_ptr != 0) {
        uint8_t id = pci_read_config_byte(bus, device, function, cap_ptr);
        if (id == cap_id) return cap_ptr;
        cap_ptr = pci_read_config_byte(bus, device, function, cap_ptr + 1) & 0xFC;
    }
    return 0;
}

void pci_init(void) {
    pci_device_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read_config_word((uint8_t)bus, dev, func, 0x00);
                if (vendor == 0xFFFF) {
                    continue; // No device in this slot
                }

                uint16_t device_id  = pci_read_config_word((uint8_t)bus, dev, func, 0x02);
                uint8_t  class_code = pci_read_config_byte((uint8_t)bus, dev, func, 0x0B);
                uint8_t  subclass   = pci_read_config_byte((uint8_t)bus, dev, func, 0x0A);
                uint8_t  prog_if    = pci_read_config_byte((uint8_t)bus, dev, func, 0x09);
                uint8_t  header     = pci_read_config_byte((uint8_t)bus, dev, func, 0x0E);
                uint8_t  irq_line   = pci_read_config_byte((uint8_t)bus, dev, func, 0x3C);

                if (pci_device_count >= 256) break;

                pci_device_t *pdev = &pci_devices[pci_device_count++];
                pdev->bus        = (uint8_t)bus;
                pdev->device     = dev;
                pdev->function   = func;
                pdev->vendor_id  = vendor;
                pdev->device_id  = device_id;
                pdev->class_code = class_code;
                pdev->subclass   = subclass;
                pdev->prog_if    = prog_if;
                pdev->header_type= header;
                pdev->irq_line   = irq_line;

                // Read basic BARs
                for (int bar = 0; bar < 6; bar++) {
                    uint8_t bar_offset = 0x10 + bar * 4;
                    pdev->bar[bar] = pci_read_config_dword((uint8_t)bus, dev, func, bar_offset);
                }

                kprintf("PCI: bus %u dev %u func %u ven %x dev %x class %x irq %u\n",
                        bus, dev, func, vendor, device_id, class_code, irq_line);

                // Check if it's a multi-function device, if we are on func 0
                if (func == 0 && (header & 0x80) == 0) {
                    break; // Not a multi-function device, skip other functions
                }
            }
        }
    }
}

const pci_device_t *pci_get_devices(void) {
    return pci_devices;
}

uint32_t pci_get_device_count(void) {
    return pci_device_count;
}

const pci_device_t *pci_find_device(uint16_t vendor, uint16_t device) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor &&
            pci_devices[i].device_id == device) {
            return &pci_devices[i];
        }
    }
    return 0;
}
