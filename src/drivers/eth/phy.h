/**
 * @file    phy.h
 * @brief   Ethernet PHY common interface
 *
 * Supports both RMII and MII PHY chips via a unified API.
 * Compile-time selection via ETH_PHY_MODE in app_config.h.
 */

#ifndef PHY_H_
#define PHY_H_

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * PHY Status Structure
 *===========================================================================*/
typedef struct
{
    bool     link_up;
    uint8_t  speed;      /* 10 or 100 (Mbps) */
    uint8_t  duplex;     /* 0 = half, 1 = full */
} phy_status_t;

/*===========================================================================
 * PHY Operations Interface
 *===========================================================================*/
/**
 * @brief Initialize the PHY chip (reset + auto-negotiation)
 * @return true on success
 */
bool phy_init(void);

/**
 * @brief Poll PHY link status after auto-negotiation
 * @param status Pointer to phy_status_t to fill
 * @return true if link is up and status was read
 */
bool phy_get_status(phy_status_t *status);

/**
 * @brief Hardware reset the PHY via GPIO pin
 */
void phy_hardware_reset(void);

/**
 * @brief Read a PHY register via SMI (MDC/MDIO)
 * @param phy_addr  PHY address (0-31)
 * @param reg_addr  Register address (0-31)
 * @param value     Pointer to store read value
 * @return true on success
 */
bool phy_read_reg(uint8_t phy_addr, uint8_t reg_addr, uint16_t *value);

/**
 * @brief Write a PHY register via SMI
 * @return true on success
 */
bool phy_write_reg(uint8_t phy_addr, uint8_t reg_addr, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* PHY_H_ */
