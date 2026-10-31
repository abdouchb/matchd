#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define NO_SYS                     1
#define LWIP_SOCKET                0
#define LWIP_NETCONN               0
#define LWIP_NETIF_API             0

#define LWIP_ARP                   1
#define LWIP_ETHERNET              1
#define LWIP_IPV4                  1
#define LWIP_ICMP                  1
#define LWIP_UDP                   1
#define LWIP_TCP                   1
#define LWIP_DHCP                  1
#define LWIP_DNS                   1

/* Checksum offload workaround for QEMU SLIRP */
#define CHECKSUM_CHECK_IP          0
#define CHECKSUM_CHECK_UDP         0
#define CHECKSUM_CHECK_TCP         0
#define CHECKSUM_CHECK_ICMP        0

/* Memory configuration */
#define MEM_ALIGNMENT              8
#define MEM_SIZE                   (64 * 1024)
#define MEMP_NUM_PBUF              64
#define MEMP_NUM_TCP_PCB           16
#define MEMP_NUM_TCP_PCB_LISTEN    4
#define MEMP_NUM_TCP_SEG           64
#define PBUF_POOL_SIZE             64
#define PBUF_POOL_BUFSIZE          2000

/* TCP configuration */
#define TCP_MSS                    1460
#define TCP_WND                    (8 * TCP_MSS)
#define TCP_SND_BUF                (8 * TCP_MSS)

/* ARP/IP configuration */
#define IP_FORWARD                 0
#define IP_OPTIONS_ALLOWED         1
#define IP_REASSEMBLY              0
#define IP_FRAG                    0
#define ARP_TABLE_SIZE             10

/* System tick */
#define NO_SYS_NO_TIMERS           0
#define SYS_LIGHTWEIGHT_PROT       0

#endif /* LWIP_LWIPOPTS_H */
