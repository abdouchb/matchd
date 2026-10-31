/*
 * Zarqaa OS 0.1 - Network Subsystem Header
 * Creator: Abdelhak Chabbi
 */

#ifndef DRIVERS_NETWORK_H
#define DRIVERS_NETWORK_H

#include "zarqaa/types.h"

#define MAX_NETWORKS 12
#define SSID_MAX_LEN 32

struct wifi_network {
    char ssid[SSID_MAX_LEN];
    int signal;       /* 0-100 */
    bool encrypted;
    bool connected;
};

enum net_status {
    NET_DISCONNECTED,
    NET_CONNECTING,
    NET_CONNECTED
};

struct network_state {
    enum net_status status;
    int connected_index;       /* index into scanned list, -1 if none */
    int network_count;
    struct wifi_network networks[MAX_NETWORKS];
    int connect_timer;         /* ticks remaining for connection animation */
    int pending_index;         /* index being connected */
    uint32_t bytes_sent;
    uint32_t bytes_recv;
    char ip_address[16];
    char gateway[16];
    int speed_mbps;
};

typedef enum {
    NETWORK_EVENT_READY,
    NETWORK_EVENT_DISCONNECTED
} network_event_t;

typedef void (*network_listener_cb)(network_event_t event);

void network_init(void);
void network_tick(void);                   /* call from timer or main loop */
int  network_scan(void);                   /* returns number of networks found */
bool network_connect(int index);           /* start connecting to network */
void network_disconnect(void);
struct network_state *network_get_state(void);
int  network_get_signal(void);             /* current signal 0-100, 0 if disconnected */
const char *network_get_ssid(void);        /* current SSID or "" */
void network_register_listener(network_listener_cb callback);

#endif /* DRIVERS_NETWORK_H */
