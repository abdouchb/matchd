#ifndef FILESYSTEM_FAT_H
#define FILESYSTEM_FAT_H

#include "zarqaa/types.h"

/* Initialize the FAT filesystem by probing the primary ATA drive */
bool fat_init(void);

/* Check if FAT is mounted and optionally retrieve the volume label */
bool fat_is_mounted(char *label_out);

#endif /* FILESYSTEM_FAT_H */
