/*
 * Zarqaa OS - xHCI (USB 3.x) host controller driver
 * Root-port enumeration, HID boot keyboard/mouse and report-descriptor
 * based pointers (usb-tablet). Polled; no external hub support.
 */

#include "drivers/usb/xhci.h"
#include "drivers/usb/usb_hid.h"
#include "drivers/pit.h"
#include "kernel/pci.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/driver.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"

extern uint64_t hhdm_offset;
extern uint64_t *kernel_pml4;
extern void kprintf(const char *fmt, ...);

#define RING_TRBS      256
#define MAX_SLOTS      64
#define MAX_PORTS      32
#define MAX_HID        4

#define PORTSC_CCS     (1u << 0)
#define PORTSC_PED     (1u << 1)
#define PORTSC_PR      (1u << 4)
#define PORTSC_PP      (1u << 9)
#define PORTSC_CSC     (1u << 17)
#define PORTSC_PRC     (1u << 21)
#define PORTSC_CHANGE  0x00FE0000u
#define PORTSC_KEEP    0x0E01C3E0u
#define PORTSC_SPEED(v) (((v) >> 10) & 0xF)

#define TRB_SETUP_STAGE_T  2
#define TRB_SET_TR_DEQ     16
#define TRB_C              (1u << 0)
#define TRB_TC             (1u << 1)
#define TRB_ISP            (1u << 2)
#define TRB_IOC            (1u << 5)
#define TRB_IDT            (1u << 6)

#define CC_SUCCESS         1
#define CC_SHORT           13

#define SPEED_LS 2
#define SPEED_FS 1
#define SPEED_HS 3
#define SPEED_SS 4

static volatile uint8_t *cap_regs, *op_regs, *rt_regs;
static volatile uint32_t *db_regs;
static uint8_t max_slots, num_ports;
static uint32_t ctx_size = 32;
static bool xhci_ready;

struct ring {
    struct xhci_trb *virt;
    uint64_t phys;
    uint32_t enq;
    uint32_t cycle;
};

static uint64_t *dcbaa_virt;
static struct ring cmd_ring;
static struct xhci_trb *evt_ring;
static uint64_t evt_ring_phys;
static uint32_t evt_deq, evt_cycle = 1;

enum { HID_NONE, HID_KBD, HID_MOUSE_BOOT, HID_POINTER };

struct hid_field { int off; int size; int min, max; bool sgn, rel, valid; };

struct hid_ptr {
    uint8_t rid;
    struct hid_field btn, x, y, wheel;
};

struct hid_iface {
    int type;
    uint8_t iface, dci;
    uint16_t len;
    struct ring ring;
    uint8_t *buf;
    uint64_t buf_phys;
    uint8_t prev_mod, prev_keys[6];
    struct hid_ptr ptr;
    bool halted;
};

struct usb_dev {
    bool used;
    uint8_t slot, port, speed;
    uint8_t *dctx, *ictx, *data;
    uint64_t dctx_phys, ictx_phys, data_phys;
    struct ring ep0;
    struct hid_iface hid[MAX_HID];
    int nhid;
};

static struct usb_dev devs[MAX_SLOTS + 1];
static uint8_t port_slot[MAX_PORTS + 1];
static bool port_pending[MAX_PORTS + 1];
static bool port_gone[MAX_PORTS + 1];

static volatile bool cmd_done, ctl_done;
static volatile uint32_t cmd_cc, ctl_cc;
static volatile uint8_t cmd_slot, ctl_slot;
static bool in_poll;

static uint32_t xhci_read32(volatile uint8_t *base, uint32_t offset) {
    return *(volatile uint32_t *)(base + offset);
}

static void xhci_write32(volatile uint8_t *base, uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(base + offset) = val;
}

static void xhci_write64(volatile uint8_t *base, uint32_t offset, uint64_t val) {
    *(volatile uint64_t *)(base + offset) = val;
}

static inline void mb(void) { __asm__ volatile("mfence" ::: "memory"); }

static uint8_t *alloc_dma_page(uint64_t *phys) {
    uint64_t p = pmm_alloc_low32();
    if (!p) return NULL;
    uint8_t *v = (uint8_t *)(p + hhdm_offset);
    memset(v, 0, PAGE_SIZE);
    *phys = p;
    return v;
}

/* ---- Rings ---- */

static bool ring_init(struct ring *r) {
    if (!r->virt) {
        r->virt = (struct xhci_trb *)alloc_dma_page(&r->phys);
        if (!r->virt) return false;
    } else {
        memset(r->virt, 0, PAGE_SIZE);
    }
    r->enq = 0;
    r->cycle = 1;
    struct xhci_trb *link = &r->virt[RING_TRBS - 1];
    link->param1 = (uint32_t)r->phys;
    link->param2 = (uint32_t)(r->phys >> 32);
    link->control = (TRB_LINK << 10) | TRB_TC;
    return true;
}

/* Returns the physical address of the enqueued TRB. */
static uint64_t ring_push(struct ring *r, uint32_t p1, uint32_t p2, uint32_t status, uint32_t control) {
    struct xhci_trb *t = &r->virt[r->enq];
    uint64_t phys = r->phys + r->enq * sizeof(*t);
    t->param1 = p1;
    t->param2 = p2;
    t->status = status;
    mb();
    t->control = control | r->cycle;
    mb();
    if (++r->enq == RING_TRBS - 1) {
        // Hand the Link TRB to the controller with the current cycle, then toggle.
        r->virt[RING_TRBS - 1].control = (TRB_LINK << 10) | TRB_TC | r->cycle;
        mb();
        r->enq = 0;
        r->cycle ^= 1;
    }
    return phys;
}

static void ring_doorbell(uint8_t slot, uint8_t target) {
    xhci_write32((volatile uint8_t *)db_regs, slot * 4, target);
}

/* ---- Event processing ---- */

static void hid_report(struct usb_dev *d, struct hid_iface *h);
static void xhci_handle_event(struct xhci_trb *e);

static void process_events(void) {
    for (;;) {
        struct xhci_trb *e = &evt_ring[evt_deq];
        if ((e->control & 1) != evt_cycle) break;
        mb();
        struct xhci_trb copy = *e;
        if (++evt_deq == RING_TRBS) {
            evt_deq = 0;
            evt_cycle ^= 1;
        }
        xhci_handle_event(&copy);
        xhci_write64(rt_regs + XHCI_RT_IR0, XHCI_IR_ERDP, (evt_ring_phys + evt_deq * sizeof(struct xhci_trb)) | (1 << 3));
    }
}

static bool wait_flag(volatile bool *flag, uint32_t ms) {
    uint64_t end = pit_get_ticks() + ms;
    while (!*flag) {
        process_events();
        if (*flag) break;
        if (pit_get_ticks() > end) return false;
        __asm__ volatile("pause");
    }
    return true;
}

static uint32_t run_command(uint32_t p1, uint32_t p2, uint32_t control, uint8_t *slot_out) {
    cmd_done = false;
    ring_push(&cmd_ring, p1, p2, 0, control);
    ring_doorbell(0, 0);
    if (!wait_flag(&cmd_done, 1000)) return 0;
    if (slot_out) *slot_out = cmd_slot;
    return cmd_cc;
}

/* ---- Context helpers (CSZ-aware) ---- */

static uint32_t *ictx_slot(struct usb_dev *d) { return (uint32_t *)(d->ictx + ctx_size); }
static uint32_t *ictx_ep(struct usb_dev *d, int dci) { return (uint32_t *)(d->ictx + (dci + 1) * ctx_size); }
static uint32_t *ictx_ctrl(struct usb_dev *d) { return (uint32_t *)d->ictx; }

static void set_ep_ctx(uint32_t *ep, int type, int maxpkt, int interval, uint64_t deq, int avg) {
    ep[0] = (uint32_t)interval << 16;
    ep[1] = (3u << 1) | ((uint32_t)type << 3) | ((uint32_t)maxpkt << 16);
    ep[2] = (uint32_t)deq | 1;
    ep[3] = (uint32_t)(deq >> 32);
    ep[4] = (uint32_t)avg | ((uint32_t)avg << 16);
}

/* ---- Control transfers on EP0 ---- */

static int ctrl_xfer(struct usb_dev *d, uint8_t rt, uint8_t req, uint16_t val, uint16_t idx, uint16_t len) {
    bool in = rt & 0x80;
    uint32_t s1 = rt | ((uint32_t)req << 8) | ((uint32_t)val << 16);
    uint32_t s2 = idx | ((uint32_t)len << 16);
    uint32_t trt = len ? (in ? 3 : 2) : 0;

    ctl_done = false;
    ring_push(&d->ep0, s1, s2, 8, (TRB_SETUP_STAGE_T << 10) | TRB_IDT | (trt << 16));
    if (len)
        ring_push(&d->ep0, (uint32_t)d->data_phys, (uint32_t)(d->data_phys >> 32), len,
                  (TRB_DATA << 10) | (in ? (1u << 16) : 0));
    ring_push(&d->ep0, 0, 0, 0, (TRB_STATUS << 10) | TRB_IOC | ((len && in) ? 0 : (1u << 16)));
    ring_doorbell(d->slot, 1);
    if (!wait_flag(&ctl_done, 1500)) {
        kprintf("[xHCI] slot %d: control transfer timeout (req 0x%x)\n", d->slot, req);
        return -1;
    }
    if (ctl_cc != CC_SUCCESS && ctl_cc != CC_SHORT) {
        kprintf("[xHCI] slot %d: control transfer cc=%d (req 0x%x)\n", d->slot, (int)ctl_cc, req);
        return -1;
    }
    return 0;
}

static int get_descriptor(struct usb_dev *d, uint8_t rt, uint8_t type, uint8_t idx, uint16_t wIndex, uint16_t len) {
    return ctrl_xfer(d, rt, 6, ((uint16_t)type << 8) | idx, wIndex, len);
}

static void xhci_bios_handoff(volatile uint8_t *cap_base) {
    uint32_t hccparams1 = xhci_read32(cap_base, XHCI_CAP_HCCPARAMS1);
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFF;
    
    if (xecp == 0) return;
    
    volatile uint32_t *ext_cap = (volatile uint32_t *)(cap_base + (xecp << 2));
    
    while (ext_cap) {
        uint32_t cap = *ext_cap;
        uint8_t cap_id = cap & 0xFF;
        
        if (cap_id == 1) { // USB Legacy Support Capability (USBLEGSUP)
            if (cap & (1 << 16)) { // BIOS Owned Semaphore
                kprintf("[xHCI] BIOS owns USB controller. Requesting OS handoff...\n");
                
                // Request OS Ownership (bit 24)
                *ext_cap = cap | (1 << 24);
                
                // Wait for BIOS to clear BIOS Owned Semaphore (bit 16)
                int timeout = 0;
                while ((*ext_cap & (1 << 16)) && timeout < 1000) {
                    timeout++;
                    for (volatile int i = 0; i < 10000; i++) __asm__ volatile("pause");
                }
                
                if (timeout >= 1000) {
                    kprintf("[xHCI] WARN: BIOS handoff timed out! Forcing OS ownership...\n");
                    *ext_cap = (*ext_cap & ~(1 << 16)) | (1 << 24);
                } else {
                    kprintf("[xHCI] BIOS handoff completed successfully.\n");
                }
            } else {
                *ext_cap = cap | (1 << 24);
            }
            
            // Disable all BIOS SMIs in USBLEGCTLSTS (offset +4)
            volatile uint32_t *legctlsts = ext_cap + 1;
            uint32_t val = *legctlsts;
            val &= 0xFFFF0000; // Clear SMI enable bits (bits 0:15)
            val |= 0xE0000000; // Clear SMI status bits (bits 29:31)
            *legctlsts = val;
            break;
        }
        
        uint8_t next = (cap >> 8) & 0xFF;
        if (!next) break;
        ext_cap = (volatile uint32_t *)((uint8_t *)ext_cap + (next << 2));
    }
}


static const uint8_t kb_usb_to_ps2[256] = {
    // 0x00 - 0x03: Reserved
    [0x04] = 0x1E, // 'a'
    [0x05] = 0x30, // 'b'
    [0x06] = 0x2E, // 'c'
    [0x07] = 0x20, // 'd'
    [0x08] = 0x12, // 'e'
    [0x09] = 0x21, // 'f'
    [0x0A] = 0x22, // 'g'
    [0x0B] = 0x23, // 'h'
    [0x0C] = 0x17, // 'i'
    [0x0D] = 0x24, // 'j'
    [0x0E] = 0x25, // 'k'
    [0x0F] = 0x26, // 'l'
    [0x10] = 0x32, // 'm'
    [0x11] = 0x31, // 'n'
    [0x12] = 0x18, // 'o'
    [0x13] = 0x19, // 'p'
    [0x14] = 0x10, // 'q'
    [0x15] = 0x13, // 'r'
    [0x16] = 0x1F, // 's'
    [0x17] = 0x14, // 't'
    [0x18] = 0x16, // 'u'
    [0x19] = 0x2F, // 'v'
    [0x1A] = 0x11, // 'w'
    [0x1B] = 0x2D, // 'x'
    [0x1C] = 0x15, // 'y'
    [0x1D] = 0x2C, // 'z'
    [0x1E] = 0x02, // '1'
    [0x1F] = 0x03, // '2'
    [0x20] = 0x04, // '3'
    [0x21] = 0x05, // '4'
    [0x22] = 0x06, // '5'
    [0x23] = 0x07, // '6'
    [0x24] = 0x08, // '7'
    [0x25] = 0x09, // '8'
    [0x26] = 0x0A, // '9'
    [0x27] = 0x0B, // '0'
    [0x28] = 0x1C, // Enter
    [0x29] = 0x01, // Escape
    [0x2A] = 0x0E, // Backspace
    [0x2B] = 0x0F, // Tab
    [0x2C] = 0x39, // Space
    [0x2D] = 0x0C, // -
    [0x2E] = 0x0D, // =
    [0x2F] = 0x1A, // [
    [0x30] = 0x1B, // ]
    [0x31] = 0x2B, // backslash
    [0x33] = 0x27, // ;
    [0x34] = 0x28, // '
    [0x35] = 0x29, // `
    [0x36] = 0x33, // ,
    [0x37] = 0x34, // .
    [0x38] = 0x35, // /
    [0x39] = 0x3A, // CapsLock
    [0x3A] = 0x3B, // F1
    [0x3B] = 0x3C, // F2
    [0x3C] = 0x3D, // F3
    [0x3D] = 0x3E, // F4
    [0x3E] = 0x3F, // F5
    [0x3F] = 0x40, // F6
    [0x40] = 0x41, // F7
    [0x41] = 0x42, // F8
    [0x42] = 0x43, // F9
    [0x43] = 0x44, // F10
    [0x44] = 0x57, // F11
    [0x45] = 0x58, // F12
    [0x49] = 0x52, // Insert
    [0x4A] = 0x47, // Home
    [0x4B] = 0x49, // Page Up
    [0x4C] = 0x53, // Delete
    [0x4D] = 0x4F, // End
    [0x4E] = 0x51, // Page Down
    [0x4F] = 0x4D, // Right Arrow
    [0x50] = 0x4B, // Left Arrow
    [0x51] = 0x50, // Down Arrow
    [0x52] = 0x48, // Up Arrow
};

/* ---- HID report handling ---- */

static void queue_hid(struct usb_dev *d, struct hid_iface *h) {
    memset(h->buf, 0, h->len);
    ring_push(&h->ring, (uint32_t)h->buf_phys, (uint32_t)(h->buf_phys >> 32), h->len,
              (TRB_NORMAL << 10) | TRB_IOC | TRB_ISP);
    ring_doorbell(d->slot, h->dci);
}

static int32_t get_bits(const uint8_t *buf, int len, int off, int size, bool sgn) {
    if (size <= 0 || size > 32 || off + size > len * 8) return 0;
    uint32_t v = 0;
    for (int i = 0; i < size; i++)
        if (buf[(off + i) >> 3] & (1 << ((off + i) & 7))) v |= 1u << i;
    if (sgn && size < 32 && (v & (1u << (size - 1)))) v |= ~0u << size;
    return (int32_t)v;
}

static void kbd_report(struct hid_iface *h) {
    static const struct { uint8_t bit, sc; } mods[] = {
        {0x01, 0x1D}, {0x02, 0x2A}, {0x04, 0x38}, {0x10, 0x1D}, {0x20, 0x36}, {0x40, 0x38},
    };
    const uint8_t *buf = h->buf;
    uint8_t mod = buf[0];

    for (unsigned i = 0; i < sizeof(mods) / sizeof(mods[0]); i++)
        if ((mod & mods[i].bit) != (h->prev_mod & mods[i].bit))
            input_keyboard_push(mods[i].sc, !(mod & mods[i].bit));
    h->prev_mod = mod;

    if (buf[2] == 1) return; // ErrorRollOver

    for (int p = 0; p < 6; p++) {
        uint8_t k = h->prev_keys[p];
        if (k < 4) continue;
        bool held = false;
        for (int c = 2; c < 8; c++) if (buf[c] == k) { held = true; break; }
        if (!held && kb_usb_to_ps2[k]) input_keyboard_push(kb_usb_to_ps2[k], true);
    }
    for (int c = 2; c < 8; c++) {
        uint8_t k = buf[c];
        if (k < 4) continue;
        bool had = false;
        for (int p = 0; p < 6; p++) if (h->prev_keys[p] == k) { had = true; break; }
        if (!had && kb_usb_to_ps2[k]) input_keyboard_push(kb_usb_to_ps2[k], false);
    }
    for (int i = 0; i < 6; i++) h->prev_keys[i] = buf[2 + i];
}

static void pointer_report(struct hid_iface *h) {
    const uint8_t *buf = h->buf;
    struct hid_ptr *p = &h->ptr;
    int len = h->len;
    if (p->rid && buf[0] != p->rid) return;

    int32_t b = p->btn.valid ? get_bits(buf, len, p->btn.off, p->btn.size > 8 ? 8 : p->btn.size, false) : 0;
    int32_t x = get_bits(buf, len, p->x.off, p->x.size, p->x.sgn);
    int32_t y = get_bits(buf, len, p->y.off, p->y.size, p->y.sgn);
    int32_t w = p->wheel.valid ? get_bits(buf, len, p->wheel.off, p->wheel.size, true) : 0;
    bool l = b & 1, r = b & 2, m = b & 4;

    if (p->x.rel) {
        input_mouse_push(x, -y, w, l, r, m);
    } else {
        int range_x = p->x.max - p->x.min, range_y = p->y.max - p->y.min;
        if (range_x <= 0 || range_y <= 0) return;
        int64_t nx = (int64_t)(x - p->x.min) * 32767 / range_x;
        int64_t ny = (int64_t)(y - p->y.min) * 32767 / range_y;
        input_mouse_push_abs((int)nx, (int)ny, w, l, r, m);
    }
}

static void hid_report(struct usb_dev *d, struct hid_iface *h) {
    (void)d;
    if (h->type == HID_KBD) {
        kbd_report(h);
    } else if (h->type == HID_MOUSE_BOOT) {
        const int8_t *s = (const int8_t *)h->buf;
        input_mouse_push(s[1], -s[2], h->len >= 4 ? s[3] : 0, s[0] & 1, s[0] & 2, s[0] & 4);
    } else if (h->type == HID_POINTER) {
        pointer_report(h);
    }
}

/* Minimal HID report descriptor walk: finds buttons, X, Y and wheel input fields. */
static void parse_report_desc(const uint8_t *d, int len, struct hid_ptr *out) {
    int off_by_id[16] = {0};
    int usages[16], nusage = 0, umin = 0, umax = 0;
    int page = 0, rsize = 0, rcount = 0, lmin = 0, lmax = 0;
    uint8_t rid = 0;
    bool found = false;

    memset(out, 0, sizeof(*out));
    for (int i = 0; i < len;) {
        uint8_t pfx = d[i++];
        if (pfx == 0xFE) { i += 1 + (i < len ? d[i] : 0); continue; }
        int sz = pfx & 3; if (sz == 3) sz = 4;
        int type = (pfx >> 2) & 3, tag = pfx >> 4;
        if (i + sz > len) break;
        uint32_t u = 0;
        for (int k = 0; k < sz; k++) u |= (uint32_t)d[i + k] << (8 * k);
        int32_t sv = sz == 1 ? (int8_t)u : sz == 2 ? (int16_t)u : (int32_t)u;
        i += sz;

        if (type == 1) {
            switch (tag) {
            case 0: page = u; break;
            case 1: lmin = sv; break;
            case 2: lmax = lmin >= 0 ? (int)u : sv; break;
            case 7: rsize = u; break;
            case 8: rid = u & 0xF; if (!off_by_id[rid]) off_by_id[rid] = rid ? 8 : 0; break;
            case 9: rcount = u; break;
            }
        } else if (type == 2) {
            if (tag == 0 && nusage < 16) usages[nusage++] = u;
            else if (tag == 1) umin = u;
            else if (tag == 2) umax = u;
        } else if (type == 0) {
            if (tag == 8) { // Input
                bool constant = u & 1, var = u & 2, rel = u & 4;
                int off = off_by_id[rid];
                bool mine = !found || out->rid == rid;
                if (!constant && var && mine) {
                    for (int n = 0; n < rcount; n++) {
                        int us = nusage ? usages[n < nusage ? n : nusage - 1] : (umin + n <= umax ? umin + n : -1);
                        struct hid_field f = { off + n * rsize, rsize, lmin, lmax, lmin < 0, rel, true };
                        if (page == 9) {
                            if (!out->btn.valid) { out->btn = f; out->btn.size = rcount; out->rid = rid; }
                            break;
                        } else if (page == 1 && us == 0x30 && !out->x.valid) { out->x = f; out->rid = rid; }
                        else if (page == 1 && us == 0x31 && !out->y.valid) { out->y = f; out->rid = rid; }
                        else if (page == 1 && us == 0x38 && !out->wheel.valid) { out->wheel = f; out->rid = rid; }
                    }
                    if (out->x.valid && out->y.valid) found = true;
                }
                off_by_id[rid] += rcount * rsize;
            }
            if (tag == 8 || tag == 9 || tag == 11 || tag == 10 || tag == 12) { nusage = 0; umin = umax = 0; }
        }
    }
    if (!(out->x.valid && out->y.valid)) out->x.valid = out->y.valid = false;
}

/* ---- Event dispatch ---- */

static void xhci_handle_event(struct xhci_trb *e) {
    uint32_t type = (e->control >> 10) & 0x3F;
    if (type == TRB_CMD_COMPLETION) {
        cmd_cc = e->status >> 24;
        cmd_slot = e->control >> 24;
        cmd_done = true;
    } else if (type == TRB_TRANSFER_EVENT) {
        uint8_t slot = e->control >> 24;
        uint8_t dci = (e->control >> 16) & 0x1F;
        uint32_t cc = e->status >> 24;
        if (slot == 0 || slot > MAX_SLOTS || !devs[slot].used) return;
        struct usb_dev *d = &devs[slot];
        if (dci == 1) {
            ctl_cc = cc;
            ctl_slot = slot;
            ctl_done = true;
            return;
        }
        for (int i = 0; i < d->nhid; i++) {
            struct hid_iface *h = &d->hid[i];
            if (h->dci != dci || h->type == HID_NONE) continue;
            if (cc == CC_SUCCESS || cc == CC_SHORT) {
                hid_report(d, h);
                queue_hid(d, h);
            } else {
                h->halted = true;
            }
        }
    } else if (type == TRB_PORT_STATUS) {
        uint32_t port = e->param1 >> 24;
        if (port == 0 || port > num_ports || port > MAX_PORTS) return;
        uint32_t off = XHCI_OP_PORTSC_BASE + (port - 1) * 16;
        uint32_t v = xhci_read32(op_regs, off);
        if (v & PORTSC_CSC) {
            if (v & PORTSC_CCS) port_pending[port] = true;
            else port_gone[port] = true;
        }
        xhci_write32(op_regs, off, (v & PORTSC_KEEP) | (v & PORTSC_CHANGE));
    }
}

/* ---- Enumeration ---- */

static bool port_reset(uint8_t port, uint32_t *portsc_out) {
    uint32_t off = XHCI_OP_PORTSC_BASE + (port - 1) * 16;
    uint32_t v = xhci_read32(op_regs, off);
    if (!(v & PORTSC_CCS)) return false;

    // SuperSpeed ports train on their own; resetting an enabled one is unnecessary.
    if (!(PORTSC_SPEED(v) >= SPEED_SS && (v & PORTSC_PED))) {
        xhci_write32(op_regs, off, (v & PORTSC_KEEP) | PORTSC_CHANGE | PORTSC_PR);
        uint64_t end = pit_get_ticks() + 500;
        while (!(xhci_read32(op_regs, off) & PORTSC_PRC)) {
            if (pit_get_ticks() > end) { kprintf("[xHCI] Port %d: reset timeout\n", port); return false; }
            pit_sleep_ms(1);
        }
    }
    v = xhci_read32(op_regs, off);
    xhci_write32(op_regs, off, (v & PORTSC_KEEP) | PORTSC_CHANGE);
    pit_sleep_ms(20);
    v = xhci_read32(op_regs, off);
    if (!(v & PORTSC_PED) || !(v & PORTSC_CCS)) {
        kprintf("[xHCI] Port %d: not enabled after reset (PORTSC=0x%x)\n", port, v);
        return false;
    }
    *portsc_out = v;
    return true;
}

static void teardown_slot(uint8_t slot) {
    struct usb_dev *d = &devs[slot];
    if (!d->used) return;
    kprintf("[xHCI] Slot %d (port %d) removed\n", slot, d->port);
    d->used = false;
    port_slot[d->port] = 0;
    for (int i = 0; i < d->nhid; i++) d->hid[i].type = HID_NONE;
    d->nhid = 0;
    run_command(0, 0, (TRB_TYPE_DISABLE_SLOT_CMD << 10) | ((uint32_t)slot << 24), NULL);
    dcbaa_virt[slot] = 0;
}

static int ep0_max_packet(int speed) {
    return speed >= SPEED_SS ? 512 : speed == SPEED_HS ? 64 : 8;
}

static int hid_interval(int speed, int bInterval) {
    if (speed >= SPEED_HS) {
        int iv = bInterval - 1;
        return iv < 0 ? 0 : iv > 15 ? 15 : iv;
    }
    int n = 0, v = (bInterval ? bInterval : 1) * 8;
    while ((2 << n) <= v) n++;
    return n < 3 ? 3 : n > 10 ? 10 : n;
}

struct hid_cand {
    uint8_t iface, sub, proto, ep, interval;
    uint16_t mps, rdesc_len;
};

static void setup_hid(struct usb_dev *d, struct hid_cand *c, int n) {
    int maxdci = 1;
    d->nhid = 0;
    for (int i = 0; i < n && d->nhid < MAX_HID; i++) {
        struct hid_iface *h = &d->hid[d->nhid];
        h->type = HID_NONE;
        h->iface = c[i].iface;
        h->dci = (c[i].ep & 0xF) * 2 + 1;
        h->len = c[i].mps > 64 ? 64 : c[i].mps;
        h->buf = d->data + 2048 + d->nhid * 256;
        h->buf_phys = d->data_phys + 2048 + d->nhid * 256;
        h->halted = false;
        h->prev_mod = 0;
        memset(h->prev_keys, 0, sizeof(h->prev_keys));
        if (!ring_init(&h->ring)) return;
        if (h->dci > maxdci) maxdci = h->dci;
        d->nhid++;
    }
    if (!d->nhid) return;

    memset(d->ictx, 0, PAGE_SIZE);
    uint32_t *slot = ictx_slot(d);
    slot[0] = ((uint32_t)d->speed << 20) | ((uint32_t)maxdci << 27);
    slot[1] = (uint32_t)d->port << 16;
    ictx_ctrl(d)[1] = 1;
    for (int i = 0; i < d->nhid; i++) {
        struct hid_iface *h = &d->hid[i];
        ictx_ctrl(d)[1] |= 1u << h->dci;
        set_ep_ctx(ictx_ep(d, h->dci), 7, c[i].mps & 0x7FF, hid_interval(d->speed, c[i].interval), h->ring.phys, c[i].mps & 0x7FF);
    }
    uint32_t cc = run_command((uint32_t)d->ictx_phys, (uint32_t)(d->ictx_phys >> 32),
                              (TRB_TYPE_CONFIGURE_EP_CMD << 10) | ((uint32_t)d->slot << 24), NULL);
    if (cc != CC_SUCCESS) {
        kprintf("[xHCI] slot %d: Configure Endpoint failed (cc=%d)\n", d->slot, (int)cc);
        d->nhid = 0;
        return;
    }
}

static void classify_hid(struct usb_dev *d, struct hid_cand *c) {
    for (int i = 0; i < d->nhid; i++) {
        struct hid_iface *h = &d->hid[i];
        struct hid_cand *k = &c[i];
        if (k->sub == 1 && (k->proto == 1 || k->proto == 2)) {
            ctrl_xfer(d, 0x21, 0x0B, 0, k->iface, 0); // SET_PROTOCOL: boot
            ctrl_xfer(d, 0x21, 0x0A, 0, k->iface, 0); // SET_IDLE (may stall; ignored)
            h->type = k->proto == 1 ? HID_KBD : HID_MOUSE_BOOT;
            if (h->type == HID_KBD) h->len = 8;
        } else if (k->rdesc_len) {
            uint16_t rl = k->rdesc_len > 2048 ? 2048 : k->rdesc_len;
            if (get_descriptor(d, 0x81, 0x22, 0, k->iface, rl) < 0) continue;
            parse_report_desc(d->data, rl, &h->ptr);
            if (h->ptr.x.valid && h->ptr.y.valid) h->type = HID_POINTER;
        }
        const char *names[] = { "ignored", "keyboard", "boot mouse", h->ptr.x.rel ? "relative pointer" : "tablet (absolute)" };
        kprintf("[xHCI] slot %d iface %d ep 0x%x: HID %s (len %d, interval %d)\n", d->slot, h->iface,
                k->ep, names[h->type], h->len, k->interval);
        if (h->type != HID_NONE) queue_hid(d, h);
    }
}

static void enumerate_port(uint8_t port) {
    uint32_t portsc;
    if (!port_reset(port, &portsc)) return;
    uint8_t speed = PORTSC_SPEED(portsc);
    kprintf("[xHCI] Port %d: device connected, speed=%d\n", port, speed);

    uint8_t slot = 0;
    uint32_t cc = run_command(0, 0, TRB_TYPE_ENABLE_SLOT_CMD << 10, &slot);
    if (cc != CC_SUCCESS || slot == 0 || slot > MAX_SLOTS || slot > max_slots) {
        kprintf("[xHCI] Port %d: Enable Slot failed (cc=%d slot=%d)\n", port, (int)cc, slot);
        return;
    }
    struct usb_dev *d = &devs[slot];
    d->slot = slot; d->port = port; d->speed = speed;
    if (!d->dctx) {
        d->dctx = alloc_dma_page(&d->dctx_phys);
        d->ictx = alloc_dma_page(&d->ictx_phys);
        d->data = alloc_dma_page(&d->data_phys);
    }
    if (!d->dctx || !d->ictx || !d->data || !ring_init(&d->ep0)) {
        kprintf("[xHCI] Slot %d: out of DMA memory\n", slot);
        return;
    }
    memset(d->dctx, 0, PAGE_SIZE);
    memset(d->ictx, 0, PAGE_SIZE);
    dcbaa_virt[slot] = d->dctx_phys;
    d->used = true;
    port_slot[port] = slot;
    d->nhid = 0;

    int mps = ep0_max_packet(speed);
    ictx_ctrl(d)[1] = 3;
    ictx_slot(d)[0] = ((uint32_t)speed << 20) | (1u << 27);
    ictx_slot(d)[1] = (uint32_t)port << 16;
    set_ep_ctx(ictx_ep(d, 1), 4, mps, 0, d->ep0.phys, 8);
    cc = run_command((uint32_t)d->ictx_phys, (uint32_t)(d->ictx_phys >> 32),
                     (TRB_TYPE_ADDRESS_DEVICE_CMD << 10) | ((uint32_t)slot << 24), NULL);
    if (cc != CC_SUCCESS) {
        kprintf("[xHCI] Slot %d: Address Device failed (cc=%d)\n", slot, (int)cc);
        goto fail;
    }

    // First 8 bytes reveal bMaxPacketSize0; FS devices may differ from our guess.
    if (get_descriptor(d, 0x80, 1, 0, 0, 8) < 0) goto fail;
    int real = d->data[7];
    if (speed >= SPEED_SS) real = 1 << real;
    if (real && real != mps && (speed == SPEED_FS || speed == SPEED_LS)) {
        memset(d->ictx, 0, 3 * ctx_size);
        ictx_ctrl(d)[1] = 2;
        set_ep_ctx(ictx_ep(d, 1), 4, real, 0, d->ep0.phys, 8);
        // Evaluate Context only updates max packet size; keep the live dequeue pointer.
        ictx_ep(d, 1)[2] = 0; ictx_ep(d, 1)[3] = 0;
        cc = run_command((uint32_t)d->ictx_phys, (uint32_t)(d->ictx_phys >> 32),
                         (TRB_TYPE_EVALUATE_CTX_CMD << 10) | ((uint32_t)slot << 24), NULL);
        if (cc != CC_SUCCESS) kprintf("[xHCI] Slot %d: Evaluate Context failed (cc=%d)\n", slot, (int)cc);
    }

    if (get_descriptor(d, 0x80, 1, 0, 0, 18) < 0) goto fail;
    uint8_t dd[18];
    memcpy(dd, d->data, 18);
    kprintf("[xHCI] Slot %d: USB %x.%02x device %04x:%04x class %d, %d config(s)\n", slot,
            dd[3], dd[2], dd[8] | (dd[9] << 8), dd[10] | (dd[11] << 8), dd[4], dd[17]);

    if (get_descriptor(d, 0x80, 2, 0, 0, 9) < 0) goto fail;
    uint16_t total = d->data[2] | (d->data[3] << 8);
    uint8_t cfg_value = d->data[5];
    if (total < 9) goto fail;
    if (total > 2048) total = 2048;
    if (get_descriptor(d, 0x80, 2, 0, 0, total) < 0) goto fail;

    struct hid_cand cand[MAX_HID];
    int nc = 0;
    int cur = -1; // index into cand for the current HID interface
    bool hid_if = false;
    const uint8_t *p = d->data;
    for (int i = 0; i + 2 <= total && p[i] >= 2;) {
        uint8_t len = p[i], t = p[i + 1];
        if (i + len > total) break;
        if (t == 4 && len >= 9) {
            hid_if = p[i + 5] == 3 && p[i + 3] == 0 && nc < MAX_HID;
            cur = -1;
            if (hid_if) {
                memset(&cand[nc], 0, sizeof(cand[nc]));
                cand[nc].iface = p[i + 2];
                cand[nc].sub = p[i + 6];
                cand[nc].proto = p[i + 7];
                cur = nc;
            }
        } else if (t == 0x21 && cur >= 0 && len >= 9) {
            cand[cur].rdesc_len = p[i + 7] | (p[i + 8] << 8);
        } else if (t == 5 && cur >= 0 && len >= 7 && (p[i + 2] & 0x80) && (p[i + 3] & 3) == 3 && !cand[cur].ep) {
            cand[cur].ep = p[i + 2];
            cand[cur].mps = p[i + 4] | (p[i + 5] << 8);
            cand[cur].interval = p[i + 6];
            nc++;
            cur = -1;
        }
        i += len;
    }

    if (nc) setup_hid(d, cand, nc);
    if (ctrl_xfer(d, 0x00, 9, cfg_value, 0, 0) < 0) goto fail;
    kprintf("[xHCI] Slot %d: configuration %d set, %d HID interface(s)\n", slot, cfg_value, d->nhid);
    if (d->nhid) classify_hid(d, cand);
    return;

fail:
    kprintf("[xHCI] Port %d: enumeration failed\n", port);
}

static void service_ports(void) {
    for (uint8_t p = 1; p <= num_ports && p <= MAX_PORTS; p++) {
        if (port_gone[p]) {
            port_gone[p] = false;
            if (port_slot[p]) teardown_slot(port_slot[p]);
        }
        if (port_pending[p]) {
            port_pending[p] = false;
            if (port_slot[p]) teardown_slot(port_slot[p]);
            enumerate_port(p);
        }
    }
}

static void recover_endpoints(void) {
    for (int s = 1; s <= MAX_SLOTS; s++) {
        struct usb_dev *d = &devs[s];
        if (!d->used) continue;
        for (int i = 0; i < d->nhid; i++) {
            struct hid_iface *h = &d->hid[i];
            if (!h->halted) continue;
            h->halted = false;
            uint32_t sl = (uint32_t)d->slot << 24, ep = (uint32_t)h->dci << 16;
            run_command(0, 0, (TRB_RESET_EP << 10) | ep | sl, NULL);
            uint64_t deq = h->ring.phys + h->ring.enq * sizeof(struct xhci_trb);
            run_command((uint32_t)deq | h->ring.cycle, (uint32_t)(deq >> 32), (TRB_SET_TR_DEQ << 10) | ep | sl, NULL);
            queue_hid(d, h);
        }
    }
}

void xhci_poll_events(void) {
    if (!xhci_ready || in_poll) return;
    in_poll = true;
    process_events();
    service_ports();
    recover_endpoints();
    in_poll = false;
}


/* ---- Controller init ---- */

static bool wait_reg(uint32_t off, uint32_t mask, uint32_t val, uint32_t ms) {
    uint64_t end = pit_get_ticks() + ms;
    while ((xhci_read32(op_regs, off) & mask) != val) {
        if (pit_get_ticks() > end) return false;
        pit_sleep_ms(1);
    }
    return true;
}

static bool xhci_driver_init(const pci_device_t *dev) {
    if (dev->class_code != 0x0C || dev->subclass != 0x03 || dev->prog_if != 0x30) {
        return false;
    }

    kprintf("[xHCI] Found Controller at %02x:%02x.%d\n", dev->bus, dev->device, dev->function);

    uint16_t cmd = pci_read_config_word(dev->bus, dev->device, dev->function, 0x04);
    cmd |= (1 << 2) | (1 << 1) | (1 << 0);
    pci_write_config_word(dev->bus, dev->device, dev->function, 0x04, cmd);

    uint32_t bar0_low = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x10);
    uint32_t bar0_high = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x14);
    uint64_t phys_base = bar0_low & 0xFFFFFFF0;
    if ((bar0_low & 0x04) != 0) phys_base |= ((uint64_t)bar0_high << 32);

    if (!phys_base) {
        early_blog(BLOG_FAIL, "xHCI", "Invalid MMIO BAR");
        return false;
    }

    char msg[64];
    sprintf(msg, "xHCI MMIO at 0x%x", (uint32_t)phys_base);
    early_blog(BLOG_INFO, "xHCI", msg);

    for (uint64_t offset = 0; offset < 65536; offset += PAGE_SIZE) {
        vmm_map_mmio(kernel_pml4, phys_base + hhdm_offset + offset, phys_base + offset, PTE_PRESENT | PTE_WRITABLE | 0x18);
    }

    cap_regs = (volatile uint8_t *)(phys_base + hhdm_offset);
    op_regs = cap_regs + cap_regs[XHCI_CAP_CAPLENGTH];

    xhci_bios_handoff(cap_regs);

    uint32_t hcsparams1 = xhci_read32(cap_regs, XHCI_CAP_HCSPARAMS1);
    uint32_t hcsparams2 = xhci_read32(cap_regs, XHCI_CAP_HCSPARAMS2);
    uint32_t hccparams1 = xhci_read32(cap_regs, XHCI_CAP_HCCPARAMS1);
    max_slots = hcsparams1 & 0xFF;
    num_ports = (hcsparams1 >> 24) & 0xFF;
    if (max_slots > MAX_SLOTS) max_slots = MAX_SLOTS;
    ctx_size = (hccparams1 & (1 << 2)) ? 64 : 32;

    db_regs = (volatile uint32_t *)(cap_regs + xhci_read32(cap_regs, XHCI_CAP_DBOFF));
    rt_regs = cap_regs + xhci_read32(cap_regs, XHCI_CAP_RTSOFF);

    kprintf("[xHCI] Version 0x%x, MaxSlots=%d, Ports=%d, ContextSize=%d\n",
            xhci_read32(cap_regs, XHCI_CAP_HCIVERSION) >> 16, max_slots, num_ports, (int)ctx_size);

    if (xhci_read32(op_regs, XHCI_OP_USBCMD) & USBCMD_RUN) {
        xhci_write32(op_regs, XHCI_OP_USBCMD, xhci_read32(op_regs, XHCI_OP_USBCMD) & ~USBCMD_RUN);
        wait_reg(XHCI_OP_USBSTS, USBSTS_HCH, USBSTS_HCH, 500);
    }
    xhci_write32(op_regs, XHCI_OP_USBCMD, USBCMD_HCRST);
    wait_reg(XHCI_OP_USBCMD, USBCMD_HCRST, 0, 1000);
    if (!wait_reg(XHCI_OP_USBSTS, USBSTS_CNR, 0, 1000)) {
        early_blog(BLOG_FAIL, "xHCI", "Controller reset timed out");
        return false;
    }

    uint64_t dcbaa_phys;
    dcbaa_virt = (uint64_t *)alloc_dma_page(&dcbaa_phys);
    if (!dcbaa_virt) return false;

    // Scratchpad buffers requested by the controller (needed on real hardware)
    uint32_t nscratch = ((hcsparams2 >> 27) & 0x1F) | (((hcsparams2 >> 21) & 0x1F) << 5);
    if (nscratch) {
        uint64_t arr_phys;
        uint64_t *arr = (uint64_t *)alloc_dma_page(&arr_phys);
        if (!arr || nscratch > PAGE_SIZE / 8) return false;
        for (uint32_t i = 0; i < nscratch; i++) {
            uint64_t bp;
            if (!alloc_dma_page(&bp)) return false;
            arr[i] = bp;
        }
        dcbaa_virt[0] = arr_phys;
    }
    xhci_write64(op_regs, XHCI_OP_DCBAAP, dcbaa_phys);

    if (!ring_init(&cmd_ring)) return false;
    xhci_write64(op_regs, XHCI_OP_CRCR, cmd_ring.phys | 1);

    uint64_t erst_phys;
    struct xhci_erst_entry *erst = (struct xhci_erst_entry *)alloc_dma_page(&erst_phys);
    evt_ring = (struct xhci_trb *)alloc_dma_page(&evt_ring_phys);
    if (!erst || !evt_ring) return false;
    erst[0].ring_segment_base = evt_ring_phys;
    erst[0].ring_segment_size = RING_TRBS;

    volatile uint8_t *ir0 = rt_regs + XHCI_RT_IR0;
    xhci_write32(ir0, XHCI_IR_ERSTSZ, 1);
    xhci_write64(ir0, XHCI_IR_ERDP, evt_ring_phys | (1 << 3));
    xhci_write64(ir0, XHCI_IR_ERSTBA, erst_phys);

    uint32_t config = xhci_read32(op_regs, XHCI_OP_CONFIG);
    xhci_write32(op_regs, XHCI_OP_CONFIG, (config & ~0xFF) | max_slots);

    xhci_write32(op_regs, XHCI_OP_USBCMD, USBCMD_RUN);
    if (!wait_reg(XHCI_OP_USBSTS, USBSTS_HCH, 0, 1000)) {
        early_blog(BLOG_FAIL, "xHCI", "Controller failed to start");
        return false;
    }
    xhci_ready = true;
    early_blog(BLOG_OK, "xHCI", "Host Controller started successfully!");

    if (num_ports > MAX_PORTS) num_ports = MAX_PORTS;
    for (uint8_t i = 1; i <= num_ports; i++) {
        uint32_t off = XHCI_OP_PORTSC_BASE + (i - 1) * 16;
        uint32_t v = xhci_read32(op_regs, off);
        if (!(v & PORTSC_PP)) xhci_write32(op_regs, off, (v & PORTSC_KEEP) | PORTSC_PP);
    }
    pit_sleep_ms(100); // VBUS settle / device attach debounce

    in_poll = true;
    for (uint8_t i = 1; i <= num_ports; i++) {
        uint32_t v = xhci_read32(op_regs, XHCI_OP_PORTSC_BASE + (i - 1) * 16);
        if (v & PORTSC_CCS) port_pending[i] = true;
    }
    service_ports();
    in_poll = false;

    return true;
}

static pci_driver_t xhci_driver = {
    .name = "xHCI (USB 3.0) Controller",
    .match_type = DRIVER_MATCH_CLASS,
    .class_code = 0x0C,
    .subclass = 0x03,
    .init = xhci_driver_init
};

void xhci_init(void) {
    driver_register(&xhci_driver);
}
