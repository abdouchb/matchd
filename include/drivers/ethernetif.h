#ifndef DRIVERS_ETHERNETIF_H
#define DRIVERS_ETHERNETIF_H

#include "lwip/netif.h"
#include "lwip/err.h"

err_t ethernetif_init(struct netif *netif);
void ethernetif_poll(struct netif *netif);

extern struct netif netif;

#endif /* DRIVERS_ETHERNETIF_H */
