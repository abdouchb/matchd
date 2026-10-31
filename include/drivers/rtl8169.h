/*
 * Zarqaa OS 0.1 - RTL8169/8136 Gigabit Ethernet Driver
 */

#ifndef DRIVERS_RTL8169_H
#define DRIVERS_RTL8169_H

#include "zarqaa/types.h"

#define RTL8169_VENDOR_ID 0x10EC
#define RTL8169_DEVICE_ID 0x8169
#define RTL8168_DEVICE_ID 0x8168
#define RTL8136_DEVICE_ID 0x8136

extern bool rtl8169_found;
void rtl8169_init(void);
void rtl8169_send_packet(void *data, uint32_t len);

// For lwIP to poll
typedef void (*rtl8169_rx_callback_t)(void *data, uint32_t len);
void rtl8169_set_rx_callback(rtl8169_rx_callback_t cb);
void rtl8169_poll(void);

extern uint8_t rtl8169_mac[6];

#endif /* DRIVERS_RTL8169_H */
