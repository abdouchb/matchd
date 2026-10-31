#include "drivers/ata.h"
#include "zarqaa/types.h"
#include "zarqaa/string.h"
#include "kernel/scheduler.h"
#include "kernel/early_boot_log.h"

extern void kprintf(const char *fmt, ...);

static volatile bool ata_lock = false;
static bool ata_present = false;  /* Set true only if a real drive is detected */

static void ata_lock_acquire(void) {
    /* Use pause-based spin instead of task_yield to avoid calling
     * the scheduler before it is initialized during early boot. */
    while (__atomic_test_and_set(&ata_lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
}

static void ata_lock_release(void) {
    __atomic_clear(&ata_lock, __ATOMIC_RELEASE);
}

/* Primary ATA bus I/O ports */
#define ATA_PORT_DATA         0x1F0
#define ATA_PORT_ERROR        0x1F1
#define ATA_PORT_FEATURES     0x1F1
#define ATA_PORT_SECTOR_COUNT 0x1F2
#define ATA_PORT_LBA_LO       0x1F3
#define ATA_PORT_LBA_MID      0x1F4
#define ATA_PORT_LBA_HI       0x1F5
#define ATA_PORT_DRIVE        0x1F6
#define ATA_PORT_COMMAND      0x1F7
#define ATA_PORT_STATUS       0x1F7

/* Commands */
#define ATA_CMD_READ_PIO      0x20
#define ATA_CMD_WRITE_PIO     0x30
#define ATA_CMD_CACHE_FLUSH   0xE7
#define ATA_CMD_IDENTIFY      0xEC

/* Status bits */
#define ATA_SR_BSY            0x80    /* Busy */
#define ATA_SR_DRDY           0x40    /* Drive ready */
#define ATA_SR_DF             0x20    /* Drive write fault */
#define ATA_SR_DSC            0x10    /* Drive seek complete */
#define ATA_SR_DRQ            0x08    /* Data request ready */
#define ATA_SR_CORR           0x04    /* Corrected data */
#define ATA_SR_IDX            0x02    /* Index */
#define ATA_SR_ERR            0x01    /* Error */

/* Short delay: read alternate status 4 times (~400ns) */
static inline void ata_io_delay(void) {
    inb(0x3F6); inb(0x3F6); inb(0x3F6); inb(0x3F6);
}

static bool ata_wait_bsy(void) {
    /* Short timeout: ~500ms max, not minutes */
    for (int t = 0; t < 500000; t++) {
        uint8_t status = inb(ATA_PORT_STATUS);
        if (status == 0xFF) return false;         /* No device — floating bus */
        if (!(status & ATA_SR_BSY)) return true;  /* Not busy, good */
        __asm__ volatile("pause");
    }
    uint8_t s = inb(ATA_PORT_STATUS);
    kprintf("[ATA] BSY Timeout! Status: 0x%02x\n", s);
    early_blog(BLOG_WARN, "ATA", "BSY timeout");
    return false;
}

static bool ata_wait_drq(void) {
    for (int t = 0; t < 500000; t++) {
        uint8_t status = inb(ATA_PORT_STATUS);
        if (status == 0xFF) return false;         /* Floating bus */
        if (status & ATA_SR_ERR) {
            uint8_t err = inb(ATA_PORT_ERROR);
            kprintf("[ATA] DRQ Error! Status: 0x%02x, Error: 0x%02x\n", status, err);
            return false;
        }
        if (status & ATA_SR_DF) {
            kprintf("[ATA] Drive Fault! Status: 0x%02x\n", status);
            return false;
        }
        if (status & ATA_SR_DRQ) return true;     /* Data ready */
        __asm__ volatile("pause");
    }
    kprintf("[ATA] DRQ Timeout! Status: 0x%02x\n", inb(ATA_PORT_STATUS));
    return false;
}

/*
 * Detect whether a real ATA drive is connected to the primary bus.
 * Uses the IDENTIFY command — the standard way to probe for drives.
 * Returns true if a drive is found, false otherwise.
 */
static bool ata_detect_drive(void) {
    /* Step 1: Select master drive */
    outb(ATA_PORT_DRIVE, 0xA0);
    ata_io_delay();

    /* Step 2: Quick floating-bus check — 0xFF means no controller */
    uint8_t status = inb(ATA_PORT_STATUS);
    if (status == 0xFF) {
        kprintf("[ATA] No controller on primary bus (0xFF)\n");
        early_blog(BLOG_WARN, "ATA", "No controller on primary bus");
        return false;
    }

    /* Step 3: Zero out sector count and LBA ports */
    outb(ATA_PORT_SECTOR_COUNT, 0);
    outb(ATA_PORT_LBA_LO, 0);
    outb(ATA_PORT_LBA_MID, 0);
    outb(ATA_PORT_LBA_HI, 0);

    /* Step 4: Send IDENTIFY command */
    outb(ATA_PORT_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_delay();

    /* Step 5: Read status — if 0, no drive at all */
    status = inb(ATA_PORT_STATUS);
    if (status == 0) {
        kprintf("[ATA] No drive on primary master (status=0)\n");
        early_blog(BLOG_WARN, "ATA", "No drive on primary master");
        return false;
    }

    /* Step 6: Wait for BSY to clear with a SHORT timeout */
    for (int t = 0; t < 100000; t++) {
        status = inb(ATA_PORT_STATUS);
        if (status == 0xFF) return false;
        if (!(status & ATA_SR_BSY)) break;
        if (t == 99999) {
            kprintf("[ATA] IDENTIFY BSY timeout\n");
            early_blog(BLOG_WARN, "ATA", "IDENTIFY BSY timeout");
            return false;
        }
        __asm__ volatile("pause");
    }

    /* Step 7: Check if this is actually an ATA device (not ATAPI/SATA) */
    uint8_t lba_mid = inb(ATA_PORT_LBA_MID);
    uint8_t lba_hi  = inb(ATA_PORT_LBA_HI);
    if (lba_mid != 0 || lba_hi != 0) {
        /* Non-zero means it's ATAPI or SATA — not legacy ATA PIO */
        kprintf("[ATA] Not a legacy ATA device (LBAmid=0x%02x, LBAhi=0x%02x)\n", lba_mid, lba_hi);
        early_blog(BLOG_INFO, "ATA", "Not legacy ATA (SATA/ATAPI)");
        return false;
    }

    /* Step 8: Poll until DRQ or ERR */
    for (int t = 0; t < 100000; t++) {
        status = inb(ATA_PORT_STATUS);
        if (status & ATA_SR_ERR) {
            kprintf("[ATA] IDENTIFY failed (ERR set)\n");
            early_blog(BLOG_WARN, "ATA", "IDENTIFY command failed");
            return false;
        }
        if (status & ATA_SR_DRQ) break;
        if (t == 99999) {
            kprintf("[ATA] IDENTIFY DRQ timeout\n");
            early_blog(BLOG_WARN, "ATA", "IDENTIFY DRQ timeout");
            return false;
        }
        __asm__ volatile("pause");
    }

    /* Step 9: Read and discard the 256-word IDENTIFY data */
    for (int i = 0; i < 256; i++) {
        (void)inw(ATA_PORT_DATA);
    }

    kprintf("[ATA] Drive detected on primary master!\n");
    early_blog(BLOG_OK, "ATA", "Drive detected on primary master");
    return true;
}

void ata_init(void) {
    early_blog(BLOG_INFO, "ATA", "Probing primary master...");
    kprintf("[ATA] Probing Primary Master PIO...\n");

    ata_present = ata_detect_drive();

    if (!ata_present) {
        early_blog(BLOG_INFO, "ATA", "No legacy ATA drive, skipping PIO");
        kprintf("[ATA] No legacy ATA drive found. Skipping PIO init.\n");
    }
}

bool ata_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t *buffer) {
    if (!ata_present) return false;
    ata_lock_acquire();
    
    if (!ata_wait_bsy()) {
        ata_lock_release();
        return false;
    }
    
    /* 0xE0 for master, 0x40 for LBA addressing mode */
    outb(ATA_PORT_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PORT_SECTOR_COUNT, sector_count);
    outb(ATA_PORT_LBA_LO, (uint8_t)lba);
    outb(ATA_PORT_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PORT_LBA_HI, (uint8_t)(lba >> 16));
    
    outb(ATA_PORT_COMMAND, ATA_CMD_READ_PIO);
    
    for (int i = 0; i < sector_count; i++) {
        if (!ata_wait_bsy() || !ata_wait_drq()) {
            ata_lock_release();
            return false;
        }
        
        for (int j = 0; j < 256; j++) {
            uint16_t w = inw(ATA_PORT_DATA);
            buffer[j * 2] = (uint8_t)w;
            buffer[j * 2 + 1] = (uint8_t)(w >> 8);
        }
        buffer += ATA_SECTOR_SIZE;
    }
    
    ata_lock_release();
    return true;
}

bool ata_write_sectors(uint32_t lba, uint8_t sector_count, const uint8_t *buffer) {
    if (!ata_present) return false;
    ata_lock_acquire();
    
    if (!ata_wait_bsy()) {
        ata_lock_release();
        return false;
    }
    
    outb(ATA_PORT_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PORT_SECTOR_COUNT, sector_count);
    outb(ATA_PORT_LBA_LO, (uint8_t)lba);
    outb(ATA_PORT_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PORT_LBA_HI, (uint8_t)(lba >> 16));
    
    outb(ATA_PORT_COMMAND, ATA_CMD_WRITE_PIO);
    
    for (int i = 0; i < sector_count; i++) {
        if (!ata_wait_bsy() || !ata_wait_drq()) {
            ata_lock_release();
            return false;
        }
        
        for (int j = 0; j < 256; j++) {
            uint16_t w = buffer[j * 2] | (buffer[j * 2 + 1] << 8);
            outw(ATA_PORT_DATA, w);
        }
        buffer += ATA_SECTOR_SIZE;
    }
    
    /* Flush cache */
    outb(ATA_PORT_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();
    
    ata_lock_release();
    return true;
}
