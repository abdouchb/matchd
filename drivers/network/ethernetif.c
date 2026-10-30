#include "drivers/ethernetif.h"
#include "drivers/rtl8139.h"
#include "drivers/rtl8168.h"
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ppp/pppoe.h"
#include "zarqaa/string.h"

struct netif netif;
static struct netif *current_netif = NULL;

static err_t low_level_output(struct netif *netif, struct pbuf *p) {
    struct pbuf *q;
    char buffer[1514];
    int len = 0;

    for (q = p; q != NULL; q = q->next) {
        memcpy(buffer + len, q->payload, q->len);
        len += q->len;
    }

    if (rtl8168_is_found) {
        rtl8168_send_packet(buffer, len);
    } else {
        rtl8139_send_packet(buffer, len);
    }
    return ERR_OK;
}

static void rx_cb(void *data, uint32_t len) {
    struct pbuf *p, *q;
    
    if (current_netif == NULL) return;

    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    kprintf("[ETH] rx_cb len=%u pbuf=%p\n", len, p);
    if (p != NULL) {
        kprintf("[ETH] pbuf->tot_len=%u p->len=%u\n", p->tot_len, p->len);
        int offset = 0;
        for (q = p; q != NULL; q = q->next) {
            memcpy(q->payload, (char *)data + offset, q->len);
            offset += q->len;
        }

        if (current_netif->input(p, current_netif) != ERR_OK) {
            kprintf("[ETH] netif input returned ERR\n");
            pbuf_free(p);
        } else {
            kprintf("[ETH] netif input accepted pbuf\n");
        }
    }
}

err_t ethernetif_init(struct netif *netif) {
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    // Try RTL8168 first (real hardware priority)
    if (rtl8168_init()) {
        netif->hwaddr_len = 6;
        for (int i = 0; i < 6; i++) netif->hwaddr[i] = rtl8168_mac[i];
        rtl8168_set_rx_callback(rx_cb);
    } else {
        rtl8139_init();
        netif->hwaddr_len = 6;
        for (int i = 0; i < 6; i++) netif->hwaddr[i] = rtl8139_mac[i];
        rtl8139_set_rx_callback(rx_cb);
    }

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    current_netif = netif;

    return ERR_OK;
}

void ethernetif_poll(struct netif *netif) {
    if (rtl8168_is_found) {
        rtl8168_poll();
    } else {
        rtl8139_poll();
    }
}
