#include "filesystem/tar.h"
#include "filesystem/vfs.h"
#include "zarqaa/string.h"
#include "zarqaa/types.h"
#include "drivers/serial.h"

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};

static size_t parse_octal(const char *str, int max_len) {
    size_t size = 0;
    for (int i = 0; i < max_len; i++) {
        if (str[i] == ' ' || str[i] == '\0') break;
        if (str[i] < '0' || str[i] > '7') break;
        size = size * 8 + (str[i] - '0');
    }
    return size;
}

static void ensure_dir(const char *path) {
    char p[256];
    strncpy(p, path, 256);
    char *slash = strchr(p + 1, '/');
    while (slash) {
        *slash = '\0';
        if (!vfs_resolve_path(p)) {
            vfs_create(p, VFS_DIRECTORY);
        }
        *slash = '/';
        slash = strchr(slash + 1, '/');
    }
}

void tar_init(void *archive_addr, size_t archive_size) {
    if (!archive_addr || archive_size == 0) return;
    
    kprintf("[TAR] Initializing Ramdisk from %p (size %d bytes)\n", archive_addr, archive_size);
    
    // Create the virtual hd0 root since the fat driver won't be used
    vfs_create_dir(vfs_get_root(), "hd0");
    
    uint8_t *ptr = (uint8_t *)archive_addr;
    uint8_t *end = ptr + archive_size;
    
    while (ptr < end) {
        struct tar_header *header = (struct tar_header *)ptr;
        
        // Check for end of archive (two 512-byte blocks of nulls)
        if (header->name[0] == '\0') {
            break;
        }
        
        size_t size = parse_octal(header->size, 11);
        
        // Construct path. The tar is inside fat_root, so paths are like "www/index.html"
        char full_path[256];
        strcpy(full_path, "/hd0/");
        
        // Combine prefix and name if it's USTAR
        if (strncmp(header->magic, "ustar", 5) == 0 && header->prefix[0] != '\0') {
            strcat(full_path, header->prefix);
            strcat(full_path, "/");
        }
        strcat(full_path, header->name);
        
        // Remove trailing slash for directories to create them cleanly
        int len = strlen(full_path);
        if (len > 0 && full_path[len - 1] == '/') {
            full_path[len - 1] = '\0';
        }
        
        ensure_dir(full_path);
        
        if (header->typeflag == '5') {
            // Directory
            // Parent directories are usually automatically created if we ensure ordering,
            // but VFS create logic handles directory creation.
            vfs_create(full_path, VFS_DIRECTORY);
            kprintf("[TAR] Created Dir: %s\n", full_path);
        } else if (header->typeflag == '0' || header->typeflag == '\0') {
            // Normal file (Zero-copy directly from initrd archive to save 36+ MB of heap)
            struct vfs_node *node = vfs_create(full_path, VFS_FILE);
            if (node) {
                node->data = (void *)(ptr + 512);
                node->size = size;
                kprintf("[TAR] Loaded File: %s (%d bytes, Zero-Copy)\n", full_path, size);
            }
        }
        
        // Move to next 512-byte boundary
        ptr += 512 + ((size + 511) / 512) * 512;
    }
    
    kprintf("[TAR] Ramdisk initialized successfully.\n");
}
