/*
 * Zarqaa OS 0.1 - Simulated Network / WiFi Subsystem
 * Creator: Abdelhak Chabbi
 *
 * Provides a realistic WiFi simulation for the OS GUI.
 * Simulates scanning, connecting, signal strength fluctuation,
 * and network statistics.
 */

#include "drivers/network.h"
#include "drivers/pit.h"
#include "zarqaa/string.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "drivers/ethernetif.h"

static struct network_state state;

/* Built-in simulated WiFi networks */
static const struct {
    const char *ssid;
    int base_signal;
    bool encrypted;
} builtin_networks[] = {
    {"ZarqaaNet_5G",     95, true },
    {"SmartHome_IoT",    82, true },
    {"HomeNetwork",      78, true },
    {"Guest_Network",    70, false},
    {"CoffeeShop_Free",  62, false},
    {"Airport_Free",     55, false},
    {"Neighbors_WiFi",   45, true },
    {"DIRECT-TV",        33, true },
};
#define BUILTIN_COUNT 8

/* Simple pseudo-random based on PIT ticks */
int net_rand(int range) {
    static uint32_t seed = 0;
    if (seed == 0) seed = 0x12345678 ^ pit_get_ticks();
    seed = (1103515245 * seed + 12345) % 2147483648;
    return (seed % range);
}

uint32_t sys_now(void) {
    return pit_get_ticks(); // PIT ticks are 1000Hz (1ms)
}

void network_init(void) {
    memset(&state, 0, sizeof(state));
    state.status = NET_DISCONNECTED;
    state.connected_index = -1;
    state.pending_index = -1;
    state.network_count = 0;
    strcpy(state.ip_address, "0.0.0.0");
    strcpy(state.gateway, "0.0.0.0");
    state.speed_mbps = 0;

    /* Initialize lwIP */
    lwip_init();
    extern void dns_manager_init(void);
    dns_manager_init();

    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);

    netif_add(&netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, netif_input);
    netif_set_default(&netif);
    netif_set_up(&netif);

    dhcp_start(&netif);

    /* Let DHCP set the DNS server to QEMU's highly reliable host DNS (10.0.2.3) */
    
    /* Do an initial scan */
    network_scan();
}

int network_scan(void) {
    state.network_count = BUILTIN_COUNT;

    for (int i = 0; i < BUILTIN_COUNT; i++) {
        strcpy(state.networks[i].ssid, builtin_networks[i].ssid);
        /* Fluctuate signal slightly each scan */
        int fluct = net_rand(11) - 5; /* -5 to +5 */
        int sig = builtin_networks[i].base_signal + fluct;
        if (sig > 100) sig = 100;
        if (sig < 5) sig = 5;
        state.networks[i].signal = sig;
        state.networks[i].encrypted = builtin_networks[i].encrypted;
        state.networks[i].connected = (state.connected_index == i && state.status == NET_CONNECTED);
    }

    return state.network_count;
}

bool network_connect(int index) {
    if (index < 0 || index >= state.network_count) return false;
    if (state.status == NET_CONNECTED && state.connected_index == index) return true; /* already */

    /* Disconnect current if any */
    if (state.status == NET_CONNECTED) {
        state.networks[state.connected_index].connected = false;
    }

    state.status = NET_CONNECTING;
    state.pending_index = index;
    state.connect_timer = 150; /* ~1.5 seconds at 100Hz */
    return true;
}

void network_disconnect(void) {
    if (state.connected_index >= 0 && state.connected_index < state.network_count) {
        state.networks[state.connected_index].connected = false;
    }
    state.status = NET_DISCONNECTED;
    state.connected_index = -1;
    state.pending_index = -1;
    state.connect_timer = 0;
    strcpy(state.ip_address, "0.0.0.0");
    strcpy(state.gateway, "0.0.0.0");
    state.speed_mbps = 0;
    state.bytes_sent = 0;
    state.bytes_recv = 0;
}

#define MAX_NET_LISTENERS 8
static network_listener_cb net_listeners[MAX_NET_LISTENERS];
static int num_net_listeners = 0;
static bool dhcp_bound = false;

void network_register_listener(network_listener_cb callback) {
    for (int i = 0; i < num_net_listeners; i++) {
        if (net_listeners[i] == callback) {
            if (dhcp_bound) callback(NETWORK_EVENT_READY);
            return;
        }
    }
    if (num_net_listeners < MAX_NET_LISTENERS) {
        net_listeners[num_net_listeners++] = callback;
        if (dhcp_bound) {
            callback(NETWORK_EVENT_READY);
        }
    }
}

static void fire_network_event(network_event_t event) {
    for (int i = 0; i < num_net_listeners; i++) {
        if (net_listeners[i]) net_listeners[i](event);
    }
}

void network_tick(void) {
    /* Process lwIP timeouts and network packets */
    ethernetif_poll(&netif);
    sys_check_timeouts();

    if (dhcp_supplied_address(&netif)) {
        if (!dhcp_bound) {
            dhcp_bound = true;
            uint8_t *ip = (uint8_t *)&netif.ip_addr;
            kprintf("[lwIP] DHCP Bound: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
            
            strcpy(state.ip_address, "10.0.2.15");
            strcpy(state.gateway, "10.0.2.2");
            
            fire_network_event(NETWORK_EVENT_READY);
        }
    } else {
        if (dhcp_bound) {
            dhcp_bound = false;
            fire_network_event(NETWORK_EVENT_DISCONNECTED);
        }
    }

    if (state.status == NET_CONNECTING) {
        if (state.connect_timer > 0) {
            state.connect_timer--;
        } else {
            state.status = NET_CONNECTED;
            state.connected_index = state.pending_index;
            state.pending_index = -1;
            state.networks[state.connected_index].connected = true;

            strcpy(state.ip_address, "10.0.2.15");
            strcpy(state.gateway, "10.0.2.2");
            state.speed_mbps = 866;
            state.bytes_sent = 0;
            state.bytes_recv = 0;
        }
    } else if (state.status == NET_CONNECTED) {
        // Randomly bump data stats to look alive
        if (net_rand(10) == 0) {
            state.bytes_recv += net_rand(15000);
            state.bytes_sent += net_rand(3000);
        }
    }
}

struct network_state *network_get_state(void) {
    return &state;
}

int network_get_signal(void) {
    if (state.status != NET_CONNECTED || state.connected_index < 0) return 0;
    return state.networks[state.connected_index].signal;
}

const char *network_get_ssid(void) {
    if (state.status != NET_CONNECTED || state.connected_index < 0) return "";
    return state.networks[state.connected_index].ssid;
}
