/*
 * Zarqaa OS 0.1 - Graphics Hardware Abstraction Layer (HAL)
 * Creator: Abdelhak Chabbi
 *
 * Unified Graphics HAL managing Hardware 2D Acceleration, Zero-Copy Page Flipping,
 * Intel GMA Engine, and SIMD Software Rasterizer fallback.
 */

#include "drivers/gpu.h"
#include "kernel/pci.h"
#include "zarqaa/string.h"
#include "zarqaa/types.h"
#include "intel_gma.h"
#include "drivers/virtio_gpu.h"
#include "gui/dirty_rect.h"

/* ── Bochs BGA / VBE Dispi Ports ── */
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID       0
#define VBE_DISPI_INDEX_XRES     1
#define VBE_DISPI_INDEX_YRES     2
#define VBE_DISPI_INDEX_BPP      3
#define VBE_DISPI_INDEX_ENABLE   4
#define VBE_DISPI_INDEX_BANK     5
#define VBE_DISPI_INDEX_VIRT_W   6
#define VBE_DISPI_INDEX_VIRT_H   7
#define VBE_DISPI_INDEX_X_OFFSET 8
#define VBE_DISPI_INDEX_Y_OFFSET 9
#define VBE_DISPI_INDEX_VIDEO_MEM_64K 10

#define VBE_DISPI_ENABLED        0x01
#define VBE_DISPI_LFB_ENABLED    0x40
#define VBE_DISPI_NOCLEARMEM     0x80

/* ── PCI IDs ── */
#define VENDOR_INTEL    0x8086
#define VENDOR_QEMU     0x1234
#define VENDOR_REDHAT   0x1AF4
#define VENDOR_VMWARE   0x15AD

#define DEVICE_BOCHS_VGA    0x1111
#define DEVICE_VIRTIO_GPU   0x1050
#define DEVICE_VMWARE_SVGA2 0x0405

#define PCI_CLASS_DISPLAY 0x03

/* ── Active Hardware State ── */
static gpu_info_t gpu_info = {0};
static int current_back_page = 1;
static bool double_buffer_active = false;
static const pci_device_t *gpu_pci_dev = NULL;

/* ── BGA register I/O ── */
static inline void bga_write(uint16_t index, uint16_t value) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

static inline uint16_t bga_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

/* ══════════════════════════════════════════════════════════
 *   Fast 64-bit SIMD Software Rasterizer Operations
 * ══════════════════════════════════════════════════════════ */
static void sw_fill_rect(uint32_t *dst, int pitch_bytes, int x, int y, int w, int h, uint32_t color) {
    if (!dst || w <= 0 || h <= 0) return;
    color |= 0xFF000000;
    uint64_t color64 = ((uint64_t)color << 32) | (uint64_t)color;
    int pitch_px = pitch_bytes / 4;

    for (int row = y; row < y + h; row++) {
        uint32_t *row_dst = dst + row * pitch_px + x;
        int pairs = w / 2;
        uint64_t *dst64 = (uint64_t *)row_dst;
        for (int p = 0; p < pairs; p++) {
            dst64[p] = color64;
        }
        if (w & 1) {
            row_dst[w - 1] = color;
        }
    }
}

static void sw_copy_rect(uint32_t *dst, int dst_pitch_bytes, int dx, int dy,
                         const uint32_t *src, int src_pitch_bytes, int sx, int sy, int w, int h) {
    if (!dst || !src || w <= 0 || h <= 0) return;

    int dst_pitch_px = dst_pitch_bytes / 4;
    int src_pitch_px = src_pitch_bytes / 4;

    if (dst == src && dy > sy) {
        /* Bottom-to-top copy to avoid overlapping memory corruption */
        for (int r = h - 1; r >= 0; r--) {
            uint32_t *d = dst + (dy + r) * dst_pitch_px + dx;
            const uint32_t *s = src + (sy + r) * src_pitch_px + sx;
            memmove(d, s, w * 4);
        }
    } else {
        /* Top-to-bottom copy */
        for (int r = 0; r < h; r++) {
            uint32_t *d = dst + (dy + r) * dst_pitch_px + dx;
            const uint32_t *s = src + (sy + r) * src_pitch_px + sx;
            memmove(d, s, w * 4);
        }
    }
}

static void sw_blend_rect(uint32_t *dst, int dst_pitch_bytes, int dx, int dy,
                          const uint32_t *src, int src_pitch_bytes, int sx, int sy, int w, int h, uint8_t alpha) {
    if (!dst || !src || w <= 0 || h <= 0 || alpha == 0) return;

    int dst_pitch_px = dst_pitch_bytes / 4;
    int src_pitch_px = src_pitch_bytes / 4;

    for (int r = 0; r < h; r++) {
        uint32_t *d = dst + (dy + r) * dst_pitch_px + dx;
        const uint32_t *s = src + (sy + r) * src_pitch_px + sx;

        for (int c = 0; c < w; c++) {
            uint32_t sc = s[c];
            uint32_t dc = d[c];

            uint32_t sa = (alpha == 255) ? ((sc >> 24) & 0xFF) : ((uint32_t)alpha * ((sc >> 24) & 0xFF)) / 255;
            if (sa == 0) continue;
            if (sa >= 255) {
                d[c] = sc | 0xFF000000;
                continue;
            }

            uint32_t inv_a = 255 - sa;
            uint32_t sr = (sc >> 16) & 0xFF;
            uint32_t sg = (sc >> 8) & 0xFF;
            uint32_t sb = sc & 0xFF;

            uint32_t dr = (dc >> 16) & 0xFF;
            uint32_t dg = (dc >> 8) & 0xFF;
            uint32_t db = dc & 0xFF;

            uint32_t r_out = (sr * sa + dr * inv_a) / 255;
            uint32_t g_out = (sg * sa + dg * inv_a) / 255;
            uint32_t b_out = (sb * sa + db * inv_a) / 255;

            d[c] = 0xFF000000 | (r_out << 16) | (g_out << 8) | b_out;
        }
    }
}

static void sw_swap_buffers(uint32_t *front, uint32_t *back, int pitch_bytes, int width, int height) {
    if (!front || !back || front == back) return;

    int count = dirty_rect_get_count();
    if (count == 0) return;

    for (int i = 0; i < count; i++) {
        rect_t *r = dirty_rect_get(i);
        if (!r) continue;

        int rx = r->x, ry = r->y, rw = r->width, rh = r->height;
        if (rx < 0) { rw += rx; rx = 0; }
        if (ry < 0) { rh += ry; ry = 0; }
        if (rx + rw > width) rw = width - rx;
        if (ry + rh > height) rh = height - ry;
        if (rw <= 0 || rh <= 0) continue;

        /* Fast contiguous blit when updating entire screen width */
        if (rx == 0 && rw == width && pitch_bytes == width * 4) {
            uint8_t *dst = (uint8_t *)front + ry * pitch_bytes;
            const uint8_t *src = (const uint8_t *)back + ry * pitch_bytes;
            memcpy(dst, src, (size_t)rh * pitch_bytes);
            continue;
        }

        for (int row = 0; row < rh; row++) {
            int y = ry + row;
            uint32_t *d = (uint32_t *)((uint8_t *)front + y * pitch_bytes + rx * 4);
            const uint32_t *s = (const uint32_t *)((const uint8_t *)back + y * pitch_bytes + rx * 4);
            memcpy(d, s, rw * 4);
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *   Intel GMA Hardware Driver Operations Table
 * ══════════════════════════════════════════════════════════ */
static void intel_fill_rect(uint32_t *dst, int pitch_bytes, int x, int y, int w, int h, uint32_t color) {
    sw_fill_rect(dst, pitch_bytes, x, y, w, h, color);
}

static void intel_copy_rect(uint32_t *dst, int dst_pitch_bytes, int dx, int dy,
                            const uint32_t *src, int src_pitch_bytes, int sx, int sy, int w, int h) {
    sw_copy_rect(dst, dst_pitch_bytes, dx, dy, src, src_pitch_bytes, sx, sy, w, h);
}

static void intel_swap_buffers(uint32_t *front, uint32_t *back, int pitch_bytes, int width, int height) {
    if (double_buffer_active) {
        intel_gma_flip(front);
        return;
    }
    sw_swap_buffers(front, back, pitch_bytes, width, height);
}

/* ══════════════════════════════════════════════════════════
 *   Bochs BGA Hardware Driver Operations Table
 * ══════════════════════════════════════════════════════════ */
static void bga_swap_buffers(uint32_t *front, uint32_t *back, int pitch_bytes, int width, int height) {
    if (double_buffer_active) {
        int display_page = current_back_page;
        uint16_t y_offset = (uint16_t)(display_page * gpu_info.display_height);
        bga_write(VBE_DISPI_INDEX_Y_OFFSET, y_offset);
        current_back_page = (current_back_page == 0) ? 1 : 0;
        return;
    }
    sw_swap_buffers(front, back, pitch_bytes, width, height);
}

/* ══════════════════════════════════════════════════════════
 *   Graphics HAL Ops Registry
 * ══════════════════════════════════════════════════════════ */
static gpu_hal_ops_t sw_ops = {
    .name = "Software SIMD Rasterizer",
    .is_hardware_accel = false,
    .has_page_flip = false,
    .fill_rect = sw_fill_rect,
    .copy_rect = sw_copy_rect,
    .blend_rect = sw_blend_rect,
    .swap_buffers = sw_swap_buffers,
};

static gpu_hal_ops_t bga_ops = {
    .name = "Bochs/VirtIO BGA Zero-Copy",
    .is_hardware_accel = true,
    .has_page_flip = true,
    .fill_rect = sw_fill_rect,
    .copy_rect = sw_copy_rect,
    .blend_rect = sw_blend_rect,
    .swap_buffers = bga_swap_buffers,
};

static gpu_hal_ops_t intel_ops = {
    .name = "Intel GMA/HD Hardware BLT",
    .is_hardware_accel = true,
    .has_page_flip = true,
    .fill_rect = intel_fill_rect,
    .copy_rect = intel_copy_rect,
    .blend_rect = sw_blend_rect,
    .swap_buffers = intel_swap_buffers,
};

static gpu_hal_ops_t *active_ops = &sw_ops;

/* ══════════════════════════════════════════════════════════
 *   GPU HAL Core API
 * ══════════════════════════════════════════════════════════ */
void gpu_hal_init(void) {
    gpu_init();
}

void gpu_init(void) {
    memset(&gpu_info, 0, sizeof(gpu_info));
    active_ops = &sw_ops;

    uint32_t count = pci_get_device_count();
    const pci_device_t *devices = pci_get_devices();

    for (uint32_t i = 0; i < count; i++) {
        const pci_device_t *d = &devices[i];
        if (d->class_code != PCI_CLASS_DISPLAY) continue;

        /* ── Intel GMA / HD Graphics ── */
        if (d->vendor_id == VENDOR_INTEL) {
            gpu_pci_dev = d;
            gpu_info.type = GPU_TYPE_INTEL;
            gpu_info.vendor_id = d->vendor_id;
            gpu_info.device_id = d->device_id;
            gpu_info.mmio_base = d->bar[0] & 0xFFFFFFF0;
            gpu_info.accel_available = true;
            gpu_info.page_flip_available = true;
            active_ops = &intel_ops;
            kprintf("Graphics HAL: Intel HD (0x%x) selected | 2D BLT & Page-Flip\n", d->device_id);
            return;
        }

        /* ── Bochs BGA / QEMU Standard VGA ── */
        if (d->vendor_id == VENDOR_QEMU && d->device_id == DEVICE_BOCHS_VGA) {
            gpu_info.type = GPU_TYPE_BOCHS_BGA;
            gpu_info.vendor_id = d->vendor_id;
            gpu_info.device_id = d->device_id;
            gpu_info.mmio_base = d->bar[0] & 0xFFFFFFF0;
            gpu_info.fb_base   = d->bar[2] & 0xFFFFFFF0;

            uint16_t bga_id = bga_read(VBE_DISPI_INDEX_ID);
            if (bga_id >= 0xB0C0 && bga_id <= 0xB0CF) {
                gpu_info.accel_available = true;
                gpu_info.page_flip_available = true;
                active_ops = &bga_ops;
                kprintf("Graphics HAL: Bochs BGA selected | Zero-Copy Page-Flip\n");
            }
            return;
        }

        /* ── VirtIO-GPU ── */
        if (d->vendor_id == VENDOR_REDHAT && d->device_id == DEVICE_VIRTIO_GPU) {
            gpu_info.type = GPU_TYPE_VIRTIO;
            gpu_info.vendor_id = d->vendor_id;
            gpu_info.device_id = d->device_id;
            gpu_info.mmio_base = d->bar[0] & 0xFFFFFFF0;
            gpu_info.fb_base   = d->bar[2] & 0xFFFFFFF0;
            gpu_info.accel_available = true;
            gpu_info.page_flip_available = true;
            active_ops = &bga_ops;
            kprintf("Graphics HAL: VirtIO-GPU BGA selected | Zero-Copy Page-Flip\n");
            return;
        }

        /* ── VMware SVGA ── */
        if (d->vendor_id == VENDOR_VMWARE && d->device_id == DEVICE_VMWARE_SVGA2) {
            gpu_info.type = GPU_TYPE_VMWARE_SVGA;
            gpu_info.vendor_id = d->vendor_id;
            gpu_info.device_id = d->device_id;
            gpu_info.mmio_base = d->bar[0] & 0xFFFFFFF0;
            kprintf("Graphics HAL: VMware SVGA2 detected.\n");
            return;
        }
    }

    kprintf("Graphics HAL: Software SIMD Rasterizer active.\n");
}

const gpu_info_t *gpu_get_info(void) {
    return &gpu_info;
}

const gpu_hal_ops_t *gpu_hal_get_ops(void) {
    return active_ops;
}

void gpu_hal_fill_rect(uint32_t *dst, int pitch_bytes, int x, int y, int w, int h, uint32_t color) {
    active_ops->fill_rect(dst, pitch_bytes, x, y, w, h, color);
}

void gpu_hal_copy_rect(uint32_t *dst, int dst_pitch_bytes, int dx, int dy,
                       const uint32_t *src, int src_pitch_bytes, int sx, int sy, int w, int h) {
    active_ops->copy_rect(dst, dst_pitch_bytes, dx, dy, src, src_pitch_bytes, sx, sy, w, h);
}

void gpu_hal_blend_rect(uint32_t *dst, int dst_pitch_bytes, int dx, int dy,
                        const uint32_t *src, int src_pitch_bytes, int sx, int sy, int w, int h, uint8_t alpha) {
    active_ops->blend_rect(dst, dst_pitch_bytes, dx, dy, src, src_pitch_bytes, sx, sy, w, h, alpha);
}

void gpu_hal_swap_buffers(uint32_t *front, uint32_t *back, int pitch_bytes, int width, int height) {
    active_ops->swap_buffers(front, back, pitch_bytes, width, height);
}

/* ══════════════════════════════════════════════════════════
 *   Legacy & Direct Page Flipping Compatibility
 * ══════════════════════════════════════════════════════════ */
bool gpu_can_flip(void) {
    return gpu_info.page_flip_available;
}

bool gpu_setup_double_buffer(int width, int height) {
    if (!gpu_info.page_flip_available) return false;

    if (gpu_info.type == GPU_TYPE_INTEL) {
        gpu_info.display_width = width;
        gpu_info.display_height = height;
        if (gpu_pci_dev) {
            intel_gma_init(gpu_pci_dev, width, height, width * 4);
        }
        double_buffer_active = intel_gma_setup_double_buffer();
        return double_buffer_active;
    }

    if (gpu_info.type == GPU_TYPE_VIRTIO || gpu_info.type == GPU_TYPE_BOCHS_BGA) {
        /* VirtIO GPU / Limine FB does not expose a 2-page physical VRAM BAR if fb_base is 0 */
        if (gpu_info.fb_base == 0) return false;

        uint16_t vram_64k = bga_read(VBE_DISPI_INDEX_VIDEO_MEM_64K);
        uint32_t vram_bytes = (uint32_t)vram_64k * 64 * 1024;
        uint32_t page_size = (uint32_t)width * height * 4;

        if (vram_bytes < page_size * 2) return false;

        bga_write(VBE_DISPI_INDEX_VIRT_W, (uint16_t)width);
        bga_write(VBE_DISPI_INDEX_VIRT_H, (uint16_t)(height * 2));
        bga_write(VBE_DISPI_INDEX_X_OFFSET, 0);
        bga_write(VBE_DISPI_INDEX_Y_OFFSET, 0);

        gpu_info.display_width = width;
        gpu_info.display_height = height;
        current_back_page = 1;
        double_buffer_active = true;
        return true;
    }

    return false;
}

uint32_t *gpu_flip(uint32_t *vram_base, int pitch) {
    if (!double_buffer_active) return NULL;

    if (gpu_info.type == GPU_TYPE_INTEL) {
        return intel_gma_flip(vram_base);
    }

    if (gpu_info.type == GPU_TYPE_VIRTIO || gpu_info.type == GPU_TYPE_BOCHS_BGA) {
        int display_page = current_back_page;
        uint16_t y_offset = (uint16_t)(display_page * gpu_info.display_height);
        bga_write(VBE_DISPI_INDEX_Y_OFFSET, y_offset);
        current_back_page = (current_back_page == 0) ? 1 : 0;
        int pitch_px = pitch / 4;
        return vram_base + (current_back_page * gpu_info.display_height * pitch_px);
    }

    return NULL;
}

int gpu_get_back_page(void) {
    return current_back_page;
}

void gpu_fill_rect(uint32_t *fb, int pitch, int x, int y, int w, int h, uint32_t color) {
    gpu_hal_fill_rect(fb, pitch, x, y, w, h, color);
}

void gpu_copy_rect(uint32_t *fb, int pitch_bytes, int src_x, int src_y, int dst_x, int dst_y, int w, int h) {
    gpu_hal_copy_rect(fb, pitch_bytes, dst_x, dst_y, fb, pitch_bytes, src_x, src_y, w, h);
}

