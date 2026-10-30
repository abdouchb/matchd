#include "drivers/rtl8169.h"
#include "kernel/pci.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/heap.h"
#include "kernel/driver.h"
#include "kernel/syslog.h"
#include "zarqaa/string.h"

extern void kprintf(const char *fmt, ...);

#define RTL8169_MAC0          0x00
#define RTL8169_TX_DESC_LOW   0x20
#define RTL8169_TX_DESC_HIGH  0x24
#define RTL8169_COMMAND       0x37
#define RTL8169_TPPOLL        0x38
#define RTL8169_IMR           0x3C
#define RTL8169_ISR           0x3E
#define RTL8169_TCR           0x40
#define RTL8169_RCR           0x44
#define RTL8169_9346CR        0x50
#define RTL8169_RX_DESC_LOW   0xE4
#define RTL8169_RX_DESC_HIGH  0xE8

/* Descriptor Flags */
#define DESC_OWN  0x80000000
#define DESC_EOR  0x40000000
#define DESC_FS   0x20000000
#define DESC_LS   0x10000000

#define NUM_TX_DESC 256
#define NUM_RX_DESC 256
#define RX_BUF_SIZE 2048

struct rtl8169_desc {
    uint32_t cmd_len;
    uint32_t vlan;
    uint32_t buf_low;
    uint32_t buf_high;
} __attribute__((packed));

static volatile uint8_t *mmio_base = NULL;
bool rtl8169_found = false;
uint8_t rtl8169_mac[6];
static rtl8169_rx_callback_t rx_cb = NULL;

static struct rtl8169_desc *tx_ring;
static struct rtl8169_desc *rx_ring;
static uint64_t tx_ring_phys;
static uint64_t rx_ring_phys;

static uint8_t *tx_bufs;
static uint8_t *rx_bufs;
static uint64_t tx_bufs_phys;
static uint64_t rx_bufs_phys;

static uint16_t tx_cur = 0;
static uint16_t rx_cur = 0;

static inline uint8_t  mmio_read8 (uint32_t off) { return *(volatile uint8_t  *)(mmio_base + off); }
static inline uint16_t mmio_read16(uint32_t off) { return *(volatile uint16_t *)(mmio_base + off); }
static inline uint32_t mmio_read32(uint32_t off) { return *(volatile uint32_t *)(mmio_base + off); }
static inline void mmio_write8 (uint32_t off, uint8_t  v) { *(volatile uint8_t  *)(mmio_base + off) = v; }
static inline void mmio_write16(uint32_t off, uint16_t v) { *(volatile uint16_t *)(mmio_base + off) = v; }
static inline void mmio_write32(uint32_t off, uint32_t v) { *(volatile uint32_t *)(mmio_base + off) = v; }

void rtl8169_set_rx_callback(rtl8169_rx_callback_t cb) {
    rx_cb = cb;
}

static bool rtl8169_driver_init(const pci_device_t *dev) {
    if (!dev) return false;

    // Enable bus mastering & Memory space
    uint16_t cmd = pci_read_config_word(dev->bus, dev->device, dev->function, 0x04);
    cmd |= (1 << 2) | (1 << 1); 
    pci_write_config_word(dev->bus, dev->device, dev->function, 0x04, cmd);

    // Find MMIO BAR
    uint64_t phys_base = 0;
    for (int i = 0; i < 6; i++) {
        uint32_t bar = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x10 + (i * 4));
        if ((bar & 0x01) == 0 && bar != 0) { // Memory space
            if (bar & 0x04) { // 64-bit BAR
                uint32_t bar_high = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x14 + (i * 4));
                phys_base = ((uint64_t)bar_high << 32) | (bar & ~0xFULL);
                break;
            } else {
                phys_base = bar & ~0xFULL;
                break;
            }
        }
    }

    if (!phys_base) {
        kprintf("[RTL8169] MMIO BAR not found!\n");
        return false;
    }

    // Map MMIO
    extern uint64_t hhdm_offset;
    mmio_base = (volatile uint8_t *)(phys_base + hhdm_offset);

    kprintf("[RTL8169] MMIO base mapped at 0x%x\n", (uint32_t)(phys_base + hhdm_offset));

    // Read MAC Address
    uint32_t mac1 = mmio_read32(RTL8169_MAC0);
    uint32_t mac2 = mmio_read32(RTL8169_MAC0 + 4);
    
    rtl8169_mac[0] = mac1 & 0xFF;
    rtl8169_mac[1] = (mac1 >> 8) & 0xFF;
    rtl8169_mac[2] = (mac1 >> 16) & 0xFF;
    rtl8169_mac[3] = (mac1 >> 24) & 0xFF;
    rtl8169_mac[4] = mac2 & 0xFF;
    rtl8169_mac[5] = (mac2 >> 8) & 0xFF;

    kprintf("[RTL8169] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
            rtl8169_mac[0], rtl8169_mac[1], rtl8169_mac[2], 
            rtl8169_mac[3], rtl8169_mac[4], rtl8169_mac[5]);

    // Reset Controller
    mmio_write8(RTL8169_COMMAND, 0x10);
    int timeout = 10000;
    while ((mmio_read8(RTL8169_COMMAND) & 0x10) && timeout > 0) {
        // Wait for reset
        for (volatile int i = 0; i < 1000; i++) __asm__ volatile("pause");
        timeout--;
    }
    
    if (timeout == 0) {
        kprintf("[RTL8169] WARNING: Reset timed out!\n");
    }

    // Unlock Config Registers
    mmio_write8(RTL8169_9346CR, 0xC0);

    extern uint64_t hhdm_offset;

    // Allocate Descriptor Rings (must be 256-byte aligned, 1 page is plenty)
    tx_ring_phys = pmm_alloc();
    rx_ring_phys = pmm_alloc();
    tx_ring = (struct rtl8169_desc *)(tx_ring_phys + hhdm_offset);
    rx_ring = (struct rtl8169_desc *)(rx_ring_phys + hhdm_offset);
    memset(tx_ring, 0, 4096);
    memset(rx_ring, 0, 4096);

    // Allocate Buffers
    tx_bufs_phys = pmm_alloc_blocks( (NUM_TX_DESC * RX_BUF_SIZE) / 4096 + 1 );
    rx_bufs_phys = pmm_alloc_blocks( (NUM_RX_DESC * RX_BUF_SIZE) / 4096 + 1 );
    tx_bufs = (uint8_t *)(tx_bufs_phys + hhdm_offset);
    rx_bufs = (uint8_t *)(rx_bufs_phys + hhdm_offset);

    // Initialize RX Ring
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_ring[i].buf_low = (uint32_t)((rx_bufs_phys + (i * RX_BUF_SIZE)) & 0xFFFFFFFF);
        rx_ring[i].buf_high = (uint32_t)((rx_bufs_phys + (i * RX_BUF_SIZE)) >> 32);
        rx_ring[i].cmd_len = DESC_OWN | RX_BUF_SIZE; // Owned by NIC, max 2048 bytes
        if (i == NUM_RX_DESC - 1) {
            rx_ring[i].cmd_len |= DESC_EOR; // End of Ring
        }
    }

    // Initialize TX Ring
    for (int i = 0; i < NUM_TX_DESC; i++) {
        tx_ring[i].buf_low = (uint32_t)((tx_bufs_phys + (i * RX_BUF_SIZE)) & 0xFFFFFFFF);
        tx_ring[i].buf_high = (uint32_t)((tx_bufs_phys + (i * RX_BUF_SIZE)) >> 32);
        tx_ring[i].cmd_len = 0; 
        if (i == NUM_TX_DESC - 1) {
            tx_ring[i].cmd_len |= DESC_EOR; // End of Ring
        }
    }

    // Write Descriptor Addresses
    mmio_write32(RTL8169_TX_DESC_HIGH, (uint32_t)(tx_ring_phys >> 32));
    mmio_write32(RTL8169_TX_DESC_LOW, (uint32_t)(tx_ring_phys & 0xFFFFFFFF));
    mmio_write32(RTL8169_RX_DESC_HIGH, (uint32_t)(rx_ring_phys >> 32));
    mmio_write32(RTL8169_RX_DESC_LOW, (uint32_t)(rx_ring_phys & 0xFFFFFFFF));

    // Configure RX/TX
    mmio_write32(RTL8169_RCR, 0x0000E70F); // Accept all (Promisc, Broadcast, Multicast, MyMAC)
    mmio_write32(RTL8169_TCR, 0x03000700); // IFG, Max DMA Burst

    // Lock Config Registers
    mmio_write8(RTL8169_9346CR, 0x00);

    // Enable RX and TX
    mmio_write8(RTL8169_COMMAND, 0x0C); 

    rtl8169_found = true;
    kprintf("[RTL8169] Initialization complete.\n");

    return true;
}

static pci_driver_t rtl8169_driver = {
    .name = "Realtek RTL8169/8136 Gigabit Ethernet",
    .match_type = DRIVER_MATCH_EXACT,
    .vendor_id = 0, // Handled in rtl8169_init manual register
    .device_id = 0,
    .init = rtl8169_driver_init
};

void rtl8169_init(void) {
    // Register for multiple devices
    static pci_driver_t dev1, dev2, dev3;
    
    dev1 = rtl8169_driver;
    dev1.vendor_id = RTL8169_VENDOR_ID;
    dev1.device_id = RTL8169_DEVICE_ID;
    driver_register(&dev1);

    dev2 = rtl8169_driver;
    dev2.vendor_id = RTL8169_VENDOR_ID;
    dev2.device_id = RTL8168_DEVICE_ID;
    driver_register(&dev2);

    dev3 = rtl8169_driver;
    dev3.vendor_id = RTL8169_VENDOR_ID;
    dev3.device_id = RTL8136_DEVICE_ID;
    driver_register(&dev3);
}

void rtl8169_send_packet(void *data, uint32_t len) {
    if (!rtl8169_found) return;

    if (tx_ring[tx_cur].cmd_len & DESC_OWN) {
        kprintf("[RTL8169] TX Ring full!\n");
        return; // TX Ring full
    }

    uint8_t *tx_buf = tx_bufs + (tx_cur * RX_BUF_SIZE);
    memcpy(tx_buf, data, len);

    // Pad short packets
    if (len < 60) len = 60;

    __asm__ volatile("sfence" ::: "memory");

    uint32_t cmd = DESC_OWN | DESC_FS | DESC_LS | len;
    if (tx_cur == NUM_TX_DESC - 1) {
        cmd |= DESC_EOR;
    }

    tx_ring[tx_cur].cmd_len = cmd;

    __asm__ volatile("sfence" ::: "memory");

    // Tell NIC to check TX descriptors
    mmio_write8(RTL8169_TPPOLL, 0x40); // NPQ

    tx_cur = (tx_cur + 1) % NUM_TX_DESC;
}

void rtl8169_poll(void) {
    if (!rtl8169_found) return;

    // Acknowledge interrupts
    uint16_t isr = mmio_read16(RTL8169_ISR);
    if (isr) {
        mmio_write16(RTL8169_ISR, isr);
    }

    while (!(rx_ring[rx_cur].cmd_len & DESC_OWN)) {
        uint32_t len = rx_ring[rx_cur].cmd_len & 0x3FFF;
        uint8_t *rx_buf = rx_bufs + (rx_cur * RX_BUF_SIZE);

        if (rx_cb && len > 4) {
            rx_cb(rx_buf, len - 4); // Strip CRC
        }

        // Give descriptor back to NIC
        uint32_t cmd = DESC_OWN | RX_BUF_SIZE;
        if (rx_cur == NUM_RX_DESC - 1) {
            cmd |= DESC_EOR;
        }
        rx_ring[rx_cur].cmd_len = cmd;
        
        __asm__ volatile("sfence" ::: "memory");

        rx_cur = (rx_cur + 1) % NUM_RX_DESC;
    }
}
