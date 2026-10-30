#include "drivers/rtl8168.h"
#include "kernel/pci.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"
#include "kernel/early_boot_log.h"

#define NUM_TX_DESC 128
#define NUM_RX_DESC 128
#define RX_BUF_SIZE 1536

extern uint64_t hhdm_offset;
extern void kprintf(const char *fmt, ...);

struct rtl8168_desc {
    uint32_t opts1;
    uint32_t opts2;
    uint64_t addr;
} __attribute__((packed));

static uint64_t mmio_base_phys;
static uint8_t *mmio_base;
static struct rtl8168_desc *tx_desc;
static struct rtl8168_desc *rx_desc;
static uint8_t *rx_buffers[NUM_RX_DESC];
static uint8_t *tx_buffers[NUM_TX_DESC];

static uint16_t tx_cur = 0;
static uint16_t rx_cur = 0;

uint8_t rtl8168_mac[6];
bool rtl8168_is_found = false;
static rtl8168_rx_callback_t rx_cb = NULL;

void rtl8168_set_rx_callback(rtl8168_rx_callback_t cb) {
    rx_cb = cb;
}

static uint8_t mmio_read8(uint32_t reg) { return *(volatile uint8_t *)(mmio_base + reg); }
static uint16_t mmio_read16(uint32_t reg) { return *(volatile uint16_t *)(mmio_base + reg); }
static uint32_t mmio_read32(uint32_t reg) { return *(volatile uint32_t *)(mmio_base + reg); }
static void mmio_write8(uint32_t reg, uint8_t val) { *(volatile uint8_t *)(mmio_base + reg) = val; }
static void mmio_write16(uint32_t reg, uint16_t val) { *(volatile uint16_t *)(mmio_base + reg) = val; }
static void mmio_write32(uint32_t reg, uint32_t val) { *(volatile uint32_t *)(mmio_base + reg) = val; }

bool rtl8168_init(void) {
    const pci_device_t *dev = pci_find_device(RTL8168_VENDOR_ID, RTL8168_DEVICE_ID);
    if (!dev) dev = pci_find_device(RTL8168_VENDOR_ID, RTL8136_DEVICE_ID);
    
    if (!dev) {
        early_blog(BLOG_WARN, "RTL8168", "Device not found.");
        return false;
    }
    
    rtl8168_is_found = true;
    early_blog(BLOG_INFO, "RTL8168", "Found RTL8168/810xE Network Controller");

    uint16_t command = pci_read_config_word(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 2) | (1 << 1); 
    pci_write_config_word(dev->bus, dev->device, dev->function, 0x04, command);

    uint32_t bar2 = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x18);
    mmio_base_phys = bar2 & 0xFFFFFFF0;
    
    if (mmio_base_phys == 0) {
        uint32_t bar0 = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x10);
        if ((bar0 & 1) == 0) {
            mmio_base_phys = bar0 & 0xFFFFFFF0;
        }
    }
    
    if (mmio_base_phys == 0) {
        early_blog(BLOG_FAIL, "RTL8168", "Invalid MMIO base.");
        return false;
    }

    mmio_base = (uint8_t *)(mmio_base_phys + hhdm_offset);
    
    for (uint64_t offset = 0; offset < 4096 * 4; offset += 4096) {
        vmm_map_page(kernel_pml4, (mmio_base_phys + offset) + hhdm_offset, mmio_base_phys + offset, PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);
    }

    mmio_write8(0x37, 0x10);
    for (int i = 0; i < 10000; i++) {
        if ((mmio_read8(0x37) & 0x10) == 0) break;
    }

    uint32_t mac1 = mmio_read32(0x00);
    uint16_t mac2 = mmio_read16(0x04);
    rtl8168_mac[0] = mac1 & 0xFF; rtl8168_mac[1] = (mac1 >> 8) & 0xFF;
    rtl8168_mac[2] = (mac1 >> 16) & 0xFF; rtl8168_mac[3] = (mac1 >> 24) & 0xFF;
    rtl8168_mac[4] = mac2 & 0xFF; rtl8168_mac[5] = (mac2 >> 8) & 0xFF;
    
    kprintf("[RTL8168] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
            rtl8168_mac[0], rtl8168_mac[1], rtl8168_mac[2], 
            rtl8168_mac[3], rtl8168_mac[4], rtl8168_mac[5]);

    uint64_t tx_desc_phys = pmm_alloc_low32();
    uint64_t rx_desc_phys = pmm_alloc_low32();
    tx_desc = (struct rtl8168_desc *)(tx_desc_phys + hhdm_offset);
    rx_desc = (struct rtl8168_desc *)(rx_desc_phys + hhdm_offset);
    
    memset(tx_desc, 0, 4096);
    memset(rx_desc, 0, 4096);

    for (int i = 0; i < NUM_RX_DESC; i++) {
        uint64_t buf_phys = pmm_alloc_low32();
        rx_buffers[i] = (uint8_t *)(buf_phys + hhdm_offset);
        rx_desc[i].addr = buf_phys;
        rx_desc[i].opts2 = 0;
        uint32_t opts = RX_BUF_SIZE;
        if (i == NUM_RX_DESC - 1) opts |= (1 << 30);
        opts |= (1u << 31);
        rx_desc[i].opts1 = opts;
    }
    
    for (int i = 0; i < NUM_TX_DESC; i++) {
        uint64_t buf_phys = pmm_alloc_low32();
        tx_buffers[i] = (uint8_t *)(buf_phys + hhdm_offset);
        tx_desc[i].addr = buf_phys;
    }

    mmio_write8(0x50, 0xC0);
    
    mmio_write32(0x20, (uint32_t)(tx_desc_phys & 0xFFFFFFFF));
    mmio_write32(0x24, (uint32_t)(tx_desc_phys >> 32));
    mmio_write32(0xE4, (uint32_t)(rx_desc_phys & 0xFFFFFFFF));
    mmio_write32(0xE8, (uint32_t)(rx_desc_phys >> 32));
    
    mmio_write16(0xDA, RX_BUF_SIZE);
    
    mmio_write32(0x44, 0x0F | (1 << 7) | (7 << 8)); 
    
    mmio_write32(0x40, (3 << 24));
    
    mmio_write8(0x37, 0x0C); 
    
    mmio_write8(0x50, 0x00);
    
    mmio_write16(0x3E, 0xFFFF);
    mmio_write16(0x3C, 0x803F);

    early_blog(BLOG_OK, "RTL8168", "Driver init complete");
    return true;
}

void rtl8168_send_packet(void *data, uint32_t len) {
    if (!rtl8168_is_found) return;
    
    if (len > 1514) len = 1514;
    
    memcpy(tx_buffers[tx_cur], data, len);
    
    uint32_t opts = len;
    opts |= (1 << 28);
    opts |= (1 << 29);
    if (tx_cur == NUM_TX_DESC - 1) opts |= (1 << 30);
    opts |= (1u << 31);
    
    tx_desc[tx_cur].opts1 = opts;
    
    mmio_write8(0x38, 0x40);
    
    tx_cur = (tx_cur + 1) % NUM_TX_DESC;
}

void rtl8168_poll(void) {
    if (!rtl8168_is_found) return;
    
    uint16_t status = mmio_read16(0x3E);
    if (status) {
        mmio_write16(0x3E, status);
        
        if (status & 0x01) {
            int max_packets = 64;
            while (max_packets--) {
                uint32_t opts = rx_desc[rx_cur].opts1;
                if (opts & (1u << 31)) break;
                
                uint32_t len = opts & 0x3FFF;
                if (len >= 4) {
                    if (rx_cb) rx_cb(rx_buffers[rx_cur], len - 4);
                }
                
                uint32_t new_opts = RX_BUF_SIZE;
                if (rx_cur == NUM_RX_DESC - 1) new_opts |= (1 << 30);
                new_opts |= (1u << 31);
                rx_desc[rx_cur].opts1 = new_opts;
                
                rx_cur = (rx_cur + 1) % NUM_RX_DESC;
            }
        }
    }
}
