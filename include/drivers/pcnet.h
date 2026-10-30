#ifndef PCNET_H
#define PCNET_H

#include "zarqaa/types.h"

#define PCNET_VENDOR_ID 0x1022
#define PCNET_DEVICE_ID 0x2000

typedef void (*pcnet_rx_callback_t)(void *data, uint32_t len);

bool pcnet_init(void);
void pcnet_set_rx_callback(pcnet_rx_callback_t cb);
void pcnet_send_packet(const void *data, uint32_t len);
void pcnet_poll(void);

extern uint8_t pcnet_mac[6];
extern bool pcnet_found;

#endif
