#include "drivers/ac97.h"
#include "kernel/pci.h"
#include "kernel/pmm.h"
#include "drivers/pit.h"
#include "zarqaa/string.h"

// AC97 PCI Vendor and Device IDs
#define AC97_VENDOR_ID 0x8086
#define AC97_DEVICE_ID 0x2415 // Intel 82801AA AC'97 Audio

// AC97 NAM (Native Audio Mixer) Registers
#define AC97_RESET         0x00
#define AC97_MASTER_VOL    0x02 // Master Volume
#define AC97_PCM_OUT_VOL   0x18 // PCM Out Volume
#define AC97_EXT_AUDIO_CTRL 0x2A // Extended Audio Control
#define AC97_PCM_OUT_RATE  0x2C // PCM Front DAC Rate
#define AC97_POWER_DOWN    0x26 // Powerdown Control/Status

// AC97 NABM (Native Audio Bus Mastering) Registers
#define AC97_PO_BDBAR      0x10 // PCM Out Buffer Descriptor List Base Address
#define AC97_PO_CIV        0x14 // PCM Out Current Index Value
#define AC97_PO_LVI        0x15 // PCM Out Last Valid Index
#define AC97_PO_SR         0x16 // PCM Out Status Register
#define AC97_PO_PICB       0x18 // PCM Out Position In Current Buffer
#define AC97_PO_PIV        0x1A // PCM Out Prefetched Index Value
#define AC97_PO_CR         0x1B // PCM Out Control Register
#define AC97_GLOB_CTRL     0x2C // Global Control
#define AC97_GLOB_STA      0x30 // Global Status

#define AC97_CR_RUN_PAUSE  (1 << 0)
#define AC97_CR_RESET      (1 << 1)
#define AC97_CR_IOCE       (1 << 4)

struct ac97_bdl_entry {
    uint32_t buffer_addr; // Physical address
    uint16_t length;      // Number of samples
    uint16_t flags;       // Bit 15: IOC
} __attribute__((packed));

static uint32_t nam_base = 0;
static uint32_t nabm_base = 0;
static struct ac97_bdl_entry *po_bdl;
static uint64_t po_bdl_phys = 0;

static uint64_t ring_buffer_phys = 0;
static uint8_t *ring_buffer_virt = NULL;

extern uint64_t pmm_alloc_blocks(uint64_t count);
extern uint64_t hhdm_offset;

static void delay_ms(int ms) {
    // Basic busy wait since we might be initializing early
    for(volatile int i = 0; i < ms * 10000; i++) {
        __asm__ __volatile__ ("pause");
    }
}

void ac97_init(void) {
    const pci_device_t *ac97_dev = pci_find_device(AC97_VENDOR_ID, AC97_DEVICE_ID);
    
    if (!ac97_dev) {
        kprintf("[AC97] No AC97 Audio Controller found.\n");
        return;
    }
    
    kprintf("[AC97] Found Intel AC97 Audio at %d:%d:%d\n", ac97_dev->bus, ac97_dev->device, ac97_dev->function);
    
    // Enable Bus Mastering and IO Space
    uint16_t command = pci_read_config_word(ac97_dev->bus, ac97_dev->device, ac97_dev->function, 0x04);
    command |= (1 << 2) | (1 << 0);
    pci_write_config_word(ac97_dev->bus, ac97_dev->device, ac97_dev->function, 0x04, command);
    
    // Get BARs
    nam_base = pci_read_config_dword(ac97_dev->bus, ac97_dev->device, ac97_dev->function, 0x10) & ~1;
    nabm_base = pci_read_config_dword(ac97_dev->bus, ac97_dev->device, ac97_dev->function, 0x14) & ~1;
    
    if (!nam_base || !nabm_base) {
        kprintf("[AC97] Invalid BARs: NAM=%x NABM=%x\n", nam_base, nabm_base);
        return;
    }
    
    kprintf("[AC97] NAM=%x, NABM=%x\n", nam_base, nabm_base);
    
    // 1. Reset Codec
    outw(nam_base + AC97_RESET, 1);
    delay_ms(10);
    
    // 1.5. Power up all DACs (write 0x0000 to Powerdown register)
    outw(nam_base + AC97_POWER_DOWN, 0x0000);
    
    // 2. Set Master and PCM out volume to max (0x0000 = max volume)
    outw(nam_base + AC97_MASTER_VOL, 0x0000);
    outw(nam_base + AC97_PCM_OUT_VOL, 0x0000);
    
    // Enable Variable Rate Audio (VRA)
    uint16_t ext_ctrl = inw(nam_base + AC97_EXT_AUDIO_CTRL);
    ext_ctrl |= 1; // VRA bit
    outw(nam_base + AC97_EXT_AUDIO_CTRL, ext_ctrl);
    
    // Set sample rate to 48000Hz (Native AC97 Rate and matches our WAV)
    outw(nam_base + AC97_PCM_OUT_RATE, 48000);
    
    // Reset Bus Master control
    outb(nabm_base + AC97_PO_CR, AC97_CR_RESET);
    delay_ms(10);
    
    // Allocate physical memory for BDL (1 page)
    po_bdl_phys = pmm_alloc();
    if (!po_bdl_phys) {
        kprintf("[AC97] Failed to allocate BDL\n");
        return;
    }
    po_bdl = (struct ac97_bdl_entry *)(po_bdl_phys + hhdm_offset);
    memset(po_bdl, 0, 4096);
    
    // Allocate Ring Buffer (32 pages = 128KB)
    ring_buffer_phys = pmm_alloc_blocks(32);
    if (!ring_buffer_phys) {
        kprintf("[AC97] Failed to allocate Ring Buffer\n");
        return;
    }
    ring_buffer_virt = (uint8_t *)(ring_buffer_phys + hhdm_offset);
    memset(ring_buffer_virt, 0, 32 * 4096);
    // Fill BDL entries
    for (int i = 0; i < 32; i++) {
        po_bdl[i].buffer_addr = (uint32_t)(ring_buffer_phys + (i * 4096));
        po_bdl[i].length = 2048; // Length is in 16-bit words! 4096 bytes = 2048 words
        po_bdl[i].flags = 0x8000; // IOC flag (Interrupt on Completion)
    }
    
    // Set BDL Base
    outl(nabm_base + AC97_PO_BDBAR, (uint32_t)po_bdl_phys);
    
    kprintf("[AC97] Initialization complete. Ring Buffer at %x\n", ring_buffer_phys);
}

void ac97_set_volume(uint8_t volume) {
    if (!nam_base) return;
    // AC97 volume goes from 0 (max) to 0x3F (min)
    if (volume > 100) volume = 100;
    uint8_t hw_vol = 0x3F - ((volume * 0x3F) / 100);
    uint16_t vol_val = (hw_vol << 8) | hw_vol;
    outw(nam_base + AC97_MASTER_VOL, vol_val);
    outw(nam_base + AC97_PCM_OUT_VOL, vol_val);
}

uint32_t ac97_get_ring_buffer_phys(void) {
    return (uint32_t)ring_buffer_phys;
}

uint8_t *ac97_get_ring_buffer_virt(void) {
    return ring_buffer_virt;
}

void ac97_start_ring(void) {
    if (!nabm_base || !ring_buffer_phys) return;
    
    // Stop current playback
    outb(nabm_base + AC97_PO_CR, 0);
    
    // Reset registers (CIV to 0, this also resets BDBAR!)
    outb(nabm_base + AC97_PO_CR, 2); // AC97_CR_RESET
    delay_ms(1);
    
    // Clear reset
    outb(nabm_base + AC97_PO_CR, 0);
    
    // Restore BDBAR because CR_RESET zeroed it!
    outl(nabm_base + 0x10, po_bdl_phys); // 0x10 is AC97_PO_BDBAR
    
    // Start playback (Run bit = 1, IOCE = 0 to avoid unhandled interrupts)
    outb(nabm_base + AC97_PO_CR, 1); // AC97_CR_RUN_PAUSE
}

void ac97_set_lvi(uint8_t lvi) {
    if (!nabm_base) return;
    outb(nabm_base + AC97_PO_LVI, lvi & 0x1F);
}

uint8_t ac97_get_civ(void) {
    if (!nabm_base) return 0;
    return inb(nabm_base + AC97_PO_CIV) & 0x1F;
}

void ac97_stop(void) {
    if (!nabm_base) return;
    outb(nabm_base + AC97_PO_CR, 0);
}
