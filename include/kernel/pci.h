/*
 * Zarqaa OS 0.1 - PCI Discovery Subsystem
 * Creator: Abdelhak Chabbi & AI
 */

#pragma once

#include <stdint.h>

typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  header_type;
    uint8_t  irq_line;      // From Interrupt Line register
    uint32_t bar[6];        // Base Address Registers
} pci_device_t;

// Initialize PCI interface and scan buses
void pci_init(void);

// Config space access
uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t val);
void pci_write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t val);
uint8_t pci_find_capability(uint8_t bus, uint8_t device, uint8_t function, uint8_t cap_id);


// Get the array of discovered devices
const pci_device_t *pci_get_devices(void);

// Get the number of discovered devices
uint32_t pci_get_device_count(void);

// Find a specific device by vendor and device ID
const pci_device_t *pci_find_device(uint16_t vendor, uint16_t device);
