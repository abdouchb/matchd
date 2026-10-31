/*
 * Zarqaa OS 0.1 - Intel High Definition Audio Driver
 * Creator: Abdelhak Chabbi
 *
 * Full HDA driver with CORB/RIRB command transport,
 * codec discovery, widget enumeration, and PCM output stream.
 * Targets Realtek ALC282/283 (HP Pavilion 15-n234sl).
 */

#include "drivers/hda.h"
#include "kernel/pci.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/driver.h"
#include "kernel/syslog.h"
#include "kernel/heap.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"

extern uint64_t hhdm_offset;
extern uint64_t *kernel_pml4;
extern void kprintf(const char *fmt, ...);

/* ========== Controller State ========== */

static volatile uint8_t *hda_base = NULL;  /* MMIO base virtual address */
static bool hda_available = false;

/* CORB/RIRB */
// Global codec tracking
static uint8_t codec_addr = 0;
static uint32_t max_gain_steps = 0x7F; // Fallback max gain
static uint16_t  rirb_entries = 256;
static uint16_t  rirb_rp = 0;  /* Our local read pointer */

static uint32_t *corb_buf = NULL;
static uint64_t  corb_phys = 0;
static uint64_t *rirb_buf = NULL;
static uint64_t  rirb_phys = 0;
static uint16_t  corb_entries = 256;

/* Output Stream */
static int out_stream_offset = 0;  /* MMIO offset of output stream 0 */
static struct hda_bdl_entry *out_bdl = NULL;
static uint64_t out_bdl_phys = 0;
static uint8_t *out_buf = NULL;
static uint64_t out_buf_phys = 0;
#define OUT_BUF_SIZE (32 * 4096)  /* 128KB ring buffer */
#define OUT_BDL_COUNT 32

/* Codec Setup Variables */
static uint8_t active_dacs[32]; /* Up to 32 widgets can be tracked for volume */
static uint8_t num_active_dacs = 0;
static uint64_t *dma_pos_buf = NULL;
static uint64_t dma_pos_phys = 0;

/* ========== MMIO Helpers ========== */

static inline uint8_t  hda_read8 (uint32_t off) { return *(volatile uint8_t  *)(hda_base + off); }
static inline uint16_t hda_read16(uint32_t off) { return *(volatile uint16_t *)(hda_base + off); }
static inline uint32_t hda_read32(uint32_t off) { return *(volatile uint32_t *)(hda_base + off); }
static inline void hda_write8 (uint32_t off, uint8_t  v) { *(volatile uint8_t  *)(hda_base + off) = v; }
static inline void hda_write16(uint32_t off, uint16_t v) { *(volatile uint16_t *)(hda_base + off) = v; }
static inline void hda_write32(uint32_t off, uint32_t v) { *(volatile uint32_t *)(hda_base + off) = v; }

static void hda_delay(void) {
    for (volatile int i = 0; i < 10000; i++) {
        __asm__ volatile("pause");
    }
}

/* ========== CORB/RIRB Transport ========== */

static void corb_rirb_setup(void) {
    /* Stop CORB and RIRB */
    hda_write8(HDA_CORBCTL, 0);
    hda_write8(HDA_RIRBCTL, 0);
    hda_delay();

    /* Determine CORB size */
    uint8_t corbsize_cap = hda_read8(HDA_CORBSIZE);
    if (corbsize_cap & 0x40)      { corb_entries = 256; hda_write8(HDA_CORBSIZE, 0x02); }
    else if (corbsize_cap & 0x20) { corb_entries = 16;  hda_write8(HDA_CORBSIZE, 0x01); }
    else                          { corb_entries = 2;   hda_write8(HDA_CORBSIZE, 0x00); }

    /* Determine RIRB size */
    uint8_t rirbsize_cap = hda_read8(HDA_RIRBSIZE);
    if (rirbsize_cap & 0x40)      { rirb_entries = 256; hda_write8(HDA_RIRBSIZE, 0x02); }
    else if (rirbsize_cap & 0x20) { rirb_entries = 16;  hda_write8(HDA_RIRBSIZE, 0x01); }
    else                          { rirb_entries = 2;   hda_write8(HDA_RIRBSIZE, 0x00); }

    /* Allocate CORB (1 page is plenty for 256 * 4 bytes = 1KB) */
    corb_phys = pmm_alloc_low32();
    if (!corb_phys) return;
    corb_buf = (uint32_t *)(corb_phys + hhdm_offset);
    memset(corb_buf, 0, 4096);

    /* Allocate RIRB (1 page for 256 * 8 bytes = 2KB) */
    rirb_phys = pmm_alloc_low32();
    if (!rirb_phys) return;
    rirb_buf = (uint64_t *)(rirb_phys + hhdm_offset);
    memset(rirb_buf, 0, 4096);

    /* Program CORB base address */
    for (uint32_t i = 0; i < 4096; i += 64) __asm__ volatile("clflush (%0)" :: "r"((uint8_t*)corb_buf + i) : "memory");
    for (uint32_t i = 0; i < 4096; i += 64) __asm__ volatile("clflush (%0)" :: "r"((uint8_t*)rirb_buf + i) : "memory");

    hda_write32(HDA_CORBLBASE, (uint32_t)(corb_phys & 0xFFFFFFFF));
    hda_write32(HDA_CORBUBASE, (uint32_t)(corb_phys >> 32));

    /* Reset CORB read pointer */
    hda_write16(HDA_CORBRP, 0x8000);  /* Set reset bit */
    hda_delay();
    hda_write16(HDA_CORBRP, 0x0000);  /* Clear reset bit */
    hda_delay();

    /* Set CORB write pointer to 0 */
    hda_write16(HDA_CORBWP, 0);

    /* Program RIRB base address */
    hda_write32(HDA_RIRBLBASE, (uint32_t)(rirb_phys & 0xFFFFFFFF));
    hda_write32(HDA_RIRBUBASE, (uint32_t)(rirb_phys >> 32));

    /* Reset RIRB write pointer */
    hda_write16(HDA_RIRBWP, 0x8000);
    rirb_rp = 0;

    /* Set RIRB Interrupt Count (unused but set it) */
    hda_write16(HDA_RINTCNT, 1);

    /* Start CORB and RIRB DMA engines */
    hda_write8(HDA_CORBCTL, HDA_CORBCTL_RUN);
    hda_write8(HDA_RIRBCTL, HDA_RIRBCTL_RUN);
    hda_delay();

    kprintf("[HDA] CORB/RIRB initialized (CORB=%d, RIRB=%d entries)\n", corb_entries, rirb_entries);
}

/* Send a verb via CORB and wait for response via RIRB */
static uint32_t hda_send_verb(uint32_t verb) {
    /* Write verb to CORB */
    uint16_t wp = hda_read16(HDA_CORBWP) & 0xFF;
    wp = (wp + 1) % corb_entries;
    corb_buf[wp] = verb;
    __asm__ volatile("clflush (%0)" :: "r"(&corb_buf[wp]) : "memory");
    __asm__ volatile("sfence" ::: "memory");
    hda_write16(HDA_CORBWP, wp);

    /* Wait for RIRB response (reduced timeout: 500 iterations) */
    for (int timeout = 0; timeout < 500; timeout++) {
        uint16_t rirb_wp = hda_read16(HDA_RIRBWP) & 0xFF;
        if (rirb_wp != rirb_rp) {
            rirb_rp = (rirb_rp + 1) % rirb_entries;
            __asm__ volatile("clflush (%0)" :: "r"(&rirb_buf[rirb_rp]) : "memory");
            uint64_t response = rirb_buf[rirb_rp];
            return (uint32_t)(response & 0xFFFFFFFF);
        }
        for (volatile int d = 0; d < 100; d++) __asm__ volatile("pause");
    }
    return 0xFFFFFFFF;
}

/* Send a verb WITHOUT waiting for response - for volume/mute commands during playback.
 * Does NOT advance rirb_rp - the hardware may or may not send a response. */
static void hda_send_verb_noack(uint32_t verb) {
    uint16_t wp = hda_read16(HDA_CORBWP) & 0xFF;
    wp = (wp + 1) % corb_entries;
    corb_buf[wp] = verb;
    __asm__ volatile("clflush (%0)" :: "r"(&corb_buf[wp]) : "memory");
    __asm__ volatile("sfence" ::: "memory");
    hda_write16(HDA_CORBWP, wp);
    /* Small settle delay so HDA controller sees the write */
    for (volatile int d = 0; d < 50; d++) __asm__ volatile("pause");
}

/* Convenience: send a verb to codec/nid with a 4-bit verb ID + 8-bit payload */
static uint32_t hda_codec_verb(uint8_t cad, uint8_t nid, uint16_t verb, uint8_t payload) {
    uint32_t cmd = ((uint32_t)cad << 28) | ((uint32_t)nid << 20) | ((uint32_t)verb << 8) | payload;
    return hda_send_verb(cmd);
}

/* Convenience: 12-bit verb (like GET_PARAM) */
static uint32_t hda_codec_verb12(uint8_t cad, uint8_t nid, uint32_t verb_and_payload) {
    uint32_t cmd = ((uint32_t)cad << 28) | ((uint32_t)nid << 20) | verb_and_payload;
    return hda_send_verb(cmd);
}

static uint32_t hda_get_param(uint8_t cad, uint8_t nid, uint8_t param) {
    return hda_codec_verb12(cad, nid, (HDA_VERB_GET_PARAM << 8) | param);
}

/* ========== Codec Discovery & Widget Enumeration ========== */

static bool hda_discover_codec(uint8_t cad) {
    uint32_t vendor = hda_get_param(cad, 0, HDA_PARAM_VENDOR_ID);
    kprintf("[HDA] Codec %d: Vendor=0x%x Device=0x%x\n", cad, (vendor >> 16) & 0xFFFF, vendor & 0xFFFF);
    syslog_write("[HDA] Codec discovered and initialized.\n");

    /* Get sub-node count of root node */
    uint32_t node_count = hda_get_param(cad, 0, HDA_PARAM_NODE_COUNT);
    uint8_t start_nid = (node_count >> 16) & 0xFF;
    uint8_t num_nodes = node_count & 0xFF;
    kprintf("[HDA] Root: start_nid=%d, count=%d\n", start_nid, num_nodes);

    /* Find the Audio Function Group */
    uint8_t afg_nid = 0;
    for (uint8_t i = 0; i < num_nodes; i++) {
        uint8_t nid = start_nid + i;
        uint32_t fg_type = hda_get_param(cad, nid, HDA_PARAM_FN_GROUP_TYPE);
        if ((fg_type & 0xFF) == 0x01) {  /* Audio Function Group */
            afg_nid = nid;
            kprintf("[HDA] Audio Function Group at NID %d\n", nid);
            break;
        }
    }

    if (!afg_nid) {
        kprintf("[HDA] ERROR: No Audio Function Group found!\n");
        return false;
    }

    /* Power up the AFG: Set Power State D0 */
    hda_codec_verb(cad, afg_nid, HDA_VERB_SET_POWER_STATE, 0x00);  /* D0 */
    hda_delay();
    hda_delay();

    /* Get sub-nodes of AFG (these are the widgets) */
    node_count = hda_get_param(cad, afg_nid, HDA_PARAM_NODE_COUNT);
    start_nid = (node_count >> 16) & 0xFF;
    num_nodes = node_count & 0xFF;
    kprintf("[HDA] AFG widgets: start=%d count=%d\n", start_nid, num_nodes);

    /* Walk widgets and apply BROADCAST routing */
    int dac_count = 0;
    int pin_count = 0;
    int mixer_count = 0;
    
    for (uint8_t i = 0; i < num_nodes; i++) {
        uint8_t nid = start_nid + i;
        uint32_t wcap = hda_get_param(cad, nid, HDA_PARAM_AUDIO_WIDGET_CAP);
        uint8_t wtype = (wcap >> 20) & 0xF;

        /* Power up widget to D0 */
        hda_codec_verb(cad, nid, HDA_VERB_SET_POWER_STATE, 0x00);
        
        if (wtype == HDA_WIDGET_AO) {
            /* === DAC (Audio Output) === */
            dac_count++;
            
            /* Only assign stream + volume to the FIRST DAC */
            if (dac_count == 1) {
                /* Track this DAC for volume control */
                if (num_active_dacs < 32) active_dacs[num_active_dacs++] = nid;

                /* Discover gain range */
                uint32_t out_cap = hda_get_param(cad, nid, HDA_PARAM_OUT_AMP_CAP);
                max_gain_steps = (out_cap >> 8) & 0x7F;
                if (max_gain_steps == 0) max_gain_steps = 0x3F;

                /* 1. Set format: 48kHz, 16-bit, stereo */
                hda_codec_verb12(cad, nid, (HDA_VERB_SET_CONV_FORMAT << 8) | 0x11);
                hda_codec_verb12(cad, nid, (0x200 << 0) | 0x0011);
                
                /* 2. Set stream tag 1, channel 0 */
                hda_codec_verb(cad, nid, HDA_VERB_SET_CONV_STREAM, 0x10);
                
                /* 3. Unmute at 80% */
                uint32_t vol = (max_gain_steps * 80) / 100;
                uint32_t amp_verb = ((uint32_t)cad << 28) | ((uint32_t)nid << 20) |
                                    (0x3 << 16) | 0xB000 | vol;
                hda_send_verb(amp_verb);
            } else {
                /* Mute extra DACs */
                hda_codec_verb12(cad, nid, (0x3 << 16) | 0xB080); /* Mute */
            }
            
        } else if (wtype == HDA_WIDGET_PIN) {
            /* === Pin Complex (Speaker/Headphone jack) === */
            pin_count++;
            /* Track Pin for volume control too */
            if (num_active_dacs < 32) active_dacs[num_active_dacs++] = nid;

            /* 1. Enable Output and Headphone Amp */
            uint32_t val = hda_codec_verb12(cad, nid, (HDA_VERB_GET_PIN_CTRL << 8) | 0);
            val |= 0xC0;
            hda_codec_verb12(cad, nid, (HDA_VERB_SET_PIN_CTRL << 8) | (val & 0xFF));
            
            /* 2. Enable EAPD for laptops like HP */
            uint32_t eapd = hda_codec_verb12(cad, nid, (HDA_VERB_GET_EAPD << 8) | 0);
            eapd |= 0x02;
            hda_codec_verb12(cad, nid, (HDA_VERB_SET_EAPD << 8) | (eapd & 0xFF));
            
            /* 3. Unmute Output Amplifier at 80% */
            uint32_t amp_verb = ((uint32_t)cad << 28) | ((uint32_t)nid << 20) |
                                (0x3 << 16) | 0xB000 | (max_gain_steps * 80 / 100);
            hda_send_verb(amp_verb);
            
        } else if (wtype == HDA_WIDGET_MIXER) {
            /* === Audio Mixer === */
            mixer_count++;
            /* Track Mixer for volume control */
            if (num_active_dacs < 32) active_dacs[num_active_dacs++] = nid;

            /* Unmute at 80% */
            uint32_t amp_verb = ((uint32_t)cad << 28) | ((uint32_t)nid << 20) |
                                (0x3 << 16) | 0xB000 | (max_gain_steps * 80 / 100);
            hda_send_verb(amp_verb);
        }
    }
    
    kprintf("[HDA] Broadcast routing complete! Activated %d DACs, %d Mixers, %d Pins.\n", dac_count, mixer_count, pin_count);
    
    // Only claim success if we found analog output pins (reject HDMI-only controllers)
    if (pin_count > 0 && dac_count > 0) {
        syslog_write("[HDA] Valid analog output path configured successfully.\n");
        return true;
    } else {
        syslog_write("[HDA] No analog output pins found (likely HDMI). Rejecting.\n");
        return false;
    }
}

/* ========== Output Stream Setup ========== */

static void hda_setup_output_stream(void) {
    /* Determine output stream base offset from GCAP */
    uint16_t gcap = hda_read16(HDA_GCAP);
    uint8_t num_iss = (gcap >> 8) & 0xF;   /* Input streams */
    uint8_t num_oss = (gcap >> 12) & 0xF;  /* Output streams */
    (void)num_oss;

    /* Output stream 0 starts after all input stream descriptors */
    out_stream_offset = 0x80 + (num_iss * 0x20);
    kprintf("[HDA] Output stream at offset 0x%x (ISS=%d, OSS=%d)\n", out_stream_offset, num_iss, num_oss);

    /* Stop stream first */
    hda_write8(out_stream_offset + HDA_SD_CTL, 0);
    hda_delay();

    /* Reset stream */
    hda_write8(out_stream_offset + HDA_SD_CTL, 1);  /* SRST */
    for (int i = 0; i < 100000; i++) {
        if (hda_read8(out_stream_offset + HDA_SD_CTL) & 1) break;
        __asm__ volatile("pause");
    }
    hda_write8(out_stream_offset + HDA_SD_CTL, 0);
    for (int i = 0; i < 100000; i++) {
        if (!(hda_read8(out_stream_offset + HDA_SD_CTL) & 1)) break;
        __asm__ volatile("pause");
    }

    /* Allocate DMA Position Buffer (1 page) */
    dma_pos_phys = pmm_alloc_low32();
    if (dma_pos_phys) {
        dma_pos_buf = (uint64_t *)(dma_pos_phys + hhdm_offset);
        memset(dma_pos_buf, 0, 4096);
        hda_write32(0x70, (uint32_t)(dma_pos_phys & 0xFFFFFFFF) | 1); /* HDA_DPLBASE + Enable Bit */
        hda_write32(0x74, (uint32_t)(dma_pos_phys >> 32)); /* HDA_DPUBASE */
    }

    /* Allocate BDL (1 page) */
    out_bdl_phys = pmm_alloc_low32();
    if (!out_bdl_phys) return;
    out_bdl = (struct hda_bdl_entry *)(out_bdl_phys + hhdm_offset);
    memset(out_bdl, 0, 4096);

    /* Allocate output buffer (32 pages = 128KB) */
    out_buf_phys = pmm_alloc_blocks_low32(32);
    if (!out_buf_phys) return;
    out_buf = (uint8_t *)(out_buf_phys + hhdm_offset);
    memset(out_buf, 0, OUT_BUF_SIZE);

    /* Fill BDL entries */
    for (int i = 0; i < OUT_BDL_COUNT; i++) {
        out_bdl[i].address = out_buf_phys + (i * 4096);
        out_bdl[i].length = 4096;
        out_bdl[i].flags = 1;  /* IOC */
    }
    for (uint32_t i = 0; i < 4096; i += 64) __asm__ volatile("clflush (%0)" :: "r"((uint8_t*)out_bdl + i) : "memory");

    /* Set BDL address */
    hda_write32(out_stream_offset + HDA_SD_BDLPL, (uint32_t)(out_bdl_phys & 0xFFFFFFFF));
    hda_write32(out_stream_offset + HDA_SD_BDLPU, (uint32_t)(out_bdl_phys >> 32));

    /* Set cyclic buffer length */
    hda_write32(out_stream_offset + HDA_SD_CBL, OUT_BUF_SIZE);

    /* Set Last Valid Index */
    hda_write16(out_stream_offset + HDA_SD_LVI, OUT_BDL_COUNT - 1);

    /* Set stream format: 48kHz, 16-bit, Stereo (same as DAC converter format) */
    hda_write16(out_stream_offset + HDA_SD_FMT, 0x0011);

    /* Set stream number (tag) in CTL: stream 1 (bits 23:20) */
    uint32_t ctl = hda_read32(out_stream_offset + HDA_SD_CTL);
    ctl &= ~(0xF << 20);
    ctl |= (1 << 20);  /* Stream 1 */
    hda_write32(out_stream_offset + HDA_SD_CTL, ctl);

    kprintf("[HDA] Output stream configured: %d BDL entries, %d bytes total\n", OUT_BDL_COUNT, OUT_BUF_SIZE);
}

/* ========== PCI Driver Init ========== */

static bool hda_driver_init(const pci_device_t *dev) {
    if (hda_available) return true; // We only support one active HDA controller for now
    
    if (dev->class_code != 0x04 || dev->subclass != 0x03) {
        return false;
    }

    kprintf("[HDA] Found HD Audio Controller at %02x:%02x.%d (VID:%04x DID:%04x)\n",
            dev->bus, dev->device, dev->function, dev->vendor_id, dev->device_id);

    /* Enable Bus Mastering + Memory Space */
    uint16_t cmd = pci_read_config_word(dev->bus, dev->device, dev->function, 0x04);
    cmd |= (1 << 2) | (1 << 1);  /* Bus Master + Memory Space */
    pci_write_config_word(dev->bus, dev->device, dev->function, 0x04, cmd);

    /* Get BAR0 (64-bit MMIO) */
    uint32_t bar0_low = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x10);
    uint32_t bar0_high = pci_read_config_dword(dev->bus, dev->device, dev->function, 0x14);
    uint64_t phys_base;

    if (bar0_low & 0x04) {
        /* 64-bit BAR */
        phys_base = ((uint64_t)bar0_high << 32) | (bar0_low & ~0xFULL);
    } else {
        phys_base = bar0_low & ~0xFUL;
    }

    kprintf("[HDA] MMIO Physical Base: 0x%x%x\n", (uint32_t)(phys_base >> 32), (uint32_t)phys_base);
    
    char mmsg[128];
    sprintf(mmsg, "HDA MMIO at 0x%x", (uint32_t)phys_base);
    early_blog(BLOG_INFO, "HDA", mmsg);

    /* Map 64KB (16 pages) of MMIO starting at a safe virtual address.
     * We use vmm_map_mmio to safely skip huge pages and avoid corrupting SMRAM 
     * while mapping unmapped MMIO regions to prevent Exception 14. */
    extern uint64_t hhdm_offset;
    extern uint64_t *kernel_pml4;
    for (int i = 0; i < 16; i++) {
        uint64_t offset = i * PAGE_SIZE;
        /* Map as Present | Writable | Cache Disable (0x10) | Write Through (0x08) */
        vmm_map_mmio(kernel_pml4, phys_base + hhdm_offset + offset, phys_base + offset, PTE_PRESENT | PTE_WRITABLE | 0x18);
    }
    hda_base = (volatile uint8_t *)(phys_base + hhdm_offset);

    /* Read and log GCAP */
    uint16_t gcap = hda_read16(HDA_GCAP);
    uint8_t vmaj = hda_read8(HDA_VMAJ);
    uint8_t vmin = hda_read8(HDA_VMIN);
    kprintf("[HDA] Version %d.%d, GCAP=0x%x (OSS=%d ISS=%d BSS=%d)\n",
            vmaj, vmin, gcap, (gcap >> 12) & 0xF, (gcap >> 8) & 0xF, (gcap >> 3) & 0x1F);

    /* ---- Controller Reset ---- */
    /* Put controller in reset */
    hda_write32(HDA_GCTL, 0);
    hda_delay();

    /* Wait for CRST to read 0 */
    for (int i = 0; i < 100; i++) {
        if ((hda_read32(HDA_GCTL) & HDA_GCTL_CRST) == 0) break;
        hda_delay();
    }

    /* Bring out of reset */
    hda_write32(HDA_GCTL, HDA_GCTL_CRST);

    /* Wait for CRST to read 1 */
    for (int i = 0; i < 100; i++) {
        if (hda_read32(HDA_GCTL) & HDA_GCTL_CRST) break;
        hda_delay();
    }

    /* Wait for codec discovery (STATESTS) */
    for (int i = 0; i < 5000; i++) {
        hda_delay();
    }
    uint16_t statests = hda_read16(HDA_STATESTS);
    kprintf("[HDA] STATESTS after reset: 0x%x\n", statests);

    if (statests == 0) {
        kprintf("[HDA] ERROR: No codecs found on HDA link!\n");
        syslog_write("[ERROR] HDA: No codecs found after reset.\n");
        return true;  /* Still claim the device */
    }

    /* We must setup CORB/RIRB *before* we can query codecs */
    corb_rirb_setup();

    /* Find appropriate codec */
    uint8_t best_codec = 0xFF;
    for (int i = 0; i < 15; i++) {
        if (statests & (1 << i)) {
            uint32_t vendor = hda_get_param(i, 0, HDA_PARAM_VENDOR_ID);
            kprintf("[HDA] Codec found at address %d, Vendor=0x%x\n", i, vendor);
            
            /* If vendor is not Intel (0x8086), it's probably the main audio codec (e.g., Realtek 0x10EC) */
            if ((vendor >> 16) != 0x8086) {
                best_codec = i;
                break;
            }
            if (best_codec == 0xFF) {
                best_codec = i; /* Save first codec as fallback */
            }
        }
    }

    if (best_codec != 0xFF) {
        codec_addr = best_codec;
        kprintf("[HDA] Selected codec at address %d\n", codec_addr);
    } else {
        kprintf("[HDA] ERROR: No valid codec selected!\n");
        return true; /* Still claim device to prevent errors */
    }

    /* Discover codec widgets */
    if (!hda_discover_codec(codec_addr)) {
        early_blog(BLOG_WARN, "AUDIO", "Controller has no analog outputs (HDMI). Skipping.");
        hda_write32(HDA_GCTL, 0); // Put back in reset
        hda_base = NULL;
        return false; // Try next controller!
    }

    /* Setup output stream */
    hda_setup_output_stream();

    hda_available = true;
    kprintf("[HDA] Driver initialization complete!\n");
    syslog_write("[HDA] Intel HD Audio driver initialized successfully.\n");

    return true;
}

/* ========== Public API ========== */

bool hda_is_available(void) {
    return hda_available;
}

void hda_set_volume(uint8_t volume) {
    if (!hda_available || num_active_dacs == 0) return;
    if (volume > 100) volume = 100;

    /* Map 0-100 to 0-max_gain_steps */
    uint8_t gain = (volume * max_gain_steps) / 100;
    /* Bit 7 = mute when volume is 0 */
    uint32_t mute_bit = (volume == 0) ? 0x80 : 0x00;

    /* Fire-and-forget: send to all amps without waiting for RIRB response.
     * This prevents the UI from freezing during playback. */
    for (int i = 0; i < num_active_dacs; i++) {
        /* SET_AMP_GAIN_MUTE: Output, Left+Right channels */
        uint32_t amp_verb = ((uint32_t)codec_addr << 28) | ((uint32_t)active_dacs[i] << 20) |
                            (0x3 << 16) | 0xB000 | mute_bit | gain;
        hda_send_verb_noack(amp_verb);
    }
}

void hda_start_ring(void) {
    if (!hda_available || !out_buf) return;

    /* Update CBL to full buffer size */
    hda_write32(out_stream_offset + HDA_SD_CBL, OUT_BUF_SIZE);

    /* Calculate LVI based on actual data size */
    hda_write16(out_stream_offset + HDA_SD_LVI, OUT_BDL_COUNT - 1);

    /* Start the stream without interrupts (polling mode) */
    uint32_t ctl = hda_read32(out_stream_offset + HDA_SD_CTL);
    ctl |= HDA_SD_CTL_RUN;
    hda_write32(out_stream_offset + HDA_SD_CTL, ctl);
}

uint32_t hda_get_lpib(void) {
    if (!hda_available) return 0;
    if (dma_pos_buf) {
        /* Read from DMA Position Buffer (Stream 1 is at index num_iss) */
        uint16_t gcap = hda_read16(HDA_GCAP);
        uint8_t num_iss = (gcap >> 8) & 0xF;
        __asm__ volatile("clflush (%0)" :: "r"(&dma_pos_buf[num_iss]) : "memory");
        return (uint32_t)dma_pos_buf[num_iss];
    }
    return hda_read32(out_stream_offset + HDA_SD_LPIB);
}

uint8_t *hda_get_ring_buffer_virt(void) {
    return out_buf;
}

void hda_stop(void) {
    if (!hda_available) return;

    /* Stop stream */
    uint32_t ctl = hda_read32(out_stream_offset + HDA_SD_CTL);
    ctl &= ~HDA_SD_CTL_RUN;
    hda_write32(out_stream_offset + HDA_SD_CTL, ctl);
}

/* ========== Registration ========== */

static pci_driver_t hda_driver = {
    .name = "Intel HD Audio Controller",
    .match_type = DRIVER_MATCH_CLASS,
    .class_code = 0x04,
    .subclass = 0x03,
    .init = hda_driver_init
};

void hda_init(void) {
    driver_register(&hda_driver);
}
