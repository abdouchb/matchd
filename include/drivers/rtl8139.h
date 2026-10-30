/*
 * Zarqaa OS 0.1 - RTL8139 Network Driver
 */

#ifndef DRIVERS_RTL8139_H
#define DRIVERS_RTL8139_H

#include "zarqaa/types.h"

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

void rtl8139_init(void);
void rtl8139_send_packet(void *data, uint32_t len);
void rtl8139_receive_packet(void);

// For lwIP to poll
typedef void (*rtl8139_rx_callback_t)(void *data, uint32_t len);
void rtl8139_set_rx_callback(rtl8139_rx_callback_t cb);
void rtl8139_poll(void);

extern uint8_t rtl8139_mac[6];

#endif /* DRIVERS_RTL8139_H */
