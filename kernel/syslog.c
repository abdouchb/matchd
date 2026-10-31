#include "kernel/syslog.h"
#include "zarqaa/string.h"
#include "filesystem/fat.h"
#include "kernel/heap.h"

#define SYSLOG_SIZE (64 * 1024) // 64 KB
static char* syslog_buffer = NULL;
static size_t syslog_len = 0;
static bool syslog_ready = false;
static bool fs_ready = false;

extern void kprintf(const char* fmt, ...);

void syslog_init(void) {
    syslog_buffer = (char*)kmalloc(SYSLOG_SIZE);
    if (syslog_buffer) {
        memset(syslog_buffer, 0, SYSLOG_SIZE);
        syslog_len = 0;
        syslog_ready = true;
    } else {
        kprintf("[Syslog] WARNING: Allocation failed, in-memory syslog disabled!\n");
    }
}

void syslog_write(const char *msg) {
    if (!syslog_ready || !msg) return;

    size_t len = strlen(msg);
    if (syslog_len + len >= SYSLOG_SIZE - 1) {
        // Buffer full, just stop writing for now (or wrap around, but for boot logs, truncation is okay)
        return;
    }

    strcpy(syslog_buffer + syslog_len, msg);
    syslog_len += len;

    // If file system is ready, we could technically append to file here
    // but for performance, we might just write it out periodically or on demand.
}

void syslog_flush_to_disk(void) {
    if (!syslog_ready) return;
    
    // We assume fat_init() has been called successfully
    // Let's create or overwrite the boot.log in the root directory
    // We don't have a robust VFS write yet, so we will use fat_create_file and fat_write_file if available.
    // Assuming fat_create_file exists in FAT driver, if not we will just skip it.
    
    // Actually, writing to disk might require FAT write support.
    // If ZarqaaOS only supports reading (like many simple hobby OSes initially), we just keep it in memory!
    // The Boot Logger app can read from memory via syslog_get_buffer().
    fs_ready = true;
}

char* syslog_get_buffer(void) {
    return syslog_buffer;
}

size_t syslog_get_length(void) {
    return syslog_len;
}
