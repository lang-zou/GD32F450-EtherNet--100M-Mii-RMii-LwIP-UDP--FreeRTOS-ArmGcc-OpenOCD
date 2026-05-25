/**
 * @file    ethernetif.h
 * @brief   LwIP network interface port for GD32F450 ENET MAC
 */
#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ethernet network interface for LwIP
 * Called by netif_add() to set up the low-level hardware
 */
err_t ethernetif_init(struct netif *netif);

/**
 * @brief Process received frames — call periodically from LwIP thread
 * Reads all pending frames from ENET DMA and feeds them to LwIP stack.
 */
void ethernetif_input(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNETIF_H */
