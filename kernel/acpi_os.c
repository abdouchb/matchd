#include <uacpi/kernel_api.h>
#include "kernel/vmm.h"
#include "kernel/pmm.h"
#include "kernel/pci.h"
#include "kernel/irq.h"
#include "kernel/heap.h"
#include "zarqaa/types.h"
#include "zarqaa/string.h"

extern uint64_t hhdm_offset;
extern uint64_t g_rsdp_address;

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    if (g_rsdp_address == 0) return UACPI_STATUS_NOT_FOUND;
    *out_rsdp_address = g_rsdp_address;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle) {
    // Return the address directly as handle
    *out_handle = (uacpi_handle)(uintptr_t)((address.segment << 16) | (address.bus << 8) | (address.device << 3) | address.function);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) {
    (void)handle;
}

static void get_bdf(uacpi_handle handle, uint8_t *bus, uint8_t *dev, uint8_t *func) {
    uintptr_t val = (uintptr_t)handle;
    *bus = (val >> 8) & 0xFF;
    *dev = (val >> 3) & 0x1F;
    *func = val & 0x7;
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *value) {
    uint8_t bus, dev, func; get_bdf(device, &bus, &dev, &func);
    *value = pci_read_config_byte(bus, dev, func, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *value) {
    uint8_t bus, dev, func; get_bdf(device, &bus, &dev, &func);
    *value = pci_read_config_word(bus, dev, func, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *value) {
    uint8_t bus, dev, func; get_bdf(device, &bus, &dev, &func);
    *value = pci_read_config_dword(bus, dev, func, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value) {
    (void)device; (void)offset; (void)value;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value) {
    uint8_t bus, dev, func; get_bdf(device, &bus, &dev, &func);
    pci_write_config_word(bus, dev, func, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value) {
    uint8_t bus, dev, func; get_bdf(device, &bus, &dev, &func);
    pci_write_config_dword(bus, dev, func, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle) {
    (void)len;
    *out_handle = (uacpi_handle)(uintptr_t)base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    (void)handle;
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value) {
    *out_value = inb((uint16_t)(uintptr_t)handle + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value) {
    *out_value = inw((uint16_t)(uintptr_t)handle + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value) {
    *out_value = inl((uint16_t)(uintptr_t)handle + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value) {
    outb((uint16_t)(uintptr_t)handle + offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value) {
    outw((uint16_t)(uintptr_t)handle + offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value) {
    outl((uint16_t)(uintptr_t)handle + offset, in_value);
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    uint64_t start_page = addr & ~0xFFF;
    uint64_t end_page = (addr + len + 0xFFF) & ~0xFFF;

    for (uint64_t p = start_page; p < end_page; p += 4096) {
        uint64_t virt = p + hhdm_offset;
        if (!vmm_is_mapped(kernel_pml4, virt)) {
            vmm_map_page(kernel_pml4, virt, p, PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);
        }
    }
    
    return (void *)(addr + hhdm_offset);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    (void)addr; (void)len;
}

void *uacpi_kernel_alloc(uacpi_size size) {
    return kmalloc(size);
}

void uacpi_kernel_free(void *mem) {
    kfree(mem);
}

void uacpi_kernel_log(uacpi_log_level level, const uacpi_char *str) {
    (void)level;
    kprintf("%s", str);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    extern uint64_t pit_get_ticks(void);
    return pit_get_ticks() * 1000000ULL; // Approximate 1ms per tick to nanoseconds
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    for (volatile uint64_t i = 0; i < (uint64_t)usec * 10; i++);
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    for (volatile uint64_t i = 0; i < (uint64_t)msec * 10000; i++);
}

uacpi_handle uacpi_kernel_create_mutex(void) { return (uacpi_handle)1; }
void uacpi_kernel_free_mutex(uacpi_handle h) { (void)h; }
uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle h, uacpi_u16 t) { (void)h; (void)t; return UACPI_STATUS_OK; }
void uacpi_kernel_release_mutex(uacpi_handle h) { (void)h; }

uacpi_handle uacpi_kernel_create_event(void) { return (uacpi_handle)1; }
void uacpi_kernel_free_event(uacpi_handle h) { (void)h; }
uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle h, uacpi_u16 t) { (void)h; (void)t; return UACPI_TRUE; }
void uacpi_kernel_signal_event(uacpi_handle h) { (void)h; }
void uacpi_kernel_reset_event(uacpi_handle h) { (void)h; }

uacpi_thread_id uacpi_kernel_get_thread_id(void) { return (uacpi_thread_id)1; }

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    uacpi_interrupt_state flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags));
    return flags;
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
    if (state & (1 << 9)) asm volatile("sti");
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req) {
    (void)req;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    (void)irq; (void)handler; (void)ctx; (void)out_irq_handle;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler handler, uacpi_handle irq_handle
) {
    (void)handler; (void)irq_handle;
    return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_spinlock(void) { return (uacpi_handle)1; }
void uacpi_kernel_free_spinlock(uacpi_handle h) { (void)h; }
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h) { (void)h; return uacpi_kernel_disable_interrupts(); }
void uacpi_kernel_unlock_spinlock(uacpi_handle h, uacpi_cpu_flags f) { (void)h; uacpi_kernel_restore_interrupts(f); }

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx) {
    (void)type;
    handler(ctx); // Direct execution for now
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_OK;
}
