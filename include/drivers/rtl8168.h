/*
 * Zarqaa OS 0.1 - RTL8168 / RTL810xE Network Driver
 */

#ifndef DRIVERS_RTL8168_H
#define DRIVERS_RTL8168_H

#include "zarqaa/types.h"

#define RTL8168_VENDOR_ID 0x10EC
#define RTL8168_DEVICE_ID 0x8168
#define RTL8136_DEVICE_ID 0x8136

bool rtl8168_init(void);
void rtl8168_send_packet(void *data, uint32_t len);

// For lwIP to poll
typedef void (*rtl8168_rx_callback_t)(void *data, uint32_t len);
void rtl8168_set_rx_callback(rtl8168_rx_callback_t cb);
void rtl8168_poll(void);

extern uint8_t rtl8168_mac[6];
extern bool rtl8168_is_found;

#endif /* DRIVERS_RTL8168_H */
