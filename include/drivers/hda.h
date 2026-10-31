#ifndef DRIVERS_HDA_H
#define DRIVERS_HDA_H

#include "zarqaa/types.h"

/* Intel HD Audio Controller Register Offsets */
#define HDA_GCAP        0x00  /* Global Capabilities */
#define HDA_VMIN        0x02  /* Minor Version */
#define HDA_VMAJ        0x03  /* Major Version */
#define HDA_OUTPAY      0x04  /* Output Payload Capability */
#define HDA_INPAY       0x06  /* Input Payload Capability */
#define HDA_GCTL        0x08  /* Global Control */
#define HDA_WAKEEN      0x0C  /* Wake Enable */
#define HDA_STATESTS    0x0E  /* State Change Status */
#define HDA_GSTS        0x10  /* Global Status */
#define HDA_INTCTL      0x20  /* Interrupt Control */
#define HDA_INTSTS      0x24  /* Interrupt Status */
#define HDA_WALCLK      0x30  /* Wall Clock Counter */
#define HDA_SSYNC       0x38  /* Stream Synchronization */

/* CORB Registers */
#define HDA_CORBLBASE   0x40  /* CORB Lower Base Address */
#define HDA_CORBUBASE   0x44  /* CORB Upper Base Address */
#define HDA_CORBWP      0x48  /* CORB Write Pointer */
#define HDA_CORBRP      0x4A  /* CORB Read Pointer */
#define HDA_CORBCTL     0x4C  /* CORB Control */
#define HDA_CORBSTS     0x4D  /* CORB Status */
#define HDA_CORBSIZE    0x4E  /* CORB Size */

/* RIRB Registers */
#define HDA_RIRBLBASE   0x50  /* RIRB Lower Base Address */
#define HDA_RIRBUBASE   0x54  /* RIRB Upper Base Address */
#define HDA_RIRBWP      0x58  /* RIRB Write Pointer */
#define HDA_RINTCNT     0x5A  /* Response Interrupt Count */
#define HDA_RIRBCTL     0x5C  /* RIRB Control */
#define HDA_RIRBSTS     0x5D  /* RIRB Status */
#define HDA_RIRBSIZE    0x5E  /* RIRB Size */

/* Stream Descriptor Registers (offset from stream base) */
#define HDA_SD_CTL      0x00  /* Stream Descriptor Control */
#define HDA_SD_STS      0x03  /* Stream Descriptor Status */
#define HDA_SD_LPIB     0x04  /* Link Position In Buffer */
#define HDA_SD_CBL      0x08  /* Cyclic Buffer Length */
#define HDA_SD_LVI      0x0C  /* Last Valid Index */
#define HDA_SD_FIFOS    0x10  /* FIFO Size */
#define HDA_SD_FMT      0x12  /* Format */
#define HDA_SD_BDLPL    0x18  /* BDL Pointer Lower */
#define HDA_SD_BDLPU    0x1C  /* BDL Pointer Upper */

/* Output stream 0 base = 0x80 + (num_input_streams * 0x20) */
/* We compute this dynamically from GCAP */

/* GCTL bits */
#define HDA_GCTL_CRST   (1 << 0)  /* Controller Reset */

/* CORBCTL bits */
#define HDA_CORBCTL_RUN  (1 << 1)  /* CORB DMA Enable */

/* RIRBCTL bits */
#define HDA_RIRBCTL_RUN  (1 << 1)  /* RIRB DMA Enable */
#define HDA_RIRBCTL_INT  (1 << 0)  /* RIRB Interrupt Enable */

/* Stream CTL bits */
#define HDA_SD_CTL_RUN   (1 << 1)  /* Stream Run */
#define HDA_SD_CTL_IOCE  (1 << 2)  /* Interrupt on Completion Enable */
#define HDA_SD_CTL_STRIPE (1 << 4)

/* HDA Verb helpers */
#define HDA_VERB(cad, nid, verb, payload) \
    (((uint32_t)(cad) << 28) | ((uint32_t)(nid) << 20) | ((uint32_t)(verb) << 8) | (uint32_t)(payload))

#define HDA_VERB_12(cad, nid, verb, payload) \
    (((uint32_t)(cad) << 28) | ((uint32_t)(nid) << 20) | ((uint32_t)(verb) << 0))

/* Common Verb IDs (12-bit payload) */
#define HDA_VERB_GET_PARAM         0xF00
#define HDA_VERB_GET_CONN_LIST     0xF02
#define HDA_VERB_GET_CONN_SELECT   0xF01
#define HDA_VERB_SET_CONN_SELECT   0x701
#define HDA_VERB_GET_PIN_CTRL      0xF07
#define HDA_VERB_SET_PIN_CTRL      0x707
#define HDA_VERB_GET_EAPD          0xF0C
#define HDA_VERB_SET_EAPD          0x70C
#define HDA_VERB_GET_POWER_STATE   0xF05
#define HDA_VERB_SET_POWER_STATE   0x705
#define HDA_VERB_SET_CONV_FORMAT   0x200
#define HDA_VERB_SET_CONV_STREAM   0x706
#define HDA_VERB_SET_AMP_GAIN      0x300
#define HDA_VERB_GET_AMP_GAIN      0xB00

/* Parameters for GET_PARAM */
#define HDA_PARAM_VENDOR_ID        0x00
#define HDA_PARAM_REVISION_ID      0x02
#define HDA_PARAM_NODE_COUNT       0x04
#define HDA_PARAM_FN_GROUP_TYPE    0x05
#define HDA_PARAM_AUDIO_WIDGET_CAP 0x09
#define HDA_PARAM_PIN_CAP          0x0C
#define HDA_PARAM_CONN_LIST_LEN    0x0E
#define HDA_PARAM_OUT_AMP_CAP      0x12
#define HDA_PARAM_SUPPORTED_RATES  0x0A
#define HDA_PARAM_SUPPORTED_FMTS   0x0B

/* Widget types */
#define HDA_WIDGET_AO    0x0  /* Audio Output */
#define HDA_WIDGET_AI    0x1  /* Audio Input */
#define HDA_WIDGET_MIXER 0x2  /* Audio Mixer */
#define HDA_WIDGET_SEL   0x3  /* Audio Selector */
#define HDA_WIDGET_PIN   0x4  /* Pin Complex */
#define HDA_WIDGET_POWER 0x5  /* Power Widget */
#define HDA_WIDGET_VOL   0x6  /* Volume Knob */

/* BDL Entry */
struct hda_bdl_entry {
    uint64_t address;     /* Physical address of buffer */
    uint32_t length;      /* Buffer length in bytes */
    uint32_t flags;       /* Bit 0: IOC (Interrupt on Completion) */
} __attribute__((packed));

/* API */
void hda_init(void);
void hda_set_volume(uint8_t volume);
void hda_start_ring(void);
uint32_t hda_get_lpib(void);
uint8_t *hda_get_ring_buffer_virt(void);
void hda_stop(void);
bool hda_is_available(void);

#endif /* DRIVERS_HDA_H */
