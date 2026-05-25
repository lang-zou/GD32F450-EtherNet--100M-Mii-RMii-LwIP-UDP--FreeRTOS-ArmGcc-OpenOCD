/**
 * @file    lwipopts.h
 * @brief   LwIP Stack Configuration — UDP-optimized for GD32F450
 *
 * This file overrides the default LwIP options in lwip/src/include/lwip/opt.h.
 * UDP-only configuration with ICMP (ping) support. TCP and advanced features
 * are disabled to minimize footprint.
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/*===========================================================================
 * OS Integration
 *===========================================================================*/
#define NO_SYS                  1     /* Current phase: LwIP raw API (set to 0 when using sockets with FreeRTOS) */
#define LWIP_NETCONN            0
#define LWIP_SOCKET             0
#define LWIP_RAW                0
#define SYS_LIGHTWEIGHT_PROT    0     /* No inter-task protection needed in raw API mode */

/*===========================================================================
 * Memory Pool Configuration
 * Tuned for UDP echo workload with moderate throughput
 *===========================================================================*/
#define MEM_ALIGNMENT           4
#define MEM_SIZE                (16 * 1024)
#define MEMP_NUM_PBUF           16
#define MEMP_NUM_UDP_PCB        8
#define MEMP_NUM_TCP_PCB        0
#define MEMP_NUM_TCP_PCB_LISTEN 0
#define MEMP_NUM_TCP_SEG        0
#define MEMP_NUM_SYS_TIMEOUT    8
#define MEMP_NUM_NETBUF         0
#define MEMP_NUM_NETCONN        0
#define MEMP_NUM_TCPIP_MSG_API  0
#define MEMP_NUM_TCPIP_MSG_INPKT 0

/*===========================================================================
 * Packet Buffer Configuration
 *===========================================================================*/
#define PBUF_POOL_SIZE          16
#define PBUF_POOL_BUFSIZE       1536
#define PBUF_LINK_HLEN          16
#define PBUF_POOL_BUFSIZE_ALIGNED 1536

/*===========================================================================
 * Protocol Support — UDP only + ICMP for ping
 *===========================================================================*/
#define LWIP_UDP                1
#define LWIP_TCP                0
#define LWIP_ICMP               1
#define LWIP_IGMP               0
#define LWIP_DHCP               0
#define LWIP_DNS                0
#define LWIP_AUTOIP             0
#define LWIP_IPV4               1
#define LWIP_IPV6               0
#define LWIP_ARP                1
#define LWIP_ETHERNET           1

/*===========================================================================
 * Checksum Offloading
 * GD32F450 ENET MAC supports hardware checksum offloading
 *===========================================================================*/
#define CHECKSUM_GEN_IP         0   /* Let hardware do it */
#define CHECKSUM_GEN_UDP        0   /* Let hardware do it */
#define CHECKSUM_GEN_TCP        0
#define CHECKSUM_GEN_ICMP       0
#define CHECKSUM_CHECK_IP       0
#define CHECKSUM_CHECK_UDP      0
#define CHECKSUM_CHECK_TCP      0
#define CHECKSUM_CHECK_ICMP     0

/*===========================================================================
 * Network Interface
 *===========================================================================*/
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_REMOVE_CALLBACK      0

/*===========================================================================
 * Performance Tuning
 *===========================================================================*/
#define LWIP_STATS              0
#define LWIP_STATS_DISPLAY       0
#define LWIP_DEBUG               0
#define LWIP_NOASSERT            1
#define LWIP_DONT_PROVIDE_BYTEOL 1

/*===========================================================================
 * Default Network Settings
 *===========================================================================*/
#define LWIP_LOCAL_HOSTNAME     "gd32f450"
#define IP_SOF_BROADCAST        1
#define IP_SOF_BROADCAST_RECV   1

/*===========================================================================
 * Hardware Checksum Offload
 * GD32F4 ENET supports IPv4 header + ICMP + UDP/TCP payload checksum
 *===========================================================================*/
#define LWIP_CHECKSUM_OFFLOAD 1

#endif /* LWIPOPTS_H */
