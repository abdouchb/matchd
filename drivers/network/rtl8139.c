#include "drivers/rtl8139.h"
#include "kernel/pci.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

#define RTL8139_MAC0 0x00
#define RTL8139_MAR0 0x08
#define RTL8139_TX_STATUS0 0x10
#define RTL8139_TX_ADDR0 0x20
#define RTL8139_RX_BUF 0x30
#define RTL8139_COMMAND 0x37
#define RTL8139_CAPR 0x38
#define RTL8139_CBR 0x3A
#define RTL8139_INTR_MASK 0x3C
#define RTL8139_INTR_STATUS 0x3E
#define RTL8139_TX_CONFIG 0x40
#define RTL8139_RX_CONFIG 0x44
#define RTL8139_CONFIG1 0x52

#define RX_BUF_SIZE 8192
#define RX_BUF_PAD 16
#define RX_BUF_WRAP_PAD 1500
#define RX_BUF_TOTAL (RX_BUF_SIZE + RX_BUF_PAD + RX_BUF_WRAP_PAD)

static uint32_t io_base;
static uint8_t *rx_buffer;
static uint16_t current_rx_ptr;
static uint8_t tx_cur;
uint8_t rtl8139_mac[6];
static rtl8139_rx_callback_t rx_cb = NULL;
static bool rtl8139_found = false;

void rtl8139_set_rx_callback(rtl8139_rx_callback_t cb) {
    rx_cb = cb;
}

static uint32_t tx_phys_addr[4];
static uint8_t *tx_buffers[4];

void rtl8139_init(void) {
    const pci_device_t *dev = pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
    if (!dev) {
        kprintf("[RTL8139] Device not found.\n");
        return;
    }
    
    rtl8139_found = true;

    // Enable bus mastering
    uint16_t command = pci_read_config_word(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 2); // Bus master
    pci_write_config_word(dev->bus, dev->device, dev->function, 0x04, command);

    io_base = dev->bar[0] & ~3;
    if (io_base == 0) {
        kprintf("[RTL8139] Invalid IO base.\n");
        return;
    }

    // Power on
    outb(io_base + RTL8139_CONFIG1, 0x00);

    // Software Reset!
    outb(io_base + RTL8139_COMMAND, 0x10);
    int timeout = 10000;
    while ((inb(io_base + RTL8139_COMMAND) & 0x10) != 0 && timeout > 0) {
        for (volatile int i = 0; i < 1000; i++) __asm__ volatile("pause");
        timeout--;
    }

    // Allocate physical memory for RX buffer
    int pages = (RX_BUF_TOTAL + 4095) / 4096;
    uint32_t phys_addr = pmm_alloc_blocks(pages);
    rx_buffer = (uint8_t *)(uint64_t)(phys_addr + 0xFFFF800000000000);

    outl(io_base + RTL8139_RX_BUF, phys_addr);

    // Allocate 4 TX buffers
    for (int i = 0; i < 4; i++) {
        tx_phys_addr[i] = pmm_alloc();
        tx_buffers[i] = (uint8_t *)(uint64_t)(tx_phys_addr[i] + 0xFFFF800000000000);
    }

    // Set IMR + ISR
    outw(io_base + RTL8139_INTR_MASK, 0x0005); // ROK and TOK

    // Configure RX (Accept Broadcast, Multicast, My MAC) + (WRAP mode)
    outl(io_base + RTL8139_RX_CONFIG, 0xf | (1 << 7));

    // Enable RX and TX
    outb(io_base + RTL8139_COMMAND, 0x0C);

    // Get MAC address
    uint32_t mac1 = inl(io_base + RTL8139_MAC0);
    uint16_t mac2 = inw(io_base + RTL8139_MAC0 + 4);
    rtl8139_mac[0] = mac1 & 0xFF;
    rtl8139_mac[1] = (mac1 >> 8) & 0xFF;
    rtl8139_mac[2] = (mac1 >> 16) & 0xFF;
    rtl8139_mac[3] = (mac1 >> 24) & 0xFF;
    rtl8139_mac[4] = mac2 & 0xFF;
    rtl8139_mac[5] = (mac2 >> 8) & 0xFF;

    kprintf("[RTL8139] Init complete. MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
            rtl8139_mac[0], rtl8139_mac[1], rtl8139_mac[2], 
            rtl8139_mac[3], rtl8139_mac[4], rtl8139_mac[5]);
}

void rtl8139_send_packet(void *data, uint32_t len) {
    if (!rtl8139_found) return;
    
    uint8_t *tx_buffer = tx_buffers[tx_cur];
    uint32_t phys_addr = tx_phys_addr[tx_cur];
    
    memcpy(tx_buffer, data, len);
    if (len < 60) len = 60; // Pad short packets

    outl(io_base + RTL8139_TX_ADDR0 + (tx_cur * 4), phys_addr);
    outl(io_base + RTL8139_TX_STATUS0 + (tx_cur * 4), len);

    tx_cur = (tx_cur + 1) % 4;
}

void rtl8139_receive_packet(void) {
    if (!rtl8139_found) return;

    uint16_t status = inb(io_base + RTL8139_COMMAND);
    if ((status & 0x01) == 1) return; // Buffer is empty

    uint16_t *t = (uint16_t *)(rx_buffer + current_rx_ptr);
    uint16_t packet_length = t[1]; // length includes header + CRC
    
    if (packet_length == 0 || packet_length > 1536) {
        kprintf("[RTL8139] Invalid packet length: %u\n", packet_length);
        // Reset the driver or just skip 4 bytes to try recovering?
        // Actually, if we get 0, we must advance or we infinite loop.
        current_rx_ptr = (current_rx_ptr + 4 + 3) & ~3;
    } else {
        void *packet = rx_buffer + current_rx_ptr + 4; // Skip 4 byte header
        
        if (rx_cb) {
            rx_cb(packet, packet_length - 4); // Remove 4 bytes CRC
        }

        current_rx_ptr = (current_rx_ptr + packet_length + 4 + 3) & ~3;
    }
    
    if (current_rx_ptr >= RX_BUF_SIZE) {
        current_rx_ptr -= RX_BUF_SIZE;
    }

    outw(io_base + RTL8139_CAPR, current_rx_ptr - 16);
}

void rtl8139_poll(void) {
    if (!rtl8139_found) return;
    uint16_t status = inw(io_base + RTL8139_INTR_STATUS);
    if (status & 0x01) { // RX OK
        // Drain ALL pending packets, not just one!
        // Otherwise HTTP data packets get stuck behind TCP ACKs
        int max_packets = 64; // Safety limit to prevent infinite loop
        while (max_packets-- > 0) {
            uint16_t cmd = inb(io_base + RTL8139_COMMAND);
            if (cmd & 0x01) break; // Buffer empty (BUFE bit set)
            rtl8139_receive_packet();
        }
    }
    outw(io_base + RTL8139_INTR_STATUS, status); // Clear interrupts
}
