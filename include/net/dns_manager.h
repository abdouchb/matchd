#ifndef NET_DNS_MANAGER_H
#define NET_DNS_MANAGER_H

#include "lwip/ip_addr.h"
#include "zarqaa/types.h"

typedef void (*dns_manager_cb_t)(const char *name, const ip_addr_t *ipaddr, void *arg);

void dns_manager_init(void);
void dns_manager_resolve(const char *hostname, dns_manager_cb_t callback, void *arg);

#endif
