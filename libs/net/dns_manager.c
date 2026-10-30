#include "net/dns_manager.h"
#include "lwip/dns.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

#define DNS_CACHE_SIZE 32

typedef struct {
    char hostname[64];
    ip_addr_t ipaddr;
    uint32_t expires_at;
    bool valid;
} dns_cache_entry_t;

static dns_cache_entry_t dns_cache[DNS_CACHE_SIZE];

typedef struct dns_req {
    char hostname[64];
    dns_manager_cb_t callback;
    void *arg;
    struct dns_req *next;
} dns_req_t;

static dns_req_t *active_requests = NULL;

void dns_manager_init(void) {
    memset(dns_cache, 0, sizeof(dns_cache));
    active_requests = NULL;
}

static void lwip_dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    (void)callback_arg;
    
    // Add to cache if valid
    if (ipaddr) {
        for (int i = 0; i < DNS_CACHE_SIZE; i++) {
            if (!dns_cache[i].valid || strcmp(dns_cache[i].hostname, name) == 0) {
                strncpy(dns_cache[i].hostname, name, 63);
                dns_cache[i].hostname[63] = '\0';
                dns_cache[i].ipaddr = *ipaddr;
                dns_cache[i].valid = true;
                break;
            }
        }
    }
    
    // Find all requests for this name and detach them from the list FIRST
    dns_req_t *matched_reqs = NULL;
    
    __asm__ volatile ("cli");
    dns_req_t *prev = NULL;
    dns_req_t *curr = active_requests;
    while (curr) {
        if (strcmp(curr->hostname, name) == 0) {
            dns_req_t *to_detach = curr;
            if (prev) {
                prev->next = curr->next;
            } else {
                active_requests = curr->next;
            }
            curr = curr->next;
            
            // Add to matched_reqs
            to_detach->next = matched_reqs;
            matched_reqs = to_detach;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
    __asm__ volatile ("sti");
    
    // Trigger callbacks safely outside the list
    curr = matched_reqs;
    while (curr) {
        curr->callback(name, ipaddr, curr->arg);
        dns_req_t *to_free = curr;
        curr = curr->next;
        kfree(to_free);
    }
}

void dns_manager_resolve(const char *hostname, dns_manager_cb_t callback, void *arg) {
    // 1. Check cache
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (dns_cache[i].valid && strcmp(dns_cache[i].hostname, hostname) == 0) {
            callback(hostname, &dns_cache[i].ipaddr, arg);
            return;
        }
    }
    
    // 2. Add to request queue
    dns_req_t *req = kmalloc(sizeof(dns_req_t));
    if (!req) {
        callback(hostname, NULL, arg);
        return;
    }
    strncpy(req->hostname, hostname, 63);
    req->hostname[63] = '\0';
    req->callback = callback;
    req->arg = arg;
    
    __asm__ volatile ("cli");
    req->next = active_requests;
    active_requests = req;
    __asm__ volatile ("sti");
    
    // 3. Initiate lwIP resolution
    ip_addr_t addr;
    err_t err = dns_gethostbyname(hostname, &addr, lwip_dns_found, NULL);
    if (err == ERR_OK) {
        lwip_dns_found(hostname, &addr, NULL);
    } else if (err != ERR_INPROGRESS) {
        lwip_dns_found(hostname, NULL, NULL);
    }
}
