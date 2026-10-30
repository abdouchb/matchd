#ifndef E1000_H
#define E1000_H

#include "zarqaa/types.h"

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E

typedef void (*e1000_rx_callback_t)(void *data, uint32_t len);

extern bool e1000_found;
void e1000_init(void);
void e1000_set_rx_callback(e1000_rx_callback_t cb);
void e1000_send_packet(const void *data, uint32_t len);
void e1000_poll(void);

extern uint8_t e1000_mac[6];
extern bool e1000_found;

#endif
