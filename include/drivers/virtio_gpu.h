#pragma once

#include "zarqaa/types.h"
#include "kernel/pci.h"

// VirtIO GPU Commands
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO       0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D     0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF         0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT            0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH         0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D    0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x0107

#define VIRTIO_GPU_RESP_OK_NODATA             0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO       0x1101

// VirtIO GPU Structs
typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t padding[4];
} __attribute__((packed)) virtio_gpu_ctrl_hdr_t;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) virtio_gpu_rect_t;

typedef struct {
    virtio_gpu_rect_t r;
    uint32_t enabled;
    uint32_t flags;
} __attribute__((packed)) virtio_gpu_display_one_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_display_one_t pmodes[16];
} __attribute__((packed)) virtio_gpu_resp_display_info_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) virtio_gpu_resource_create_2d_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} __attribute__((packed)) virtio_gpu_resource_attach_backing_t;

typedef struct {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_mem_entry_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint32_t scanout_id;
    uint32_t resource_id;
} __attribute__((packed)) virtio_gpu_set_scanout_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_transfer_to_host_2d_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_resource_flush_t;

// Initialization prototype
bool virtio_gpu_init(const pci_device_t *dev);
uint32_t *virtio_gpu_flip(uint32_t *vram_base);

