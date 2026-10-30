#include "drivers/e1000.h"
#include "kernel/pci.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "zarqaa/string.h"

// E1000 Registers
#define REG_CTRL        0x0000
#define REG_STATUS      0x0008
#define REG_EERD        0x0014
#define REG_ICR         0x00C0
#define REG_IMS         0x00D0
#define REG_IMC         0x00D8
#define REG_RCTL        0x0100
#define REG_TCTL        0x0400
#define REG_RDBAL       0x2800
#define REG_RDBAH       0x2804
#define REG_RDLEN       0x2808
#define REG_RDH         0x2810
#define REG_RDT         0x2818
#define REG_TDBAL       0x3800
#define REG_TDBAH       0x3804
#define REG_TDLEN       0x3808
#define REG_TDH         0x3810
#define REG_TDT         0x3818
#define REG_MTA         0x5200
#define REG_RAL         0x5400
#define REG_RAH         0x5404

#define RCTL_EN         (1 << 1)
#define RCTL_SBP        (1 << 2)
#define RCTL_UPE        (1 << 3)
#define RCTL_MPE        (1 << 4)
#define RCTL_LPE        (1 << 5)
#define RCTL_BAM        (1 << 15)
#define RCTL_BSIZE_2048 (0 << 16)
#define RCTL_SECRC      (1 << 26)

#define TCTL_EN         (1 << 1)
#define TCTL_PSP        (1 << 3)

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 8

struct e1000_rx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t status;
    volatile uint8_t css;
    volatile uint16_t special;
} __attribute__((packed));

static uint8_t *mmio_base = NULL;
bool e1000_found = false;
uint8_t e1000_mac[6];
static e1000_rx_callback_t rx_cb = NULL;

static struct e1000_rx_desc *rx_descs;
static struct e1000_tx_desc *tx_descs;
static uint16_t rx_cur = 0;
static uint16_t tx_cur = 0;
static uint8_t *rx_buffers[E1000_NUM_RX_DESC];
static uint8_t *tx_buffers[E1000_NUM_TX_DESC];

static void mmio_write32(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *)(mmio_base + reg) = value;
}

static uint32_t mmio_read32(uint32_t reg) {
    return *(volatile uint32_t *)(mmio_base + reg);
}

void e1000_set_rx_callback(e1000_rx_callback_t cb) {
    rx_cb = cb;
}

static uint16_t eeprom_read(uint8_t addr) {
    uint32_t tmp = 0;
    mmio_write32(REG_EERD, 1 | ((uint32_t)addr << 8));
    int timeout = 100000;
    while (!((tmp = mmio_read32(REG_EERD)) & (1 << 4)) && timeout > 0) {
        timeout--;
    }
    return (uint16_t)((tmp >> 16) & 0xFFFF);
}

static void get_mac_address(void) {
    uint16_t temp;
    temp = eeprom_read(0);
    e1000_mac[0] = temp & 0xFF;
    e1000_mac[1] = temp >> 8;
    temp = eeprom_read(1);
    e1000_mac[2] = temp & 0xFF;
    e1000_mac[3] = temp >> 8;
    temp = eeprom_read(2);
    e1000_mac[4] = temp & 0xFF;
    e1000_mac[5] = temp >> 8;
}

#include "kernel/driver.h"

static bool e1000_driver_init(const pci_device_t *dev) {
    if (dev->vendor_id != E1000_VENDOR_ID) return false;

    uint16_t supported_devices[] = {
        0x100E, // 82540EM (MT Desktop)
        0x100F, // 82545EM (MT Server)
        0x1004, // 82543GC (T Server)
        0x10D3, // 82574L
        0x1019, // 82547EI
        0       // End of list
    };

    bool supported = false;
    for (int i = 0; supported_devices[i] != 0; i++) {
        if (dev->device_id == supported_devices[i]) {
            supported = true;
            kprintf("[E1000] Found device ID: 0x%X\n", supported_devices[i]);
            break;
        }
    }

    if (!supported) {
        return false;
    }

    e1000_found = true;

    // Enable bus mastering
    uint16_t command = pci_read_config_word(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 2);
    pci_write_config_word(dev->bus, dev->device, dev->function, 0x04, command);

    uint32_t bar0 = dev->bar[0] & ~0xF;
    mmio_base = (uint8_t *)(uint64_t)(bar0 + 0xFFFF800000000000);

    get_mac_address();
    kprintf("[E1000] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
        e1000_mac[0], e1000_mac[1], e1000_mac[2],
        e1000_mac[3], e1000_mac[4], e1000_mac[5]);

    for (int i = 0; i < 128; i++) {
        mmio_write32(REG_MTA + i * 4, 0);
    }

    mmio_write32(REG_IMC, 0xFFFFFFFF); // Disable interrupts

    // Allocate Rx Rings
    uint32_t rx_phys = pmm_alloc();
    rx_descs = (struct e1000_rx_desc *)(uint64_t)(rx_phys + 0xFFFF800000000000);
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        uint32_t buf_phys = pmm_alloc();
        rx_buffers[i] = (uint8_t *)(uint64_t)(buf_phys + 0xFFFF800000000000);
        rx_descs[i].addr = buf_phys;
        rx_descs[i].status = 0;
    }

    mmio_write32(REG_RDBAL, rx_phys);
    mmio_write32(REG_RDBAH, 0);
    mmio_write32(REG_RDLEN, E1000_NUM_RX_DESC * 16);
    mmio_write32(REG_RDH, 0);
    mmio_write32(REG_RDT, E1000_NUM_RX_DESC - 1);

    // Enable Rx
    mmio_write32(REG_RCTL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_LPE | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC);

    // Allocate Tx Rings
    uint32_t tx_phys = pmm_alloc();
    tx_descs = (struct e1000_tx_desc *)(uint64_t)(tx_phys + 0xFFFF800000000000);
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        uint32_t buf_phys = pmm_alloc();
        tx_buffers[i] = (uint8_t *)(uint64_t)(buf_phys + 0xFFFF800000000000);
        tx_descs[i].addr = buf_phys;
        tx_descs[i].cmd = 0;
        tx_descs[i].status = 1;
    }

    mmio_write32(REG_TDBAL, tx_phys);
    mmio_write32(REG_TDBAH, 0);
    mmio_write32(REG_TDLEN, E1000_NUM_TX_DESC * 16);
    mmio_write32(REG_TDH, 0);
    mmio_write32(REG_TDT, 0);

    // Enable Tx
    mmio_write32(REG_TCTL, TCTL_EN | TCTL_PSP);

    // Enable Interrupts
    mmio_write32(REG_IMS, 0x1F6DC);

    return true;
}

void e1000_send_packet(const void *data, uint32_t len) {
    tx_descs[tx_cur].addr = (uint32_t)((uint64_t)tx_buffers[tx_cur] - 0xFFFF800000000000);
    tx_descs[tx_cur].length = len;
    tx_descs[tx_cur].cmd = (1 << 3) | (1 << 1) | 1; // RS | IFCS | EOP
    tx_descs[tx_cur].status = 0;

    memcpy(tx_buffers[tx_cur], data, len);

    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    mmio_write32(REG_TDT, tx_cur);
}

void e1000_poll(void) {
    if (!e1000_found) return;

    mmio_write32(REG_IMC, 0xFFFFFFFF);
    uint32_t status = mmio_read32(REG_ICR);

    if (status & 0x80) { // LSC
        // Link status change
    }

    if (status & 0x04) { // RXT0
        while (rx_descs[rx_cur].status & 0x01) {
            if (rx_cb) {
                rx_cb(rx_buffers[rx_cur], rx_descs[rx_cur].length);
            }
            rx_descs[rx_cur].status = 0;
            mmio_write32(REG_RDT, rx_cur);
            rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
        }
    }
}

static pci_driver_t e1000_driver = {
    .name = "Intel E1000 Ethernet",
    .match_type = DRIVER_MATCH_CLASS,
    .vendor_id = 0x8086,
    .class_code = 0x02, // Network
    .subclass = 0x00,   // Ethernet
    .init = e1000_driver_init
};

void e1000_init(void) {
    driver_register(&e1000_driver);
}
