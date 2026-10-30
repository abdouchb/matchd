#pragma once

#include "zarqaa/types.h"

// VirtIO PCI Capability types
#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG    3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
#define VIRTIO_PCI_CAP_PCI_CFG    5

// Common Config Offsets
#define VIRTIO_PCI_COMMON_DFSELECT     0x00
#define VIRTIO_PCI_COMMON_DF           0x04
#define VIRTIO_PCI_COMMON_GFSELECT     0x08
#define VIRTIO_PCI_COMMON_GF           0x0c
#define VIRTIO_PCI_COMMON_MSIX         0x10
#define VIRTIO_PCI_COMMON_NUMQ         0x12
#define VIRTIO_PCI_COMMON_STATUS       0x14
#define VIRTIO_PCI_COMMON_CFG_GEN      0x15
#define VIRTIO_PCI_COMMON_Q_SELECT     0x16
#define VIRTIO_PCI_COMMON_Q_SIZE       0x18
#define VIRTIO_PCI_COMMON_Q_MSIX       0x1a
#define VIRTIO_PCI_COMMON_Q_ENABLE     0x1c
#define VIRTIO_PCI_COMMON_Q_NOFF       0x1e
#define VIRTIO_PCI_COMMON_Q_DESCLO     0x20
#define VIRTIO_PCI_COMMON_Q_DESCHI     0x24
#define VIRTIO_PCI_COMMON_Q_AVALLO     0x28
#define VIRTIO_PCI_COMMON_Q_AVALHI     0x2c
#define VIRTIO_PCI_COMMON_Q_USEDLO     0x30
#define VIRTIO_PCI_COMMON_Q_USEDHI     0x34

// Device Status Bits
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED      128

// Virtqueue Descriptor Flags
#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2
#define VRING_DESC_F_INDIRECT 4

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) vring_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[]; // size is queue_size
} __attribute__((packed)) vring_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) vring_used_elem_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[]; // size is queue_size
} __attribute__((packed)) vring_used_t;

typedef struct {
    uint16_t size;
    uint16_t free_idx;
    uint16_t last_used_idx;
    uint32_t notify_offset;
    vring_desc_t  *desc;
    vring_avail_t *avail;
    vring_used_t  *used;
    void          *notify_addr;
} virtqueue_t;

// Capability structure in PCI config space
typedef struct {
    uint8_t cap_vndr;      // Generic PCI field: PCI_CAP_ID_VNDR
    uint8_t cap_next;      // Generic PCI field: next ptr
    uint8_t cap_len;       // Generic PCI field: capability length
    uint8_t cfg_type;      // Identifies the structure
    uint8_t bar;           // Where to find it
    uint8_t id;            // Multiple capabilities of the same type
    uint8_t padding[2];    // Pad to full dword
    uint32_t offset;       // Offset within bar
    uint32_t length;       // Length of the structure, in bytes
} __attribute__((packed)) virtio_pci_cap_t;

typedef struct {
    virtio_pci_cap_t cap;
    uint32_t notify_off_multiplier; /* Multiplier for queue_notify_off */
} __attribute__((packed)) virtio_pci_notify_cap_t;

typedef struct {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint32_t queue_desc_lo;
    uint32_t queue_desc_hi;
    uint32_t queue_avail_lo;
    uint32_t queue_avail_hi;
    uint32_t queue_used_lo;
    uint32_t queue_used_hi;
} __attribute__((packed)) virtio_pci_common_cfg_t;

// Queue initialization prototype
bool virtqueue_init(virtqueue_t *vq, uint16_t size, uint32_t notify_offset, void *notify_addr);
void virtqueue_push(virtqueue_t *vq, void *buffer, uint32_t len, bool write);
void virtqueue_kick(virtqueue_t *vq);

