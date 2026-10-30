#include "drivers/virtio_gpu.h"
#include "drivers/virtio.h"
#include "kernel/pci.h"
#include "kernel/pmm.h"
#include "zarqaa/string.h"

extern uint64_t hhdm_offset;
static bool gpu_ready = false;
static virtqueue_t controlq;

static volatile virtio_pci_common_cfg_t *common_cfg = NULL;

bool virtio_gpu_init(const pci_device_t *dev) {
    kprintf("VirtIO GPU: Initializing device %x:%x\n", dev->vendor_id, dev->device_id);
    
    // 1. Find VirtIO capabilities
    uint8_t cap_ptr = pci_find_capability(dev->bus, dev->device, dev->function, 0x09);
    uint32_t notify_off_multiplier = 0;
    uint32_t notify_bar = 0;
    uint32_t notify_offset_base = 0;
    
    while (cap_ptr != 0) {
        uint8_t cfg_type = pci_read_config_byte(dev->bus, dev->device, dev->function, cap_ptr + 3);
        uint8_t bar = pci_read_config_byte(dev->bus, dev->device, dev->function, cap_ptr + 4);
        uint32_t offset = pci_read_config_dword(dev->bus, dev->device, dev->function, cap_ptr + 8);
        
        if (cfg_type == VIRTIO_PCI_CAP_COMMON_CFG) {
            uint32_t bar_phys = dev->bar[bar] & 0xFFFFFFF0;
            common_cfg = (volatile void *)(bar_phys + hhdm_offset + offset);
        } else if (cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG) {
            notify_bar = bar;
            notify_offset_base = offset;
            notify_off_multiplier = pci_read_config_dword(dev->bus, dev->device, dev->function, cap_ptr + 16);
        }
        cap_ptr = pci_read_config_byte(dev->bus, dev->device, dev->function, cap_ptr + 1) & 0xFC;
    }
    
    if (!common_cfg) {
        kprintf("VirtIO GPU: Modern common cfg not found!\n");
        return false;
    }

    // Reset device
    common_cfg->device_status = 0;
    // Acknowledge and Driver
    common_cfg->device_status |= VIRTIO_STATUS_ACKNOWLEDGE;
    common_cfg->device_status |= VIRTIO_STATUS_DRIVER;
    
    // Features OK
    common_cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    if (!(common_cfg->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("VirtIO GPU: Features NOT OK!\n");
        return false;
    }

    // Setup controlq (Index 0)
    common_cfg->queue_select = 0;
    uint16_t q_size = common_cfg->queue_size;
    uint32_t notify_off = common_cfg->queue_notify_off;
    
    uint32_t notify_addr_phys = (dev->bar[notify_bar] & 0xFFFFFFF0) + notify_offset_base + notify_off * notify_off_multiplier;
    void *notify_addr = (void*)(notify_addr_phys + hhdm_offset);
    
    virtqueue_init(&controlq, q_size, notify_off, notify_addr);
    
    // Inform device about queue addresses (convert to physical)
    uint64_t desc_phys = (uint64_t)controlq.desc - hhdm_offset;
    uint64_t avail_phys = (uint64_t)controlq.avail - hhdm_offset;
    uint64_t used_phys = (uint64_t)controlq.used - hhdm_offset;
    
    common_cfg->queue_desc_lo = (uint32_t)desc_phys;
    common_cfg->queue_desc_hi = (uint32_t)(desc_phys >> 32);
    common_cfg->queue_avail_lo = (uint32_t)avail_phys;
    common_cfg->queue_avail_hi = (uint32_t)(avail_phys >> 32);
    common_cfg->queue_used_lo = (uint32_t)used_phys;
    common_cfg->queue_used_hi = (uint32_t)(used_phys >> 32);
    
    common_cfg->queue_enable = 1;

    // Setup cursorq (Index 1) - Similar to controlq but omitted for brevity
    
    // Driver OK
    common_cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    
    kprintf("VirtIO GPU: Driver OK, Queues configured.\n");
    gpu_ready = true;
    return true;
}

uint32_t *virtio_gpu_flip(uint32_t *vram_base) {
    (void)vram_base;
    if (!gpu_ready) return NULL;
    
    // In a real implementation, we would send VIRTIO_GPU_CMD_SET_SCANOUT
    // and VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D here.
    // For now, we fallback to software swap as sending full DMA commands
    // requires a more complex chained virtqueue descriptor setup.
    return NULL; 
}
