/**
 * @file    eth_mac.h
 * @brief   GD32F450 ENET MAC driver — RMII/MII dual-mode with DMA descriptors
 */

#ifndef ETH_MAC_H_
#define ETH_MAC_H_

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* Maximum Ethernet frame size (including VLAN tag) */
#define ETH_MAX_FRAME_SIZE  1524

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Types
 *===========================================================================*/
typedef enum
{
    ETH_MAC_MODE_RMII = 0,
    ETH_MAC_MODE_MII  = 1,
} eth_mac_mode_t;

/*===========================================================================
 * Core API
 *===========================================================================*/

/**
 * @brief Initialize Ethernet MAC + DMA + PHY
 * @param mode  RMII or MII
 * @return true on success
 */
bool eth_mac_init(eth_mac_mode_t mode);

/**
 * @brief Send an Ethernet frame (blocking)
 * @param data  Pointer to frame data (includes Ethernet header, excludes FCS)
 * @param len   Frame length in bytes
 * @return true on success
 */
bool eth_mac_send(const uint8_t *data, uint16_t len);

/**
 * @brief Receive an Ethernet frame (polling, non-blocking)
 * @param buf      Buffer to store received frame
 * @param buf_size Size of the buffer
 * @return Number of bytes received (0 if no frame available)
 */
uint16_t eth_mac_recv(uint8_t *buf, uint16_t buf_size);

/**
 * @brief Get current link status
 * @param speed   Pointer to store speed (10/100 Mbps)
 * @param duplex  Pointer to store duplex (0=half, 1=full)
 * @return true if link is up
 */
bool eth_mac_link_status(uint8_t *speed, uint8_t *duplex);

/**
 * @brief ENET interrupt handler — call from ENET_IRQHandler()
 */
void eth_mac_isr(void);

/**
 * @brief Get MAC address (set during init)
 */
void eth_mac_get_addr(uint8_t mac[6]);

#ifdef __cplusplus
}
#endif

#endif /* ETH_MAC_H_ */
