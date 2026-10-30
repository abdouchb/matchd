#include "drivers/virtio.h"
#include "kernel/pmm.h"
#include "zarqaa/string.h"
#include "zarqaa/types.h"

extern uint64_t hhdm_offset;

static inline void io_memory_barrier(void) {
    __asm__ volatile("mfence" ::: "memory");
}

bool virtqueue_init(virtqueue_t *vq, uint16_t size, uint32_t notify_offset, void *notify_addr) {
    vq->size = size;
    vq->free_idx = 0;
    vq->last_used_idx = 0;
    vq->notify_offset = notify_offset;
    vq->notify_addr = notify_addr;

    // Allocate physical memory for the ring (size * 16 for desc, size * 2 for avail, size * 8 for used + some padding)
    // Actually, each queue requires some pages. A simple allocation of 4 contiguous pages (16KB) is enough for sizes up to 256.
    uint64_t phys_addr = pmm_alloc_blocks(4);
    if (!phys_addr) return false;
    
    memset((void*)(phys_addr + hhdm_offset), 0, 4 * PAGE_SIZE);

    uint8_t *virt_base = (uint8_t *)(phys_addr + hhdm_offset);
    
    // Desc array (16 bytes each)
    vq->desc = (vring_desc_t *)virt_base;
    uint32_t avail_offset = size * sizeof(vring_desc_t);
    
    // Avail ring (4 bytes + size * 2 bytes + 2 bytes padding)
    vq->avail = (vring_avail_t *)(virt_base + avail_offset);
    uint32_t used_offset = avail_offset + sizeof(uint16_t) * (3 + size);
    
    // Align used ring to 4096 (per virtio legacy spec, but modern it doesn't matter much if we provide pointers, though we assume contiguous here so let's align to PAGE_SIZE just in case, or just 4 bytes)
    used_offset = (used_offset + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    vq->used = (vring_used_t *)(virt_base + used_offset);

    // Initialize desc free list
    for (uint16_t i = 0; i < size - 1; i++) {
        vq->desc[i].next = i + 1;
    }
    vq->desc[size - 1].next = 0xFFFF; // End of list

    return true;
}

void virtqueue_push(virtqueue_t *vq, void *buffer, uint32_t len, bool write) {
    if (vq->free_idx >= vq->size) return; // Full (simplification: assume we don't wrap yet for 1 command)

    uint16_t head = vq->free_idx;
    vq->free_idx = vq->desc[head].next;

    // Get physical address for buffer
    uint64_t phys_addr = (uint64_t)buffer;
    if (phys_addr >= hhdm_offset) {
        phys_addr -= hhdm_offset;
    }

    vq->desc[head].addr = phys_addr;
    vq->desc[head].len = len;
    vq->desc[head].flags = write ? VRING_DESC_F_WRITE : 0;
    vq->desc[head].next = 0; // single descriptor

    io_memory_barrier();

    uint16_t avail_idx = vq->avail->idx;
    vq->avail->ring[avail_idx % vq->size] = head;

    io_memory_barrier();
    vq->avail->idx = avail_idx + 1;
    io_memory_barrier();
}

void virtqueue_kick(virtqueue_t *vq) {
    if (vq->notify_addr) {
        *(volatile uint16_t *)vq->notify_addr = 0; // usually queue index is written, but this is a simplified stub
    }
}
