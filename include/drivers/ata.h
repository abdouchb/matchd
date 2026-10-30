#ifndef DRIVERS_ATA_H
#define DRIVERS_ATA_H

#include "zarqaa/types.h"

#define ATA_SECTOR_SIZE 512

/* Initialize the ATA driver */
void ata_init(void);

/* Read sectors from the primary master drive using PIO */
bool ata_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t *buffer);

/* Write sectors to the primary master drive using PIO */
bool ata_write_sectors(uint32_t lba, uint8_t sector_count, const uint8_t *buffer);

#endif /* DRIVERS_ATA_H */
