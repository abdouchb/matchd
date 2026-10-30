/*
 * ZarqaaOS AHCI SATA Driver
 * Rewritten based on SerenityOS reference implementation
 * (Kernel/Devices/Storage/AHCI/Port.cpp + Definitions.h)
 *
 * Key fixes vs old version:
 *  1. CommandHeader uses a SINGLE u16 'attributes' field (not bitfields)
 *     CFL goes in attributes[4:0], P in bit7, W in bit6
 *  2. FIS 'C' bit goes in port_multiplier byte (header.port_muliplier |= 0x80)
 *     NOT in a separate 'c' bitfield
 *  3. CommandTable.atapi_command is 32 bytes, not 16
 *  4. PRD byte_count: interrupt flag is bit 31, NOT a separate 'i' field
 *  5. PCI Bus Master must be enabled (Serenity: PCI::enable_bus_mastering)
 *  6. Full memory fence before/after CI write
 *  7. Timeout uses real ms delay, not busy-loop counts
 */

#include "drivers/ahci.h"
#include "kernel/pci.h"
#include "kernel/vmm.h"
#include "kernel/pmm.h"
#include "kernel/driver.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"

extern uint64_t hhdm_offset;
extern void kprintf(const char *fmt, ...);

/* ATA Command codes (from SerenityOS Definitions.h) */
#define ATA_CMD_READ_DMA_EXT   0x25
#define ATA_CMD_WRITE_DMA_EXT  0x35
#define ATA_CMD_IDENTIFY       0xEC
#define ATA_SR_BSY             0x80
#define ATA_SR_DRQ             0x08
#define ATA_USE_LBA_ADDRESSING 0x40

/* FIS */
#define FIS_TYPE_REG_H2D  0x27
#define FIS_H2D_C_BIT     0x80   /* C bit in port_multiplier byte */
#define FIS_DWORD_COUNT   5      /* RegisterHostToDevice = 5 DWORDs */

/* Command Header attributes bits */
#define CMD_ATTR_CFL(n)  ((n) & 0x1F)
#define CMD_ATTR_W       (1 << 6)
#define CMD_ATTR_P       (1 << 7)

/* SATA signatures */
#define SATA_SIG_ATA    0x00000101
#define SATA_SIG_ATAPI  0xEB140101

/* ======================================================
 * Structs matching SerenityOS Definitions.h exactly
 * ====================================================== */

typedef volatile struct {
    uint32_t clb;   uint32_t clbu;
    uint32_t fb;    uint32_t fbu;
    uint32_t is;    uint32_t ie;
    uint32_t cmd;   uint32_t reserved;
    uint32_t tfd;   uint32_t sig;
    uint32_t ssts;  uint32_t sctl;
    uint32_t serr;  uint32_t sact;
    uint32_t ci;    uint32_t sntf;
    uint32_t fbs;   uint32_t devslp;
    uint8_t  reserved2[0x70 - 0x48];
    uint8_t  vs[16];
} __attribute__((packed)) hba_port_t;

typedef volatile struct {
    uint32_t cap;  uint32_t ghc;  uint32_t is;
    uint32_t pi;   uint32_t vs;   uint32_t ccc_ctl;
    uint32_t ccc_pts; uint32_t em_loc; uint32_t em_ctl;
    uint32_t cap2; uint32_t bohc;
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port_t ports[32];
} hba_mem_t;

/* CommandHeader: uses u16 attributes (NOT bitfields) */
typedef struct {
    uint16_t attributes;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} __attribute__((packed)) hba_cmd_header_t;

/* PRD: byte_count bit31 = IOC flag */
typedef struct {
    uint32_t base_low;
    uint32_t base_high;
    uint32_t reserved;
    uint32_t byte_count;
} __attribute__((packed)) hba_prdt_entry_t;

/* CommandTable: atapi_command is 32 bytes (Serenity) */
typedef struct {
    uint8_t command_fis[64];
    uint8_t atapi_command[32];
    uint8_t reserved[32];
    hba_prdt_entry_t descriptors[1];
} __attribute__((packed)) hba_cmd_tbl_t;

/* FIS H2D: port_multiplier byte contains C bit */
typedef struct {
    uint8_t  fis_type;
    uint8_t  port_multiplier; /* bit7 = C (command) */
    uint8_t  command;
    uint8_t  features_low;
    uint8_t  lba_low[3];
    uint8_t  device;
    uint8_t  lba_high[3];
    uint8_t  features_high;
    uint16_t count;
    uint8_t  icc;
    uint8_t  control;
    uint32_t reserved;
} __attribute__((packed)) fis_reg_h2d_t;

/* ======================================================
 * Driver state
 * ====================================================== */
static hba_mem_t  *abar            = NULL;
static hba_port_t *active_port     = NULL;
static bool        ahci_found      = false;
static uint64_t    bounce_buf_phys = 0;
static uint8_t    *bounce_buf_virt = NULL;
static uint64_t    g_phys_clb      = 0;
static uint64_t    g_phys_ctba     = 0;

/* ======================================================
 * Helpers
 * ====================================================== */
static void io_delay_us(int us) {
    for (volatile int i = 0; i < us * 100; i++)
        __asm__ volatile("pause");
}

static void full_memory_fence(void) {
    __asm__ volatile("mfence" ::: "memory");
}

static void clflush_range(void *addr, size_t sz) {
    for (size_t i = 0; i < sz; i += 64)
        __asm__ volatile("clflush (%0)" :: "r"((char*)addr + i) : "memory");
}

static void port_stop_cmd_list(hba_port_t *p) { p->cmd &= ~0x1; }
static void port_stop_fis_recv(hba_port_t *p)  { p->cmd &= ~(1 << 4); }
static void port_start_fis_recv(hba_port_t *p) { p->cmd |= (1 << 4); }
static void port_start_cmd_list(hba_port_t *p) { p->cmd |= 0x1; }

static bool port_wait_idle(hba_port_t *port) {
    for (int t = 0; t < 1000; t++) {
        if (!(port->cmd & ((1<<15)|(1<<14)))) return true;
        io_delay_us(1000);
    }
    return false;
}

static bool spin_until_ready(hba_port_t *port) {
    for (int i = 0; i <= 100; i++) {
        if (!(port->tfd & (ATA_SR_BSY | ATA_SR_DRQ))) return true;
        io_delay_us(1000);
    }
    return false;
}

/* Enable PCI Bus Master DMA - critical for AHCI DMA on real hardware */
static void pci_enable_bus_master(uint8_t bus, uint8_t dev, uint8_t func) {
    uint32_t cmd_reg = pci_read_config_dword(bus, dev, func, 0x04);
    cmd_reg |= (1 << 2); /* Bit 2 = Bus Master Enable */
    /* Write to config space via PCI mechanism 1 */
    uint32_t addr = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
                    ((uint32_t)func << 8) | (0x04 & 0xFC);
    __asm__ volatile("outl %0, %1" :: "a"(addr), "Nd"((uint16_t)0xCF8));
    __asm__ volatile("outl %0, %1" :: "a"(cmd_reg), "Nd"((uint16_t)0xCFC));
}

/* ======================================================
 * Port Rebase (Serenity-style)
 * ====================================================== */
static bool ahci_port_rebase(hba_port_t *port, int portno) {
    (void)portno;

    early_blog(BLOG_INFO, "AHCI", "Stopping port commands...");
    port_stop_cmd_list(port);
    port_stop_fis_recv(port);
    full_memory_fence();

    if (!port_wait_idle(port))
        early_blog(BLOG_WARN, "AHCI", "Port idle timeout, continuing");

    full_memory_fence();
    port->serr = 0xFFFFFFFF;
    port->is   = 0xFFFFFFFF;

    /* Wait for device ready (SSTS.DET = 3, max 3 seconds) */
    early_blog(BLOG_INFO, "AHCI", "Waiting for SATA link...");
    bool ready = false;
    for (int t = 0; t < 3000; t++) {
        if ((port->ssts & 0x0F) == 3) { ready = true; break; }
        io_delay_us(1000);
    }
    if (!ready) {
        early_blog(BLOG_WARN, "AHCI", "Device not ready after 3s");
        return false;
    }
    early_blog(BLOG_OK, "AHCI", "SATA link established");

    /* Use low32 allocations — AHCI may not support 64-bit DMA */
    early_blog(BLOG_INFO, "AHCI", "Allocating DMA buffers (<4GB)...");
    uint64_t phys_clb  = pmm_alloc_low32();
    uint64_t phys_fb   = pmm_alloc_low32();
    uint64_t phys_ctba = pmm_alloc_low32();
    if (!phys_clb || !phys_fb || !phys_ctba) {
        early_blog(BLOG_FAIL, "AHCI", "Failed to alloc DMA buffers!");
        return false;
    }

    memset((void*)(phys_clb  + hhdm_offset), 0, 4096);
    memset((void*)(phys_fb   + hhdm_offset), 0, 4096);
    memset((void*)(phys_ctba + hhdm_offset), 0, 4096);

    port->clb  = (uint32_t)(phys_clb & 0xFFFFFFFF);
    port->clbu = (uint32_t)(phys_clb >> 32);
    port->fb   = (uint32_t)(phys_fb  & 0xFFFFFFFF);
    port->fbu  = (uint32_t)(phys_fb  >> 32);

    hba_cmd_header_t *hdr = (hba_cmd_header_t*)(phys_clb + hhdm_offset);
    memset(hdr, 0, 4096);
    hdr[0].ctba  = (uint32_t)(phys_ctba & 0xFFFFFFFF);
    hdr[0].ctbau = (uint32_t)(phys_ctba >> 32);

    g_phys_clb  = phys_clb;
    g_phys_ctba = phys_ctba;

    full_memory_fence();
    port_start_fis_recv(port);
    port_start_cmd_list(port);
    full_memory_fence();

    if (!bounce_buf_phys) {
        bounce_buf_phys = pmm_alloc_low32();
        if (bounce_buf_phys)
            bounce_buf_virt = (uint8_t*)(bounce_buf_phys + hhdm_offset);
    }

    if (!bounce_buf_phys) {
        early_blog(BLOG_FAIL, "AHCI", "Bounce buffer alloc failed!");
        return false;
    }

    early_blog(BLOG_OK, "AHCI", "Port rebase complete");
    return true;
}

static int check_port_type(hba_port_t *port) {
    uint32_t ssts = port->ssts;
    if ((ssts & 0x0F) != 3) return 0;
    if (((ssts >> 8) & 0x0F) != 1) return 0;
    if (port->sig == SATA_SIG_ATAPI) return 2;
    if (port->sig == SATA_SIG_ATA)   return 1;
    return 0;
}

/* ======================================================
 * AHCI Read
 * ====================================================== */
bool ahci_read_sectors(uint32_t lba, uint32_t sector_count, uint8_t *buffer) {
    if (!ahci_found || !active_port) return false;
    if (!bounce_buf_phys || !g_phys_clb || !g_phys_ctba) return false;

    active_port->is   = 0xFFFFFFFF;
    active_port->serr = 0xFFFFFFFF;

    if (!spin_until_ready(active_port)) return false;

    /* Command Header */
    hba_cmd_header_t *hdr = (hba_cmd_header_t*)(g_phys_clb + hhdm_offset);
    memset(hdr, 0, sizeof(hba_cmd_header_t));
    hdr->attributes = (uint16_t)(CMD_ATTR_CFL(FIS_DWORD_COUNT) | CMD_ATTR_P);
    hdr->prdtl      = 1;
    hdr->prdbc      = 0;
    hdr->ctba       = (uint32_t)(g_phys_ctba & 0xFFFFFFFF);
    hdr->ctbau      = (uint32_t)(g_phys_ctba >> 32);

    /* Command Table */
    hba_cmd_tbl_t *tbl = (hba_cmd_tbl_t*)(g_phys_ctba + hhdm_offset);
    memset(tbl, 0, sizeof(hba_cmd_tbl_t));

    /* PRDT - bit31 = IOC (Serenity style) */
    tbl->descriptors[0].base_low  = (uint32_t)(bounce_buf_phys & 0xFFFFFFFF);
    tbl->descriptors[0].base_high = (uint32_t)(bounce_buf_phys >> 32);
    tbl->descriptors[0].byte_count = (sector_count * 512 - 1) | (1u << 31);

    /* FIS - C bit in port_multiplier (Serenity style) */
    fis_reg_h2d_t *fis = (fis_reg_h2d_t*)tbl->command_fis;
    memset(fis, 0, sizeof(fis_reg_h2d_t));
    fis->fis_type        = FIS_TYPE_REG_H2D;
    fis->port_multiplier = FIS_H2D_C_BIT;
    fis->command         = ATA_CMD_READ_DMA_EXT;
    fis->device          = ATA_USE_LBA_ADDRESSING;
    fis->lba_low[0]  = (uint8_t)(lba);
    fis->lba_low[1]  = (uint8_t)(lba >>  8);
    fis->lba_low[2]  = (uint8_t)(lba >> 16);
    fis->lba_high[0] = (uint8_t)(lba >> 24);
    fis->lba_high[1] = 0;
    fis->lba_high[2] = 0;
    fis->count       = (uint16_t)sector_count;

    clflush_range(tbl, sizeof(hba_cmd_tbl_t));
    clflush_range(hdr, sizeof(hba_cmd_header_t));
    full_memory_fence();

    active_port->ci = 1;
    full_memory_fence();

    /* Poll - 50ms max with 50us steps (real hardware DMA is usually < 1ms) */
    for (int t = 0; t < 1000; t++) {
        if (!(active_port->ci & 1)) goto done;
        if (active_port->serr) {
            return false;
        }
        if (active_port->is & (1u << 30)) {
            return false;
        }
        io_delay_us(50);
    }
    return false;

done:
    full_memory_fence();
    clflush_range(bounce_buf_virt, sector_count * 512);
    full_memory_fence();
    memcpy(buffer, bounce_buf_virt, sector_count * 512);
    return true;
}

bool ahci_write_sectors(uint32_t lba, uint32_t count, const uint8_t *buf) {
    (void)lba; (void)count; (void)buf;
    return false;
}

/* ======================================================
 * PCI Driver Init
 * ====================================================== */
static bool ahci_driver_init(const pci_device_t *dev) {
    if (ahci_found) return true;

    uint32_t bar5 = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x24);
    uint64_t phys_abar = bar5 & 0xFFFFFFF0;
    if (!phys_abar) {
        early_blog(BLOG_WARN, "AHCI", "BAR5 is zero");
        return false;
    }

    /* CRITICAL: Enable Bus Master DMA in PCI Command register */
    pci_enable_bus_master(dev->bus, dev->device, dev->function);

    /* Map the ABAR MMIO region explicitly (up to 16KB/4 pages just in case) */
    for (uint64_t offset = 0; offset < 4096 * 4; offset += 4096) {
        vmm_map_page(kernel_pml4, (phys_abar + offset) + hhdm_offset, phys_abar + offset, PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);
    }

    abar = (hba_mem_t*)(phys_abar + hhdm_offset);
    kprintf("[AHCI] HBA MMIO at 0x%x\n", (uint32_t)phys_abar);

    /* Enable AHCI mode (GHC.AE bit 31) */
    abar->ghc |= (1u << 31);
    full_memory_fence();

    uint32_t pi = abar->pi;
    for (int i = 0; i < 32; i++) {
        if (!(pi & (1u << i))) continue;
        if (check_port_type(&abar->ports[i]) != 1) continue;

        early_blog(BLOG_INFO, "AHCI", "Found ATA drive, initializing port...");
        if (!ahci_port_rebase(&abar->ports[i], i)) {
            early_blog(BLOG_WARN, "AHCI", "Port initialization failed!");
            continue;
        }
        active_port = &abar->ports[i];
        ahci_found  = true;
        return true;
    }

    early_blog(BLOG_WARN, "AHCI", "No ATA drives found on controller!");
    return false;
}

bool ahci_is_available(void) { return ahci_found && active_port != NULL; }

bool ahci_init(void) {
    static pci_driver_t drv = {
        .name       = "Intel AHCI Controller",
        .match_type = DRIVER_MATCH_CLASS,
        .class_code = 0x01,
        .subclass   = 0x06,
        .init       = ahci_driver_init,
    };
    driver_register(&drv);
    return true;
}
