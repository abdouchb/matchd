#include "filesystem/fat.h"
#include "filesystem/vfs.h"
#include "drivers/ata.h"
#include "drivers/ahci.h"
#include "kernel/heap.h"
#include "zarqaa/types.h"
#include "zarqaa/string.h"
#include "kernel/early_boot_log.h"

#pragma pack(push, 1)
struct fat_bpb {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_dir_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    /* FAT32 Extended fields */
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
};

struct fat_dir_entry {
    char     name[11];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t cluster_low;
    uint32_t size;
};
#pragma pack(pop)

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LFN       (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

static uint32_t partition_lba = 0;
static struct fat_bpb bpb;
static uint32_t fat_start_lba;
static uint32_t data_start_lba;
static uint32_t root_dir_lba;
static uint32_t root_dir_sectors;
static bool is_fat32 = false;

static bool is_mounted = false;
static char volume_name[12] = "USB Drive";

static uint32_t cluster_to_lba(uint32_t cluster) {
    if (is_fat32) {
        return data_start_lba + (cluster - 2) * bpb.sectors_per_cluster;
    } else {
        return data_start_lba + (cluster - 2) * bpb.sectors_per_cluster;
    }
}

static bool storage_read_sectors(uint32_t lba, uint32_t sector_count, uint8_t *buffer) {
    /* Try AHCI first, if it fails fall back to ATA/PIO */
    if (ahci_is_available()) {
        if (ahci_read_sectors(lba, sector_count, buffer)) {
            return true;
        }
    }
    return ata_read_sectors(lba, sector_count, buffer);
}

static uint32_t fat_get_next_cluster(uint32_t cluster) {
    if (cluster < 2) return 0x0FFFFFFF;
    
    uint8_t *fat_buf = (uint8_t *)kmalloc(512);
    if (!fat_buf) return 0x0FFFFFFF;
    
    uint32_t next = 0x0FFFFFFF;
    if (is_fat32) {
        uint32_t fat_offset = cluster * 4;
        uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
        uint32_t ent_offset = fat_offset % 512;
        if (storage_read_sectors(fat_sector, 1, fat_buf)) {
            next = (*(uint32_t *)(fat_buf + ent_offset)) & 0x0FFFFFFF;
            if (next >= 0x0FFFFFF8 || next == 0) next = 0x0FFFFFFF;
        }
    } else {
        uint32_t fat_offset = cluster * 2;
        uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
        uint32_t ent_offset = fat_offset % 512;
        if (storage_read_sectors(fat_sector, 1, fat_buf)) {
            next = *(uint16_t *)(fat_buf + ent_offset);
            if (next >= 0xFFF8 || next == 0) next = 0x0FFFFFFF;
        }
    }
    
    kfree(fat_buf);
    return next;
}

void fat_parse_dir(uint32_t start_cluster, struct vfs_node *parent) {
    uint8_t *sector_buf = (uint8_t *)kmalloc(512);
    if (!sector_buf) return;
    
    if (start_cluster == 0 && !is_fat32) {
        uint32_t current_lba = root_dir_lba;
        for (uint32_t s = 0; s < root_dir_sectors; s++) {
            if (!storage_read_sectors(current_lba + s, 1, sector_buf)) {
                kfree(sector_buf);
                return;
            }
            
            struct fat_dir_entry *entries = (struct fat_dir_entry *)sector_buf;
            for (int i = 0; i < 512 / sizeof(struct fat_dir_entry); i++) {
                if (entries[i].name[0] == 0x00) {
                    kfree(sector_buf);
                    return; // End of directory
                }
                if ((uint8_t)entries[i].name[0] == 0xE5) continue; // Deleted
                if (entries[i].attr == FAT_ATTR_LFN) continue; // Skip LFN
                if (entries[i].attr & FAT_ATTR_VOLUME_ID) {
                    int k = 0;
                    for (int j = 0; j < 11; j++) {
                        if (entries[i].name[j] != ' ') volume_name[k++] = entries[i].name[j];
                    }
                    volume_name[k] = '\0';
                    continue;
                }
                
                // Parse 8.3 name
                char filename[13];
                int j, k = 0;
                for (j = 0; j < 8 && entries[i].name[j] != ' '; j++) {
                    filename[k++] = entries[i].name[j];
                }
                if (entries[i].name[8] != ' ') {
                    filename[k++] = '.';
                    for (j = 8; j < 11 && entries[i].name[j] != ' '; j++) {
                        filename[k++] = entries[i].name[j];
                    }
                }
                filename[k] = '\0';
                
                uint32_t cluster = entries[i].cluster_low | (entries[i].cluster_high << 16);
                int type = (entries[i].attr & FAT_ATTR_DIRECTORY) ? VFS_DIRECTORY : VFS_FILE;
                vfs_create_fat_node(parent, filename, type, entries[i].size, cluster);
            }
        }
    } else {
        // FAT32 root directory or FAT12/16/32 subdirectories (walking cluster chain)
        uint32_t cur_cluster = start_cluster;
        while (cur_cluster < 0x0FFFFFF8 && cur_cluster >= 2) {
            uint32_t current_lba = cluster_to_lba(cur_cluster);
            
            for (uint32_t s = 0; s < bpb.sectors_per_cluster; s++) {
                if (!storage_read_sectors(current_lba + s, 1, sector_buf)) {
                    kfree(sector_buf);
                    return;
                }
                
                struct fat_dir_entry *entries = (struct fat_dir_entry *)sector_buf;
                for (int i = 0; i < 512 / sizeof(struct fat_dir_entry); i++) {
                    if (entries[i].name[0] == 0x00) {
                        kfree(sector_buf);
                        return; // End of directory
                    }
                    if ((uint8_t)entries[i].name[0] == 0xE5) continue;
                    if (entries[i].attr == FAT_ATTR_LFN) continue;
                    if (entries[i].attr & FAT_ATTR_VOLUME_ID) {
                        int k = 0;
                        for (int j = 0; j < 11; j++) {
                            if (entries[i].name[j] != ' ') volume_name[k++] = entries[i].name[j];
                        }
                        volume_name[k] = '\0';
                        continue;
                    }
                    
                    char filename[13];
                    int j, k = 0;
                    for (j = 0; j < 8 && entries[i].name[j] != ' '; j++) {
                        filename[k++] = entries[i].name[j];
                    }
                    if (entries[i].name[8] != ' ') {
                        filename[k++] = '.';
                        for (j = 8; j < 11 && entries[i].name[j] != ' '; j++) {
                            filename[k++] = entries[i].name[j];
                        }
                    }
                    filename[k] = '\0';
                    
                    uint32_t cluster = entries[i].cluster_low | (entries[i].cluster_high << 16);
                    int type = (entries[i].attr & FAT_ATTR_DIRECTORY) ? VFS_DIRECTORY : VFS_FILE;
                    
                    if (strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0) {
                        vfs_create_fat_node(parent, filename, type, entries[i].size, cluster);
                    }
                }
            }
            
            cur_cluster = fat_get_next_cluster(cur_cluster);
        }
    }
    kfree(sector_buf);
}

bool fat_init(void) {
    /* Use heap buffer - stack buffers break AHCI DMA on real hardware */
    extern void *kmalloc(size_t size);
    extern void kfree(void *ptr);
    uint8_t *sector_buf = (uint8_t *)kmalloc(512);
    if (!sector_buf) {
        early_blog(BLOG_FAIL, "FAT", "kmalloc(512) failed!");
        return false;
    }
    
    early_blog(BLOG_INFO, "FAT", "Reading MBR (sector 0)...");
    // Read MBR (Sector 0)
    if (!storage_read_sectors(0, 1, sector_buf)) {
        early_blog(BLOG_WARN, "FAT", "MBR read failed (no disk?)");
        kfree(sector_buf);
        return false;
    }
    early_blog(BLOG_OK, "FAT", "MBR read successful");
    
    // Check boot signature
    if (sector_buf[510] != 0x55 || sector_buf[511] != 0xAA) {
        kprintf("[FAT] No boot signature found on drive 0.\n");
        kfree(sector_buf);
        return false;
    }
    
    // Very simple check: Is this already a FAT BPB or an MBR?
    if ((sector_buf[0] == 0xEB && sector_buf[2] == 0x90) || sector_buf[0] == 0xE9) {
        partition_lba = 0;
    } else {
        // It's an MBR. Find first partition
        uint8_t *part_table = &sector_buf[0x1BE];
        uint8_t sys_id = part_table[4];
        if (sys_id == 0x00) {
            kprintf("[FAT] No partition found.\n");
            kfree(sector_buf);
            return false;
        }
        
        if (sys_id == 0xEE) {
            early_blog(BLOG_INFO, "FAT", "GPT Protective MBR found, parsing GPT...");
            if (!storage_read_sectors(1, 1, sector_buf)) {
                kprintf("[FAT] Failed to read GPT header.\n");
                kfree(sector_buf);
                return false;
            }
            if (memcmp(sector_buf, "EFI PART", 8) != 0) {
                kprintf("[FAT] Invalid GPT signature.\n");
                kfree(sector_buf);
                return false;
            }
            
            uint64_t part_array_lba = *(uint64_t *)&sector_buf[72];
            uint32_t num_parts = *(uint32_t *)&sector_buf[80];
            uint32_t part_size = *(uint32_t *)&sector_buf[84];
            
            bool found = false;
            uint32_t sectors_to_read = (num_parts * part_size + 511) / 512;
            
            for (uint32_t s = 0; s < sectors_to_read; s++) {
                if (!storage_read_sectors(part_array_lba + s, 1, sector_buf)) break;
                for (uint32_t i = 0; i < 512 / part_size; i++) {
                    uint8_t *entry = sector_buf + (i * part_size);
                    uint8_t efi_guid[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};
                    uint8_t data_guid[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};
                    
                    if (memcmp(entry, efi_guid, 16) == 0 || memcmp(entry, data_guid, 16) == 0) {
                        partition_lba = (uint32_t)(*(uint64_t *)&entry[32]);
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
            
            if (!found) {
                kprintf("[FAT] No FAT GPT partition found.\n");
                kfree(sector_buf);
                return false;
            }
        } else {
            partition_lba = *(uint32_t *)&part_table[8];
        }
        kprintf("[FAT] Partition at LBA %u\n", partition_lba);
    }
    
    // Read BPB
    if (!storage_read_sectors(partition_lba, 1, sector_buf)) {
        kfree(sector_buf);
        return false;
    }
    memcpy(&bpb, sector_buf, sizeof(struct fat_bpb));
    kfree(sector_buf);
    
    early_blog(BLOG_INFO, "FAT", "Parsing BPB geometry...");
    fat_start_lba = partition_lba + bpb.reserved_sectors;
    
    uint32_t fat_size = (bpb.sectors_per_fat_16 != 0) ? bpb.sectors_per_fat_16 : bpb.sectors_per_fat_32;
    
    if (bpb.bytes_per_sector == 0 || bpb.sectors_per_cluster == 0) {
        early_blog(BLOG_FAIL, "FAT", "Invalid BPB: sector or cluster size is 0");
        return false;
    }
    
    root_dir_lba = fat_start_lba + (bpb.fat_count * fat_size);
    root_dir_sectors = ((bpb.root_dir_entries * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
    
    data_start_lba = root_dir_lba + root_dir_sectors;
    
    uint32_t total_sectors = (bpb.total_sectors_16 != 0) ? bpb.total_sectors_16 : bpb.total_sectors_32;
    uint32_t data_sectors = total_sectors - (bpb.reserved_sectors + (bpb.fat_count * fat_size) + root_dir_sectors);
    uint32_t total_clusters = data_sectors / bpb.sectors_per_cluster;
    
    char fmt_msg[64];
    if (total_clusters < 4085) {
        is_fat32 = false;
        sprintf(fmt_msg, "Detected FAT12 (%u clusters)", total_clusters);
    } else if (total_clusters < 65525) {
        is_fat32 = false;
        sprintf(fmt_msg, "Detected FAT16 (%u clusters)", total_clusters);
    } else {
        is_fat32 = true;
        sprintf(fmt_msg, "Detected FAT32 (%u clusters)", total_clusters);
    }
    early_blog(BLOG_OK, "FAT", fmt_msg);
    
    early_blog(BLOG_INFO, "FAT", "Creating /hd0 VFS node...");
    struct vfs_node *hd0 = vfs_create_dir(vfs_get_root(), "hd0");
    if (!hd0) {
        early_blog(BLOG_FAIL, "FAT", "Failed to create /hd0 node");
        return false;
    }
    
    early_blog(BLOG_INFO, "FAT", "Parsing root directory...");
    if (is_fat32) {
        fat_parse_dir(bpb.root_cluster, hd0);
    } else {
        fat_parse_dir(0, hd0);
    }
    
    early_blog(BLOG_OK, "FAT", "Root directory parsed");
    is_mounted = true;
    kprintf("[FAT] Mounted successfully to /hd0\n");
    return true;
}

bool fat_is_mounted(char *label_out) {
    if (!is_mounted) return false;
    if (label_out) strcpy(label_out, volume_name);
    return true;
}

int fat_read_file(struct vfs_node *node, void *buffer, size_t size, size_t offset) {
    if (!node || !buffer || size == 0) return 0;
    if (offset >= node->size) return 0;
    
    uint8_t *sector_buf = (uint8_t *)kmalloc(512);
    if (!sector_buf) return 0;
    
    uint32_t cluster_size = bpb.sectors_per_cluster * 512;
    if (cluster_size == 0) {
        kfree(sector_buf);
        return 0;
    }
    
    uint32_t current_cluster = node->fat_cluster;
    uint32_t clusters_to_skip = offset / cluster_size;
    
    // Skip to target cluster
    for (uint32_t k = 0; k < clusters_to_skip; k++) {
        current_cluster = fat_get_next_cluster(current_cluster);
        if (current_cluster >= 0x0FFFFFF8 || current_cluster < 2) {
            kfree(sector_buf);
            return 0;
        }
    }
    
    uint32_t offset_in_cluster = offset % cluster_size;
    uint32_t current_offset = offset;
    uint8_t *out = (uint8_t *)buffer;
    size_t bytes_read = 0;
    
    while (bytes_read < size && current_offset < node->size && current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        uint32_t lba = cluster_to_lba(current_cluster);
        
        for (int i = 0; i < bpb.sectors_per_cluster; i++) {
            if (!storage_read_sectors(lba + i, 1, sector_buf)) {
                kfree(sector_buf);
                return bytes_read;
            }
            
            uint32_t start_copy = 0;
            if (offset_in_cluster > 0) {
                if (offset_in_cluster >= 512) {
                    offset_in_cluster -= 512;
                    continue;
                }
                start_copy = offset_in_cluster;
                offset_in_cluster = 0;
            }
            
            uint32_t to_copy = 512 - start_copy;
            if (bytes_read + to_copy > size) to_copy = size - bytes_read;
            if (current_offset + to_copy > node->size) to_copy = node->size - current_offset;
            
            memcpy(out + bytes_read, sector_buf + start_copy, to_copy);
            bytes_read += to_copy;
            current_offset += to_copy;
            
            if (bytes_read >= size || current_offset >= node->size) break;
        }
        
        current_cluster = fat_get_next_cluster(current_cluster);
    }
    
    kfree(sector_buf);
    return bytes_read;
}
