#ifndef DRIVERS_AHCI_H
#define DRIVERS_AHCI_H

#include "zarqaa/types.h"
#include "kernel/pci.h"

#define AHCI_SECTOR_SIZE 512

/* Initialize the AHCI driver */
bool ahci_init(void);

/* Check if AHCI driver is available and ready */
bool ahci_is_available(void);

/* Read sectors from the first available AHCI drive */
bool ahci_read_sectors(uint32_t lba, uint32_t sector_count, uint8_t *buffer);

/* Write sectors to the first available AHCI drive */
bool ahci_write_sectors(uint32_t lba, uint32_t sector_count, const uint8_t *buffer);

#endif /* DRIVERS_AHCI_H */
