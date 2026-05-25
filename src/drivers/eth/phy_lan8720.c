/**
 * @file    phy_lan8720.c
 * @brief   Microchip LAN8720A PHY driver (RMII mode)
 *
 * PHY address: 0x00 (default, PHYAD0 pulled low on LAN8720 boards)
 * Features: 10/100 Mbps, auto-negotiation, RMII interface
 *
 * Key registers:
 *   BCR    (0x00): Basic Control Register
 *   BSR    (0x01): Basic Status Register
 *   PHYID1 (0x02): PHY Identifier 1 (expected: 0x0007)
 *   PHYID2 (0x03): PHY Identifier 2 (expected: 0xC0F1)
 *   ANAR   (0x04): Auto-Negotiation Advertisement
 *   ANLPAR (0x05): Auto-Negotiation Link Partner Ability
 *   SCSR   (0x1F): Special Control/Status Register (LAN8720-specific)
 */

#include "phy.h"
#include "board.h"
#include "gd32f4xx_enet.h"

/* LAN8720A identification */
#define LAN8720_PHY_ID1   0x0007
#define LAN8720_PHY_ID2   0xC0F1
#define LAN8720_PHY_ADDR  0x00

/* SCSR (0x1F) bits */
#define LAN8720_SCSR_SPEED100    (1 << 2)
#define LAN8720_SCSR_DUPLEX      (1 << 3)

/*===========================================================================
 * Public API
 *===========================================================================*/
bool phy_init(void)
{
    uint16_t reg_val;

    /* Hardware reset the PHY */
    phy_hardware_reset();

    /* Verify PHY ID */
    if (!phy_read_reg(LAN8720_PHY_ADDR, 0x02, &reg_val))
    {
        return false;
    }
    if (reg_val != LAN8720_PHY_ID1)
    {
        return false;
    }

    /* Enable auto-negotiation and restart */
    phy_write_reg(LAN8720_PHY_ADDR, 0x00, (1 << 12) | (1 << 9));

    return true;
}

bool phy_get_status(phy_status_t *status)
{
    uint16_t reg_val;

    if (status == NULL)
    {
        return false;
    }

    /* Read BSR (Basic Status Register) */
    if (!phy_read_reg(LAN8720_PHY_ADDR, 0x01, &reg_val))
    {
        status->link_up = false;
        return false;
    }

    /* Check link status (bit 2) */
    if (!(reg_val & (1 << 2)))
    {
        status->link_up = false;
        return false;
    }

    /* Read SCSR for speed and duplex */
    if (!phy_read_reg(LAN8720_PHY_ADDR, 0x1F, &reg_val))
    {
        status->link_up = true;
        status->speed  = 100;
        status->duplex = 1;
        return true;
    }

    status->link_up = true;
    status->speed   = (reg_val & LAN8720_SCSR_SPEED100) ? 100 : 10;
    status->duplex  = (reg_val & LAN8720_SCSR_DUPLEX) ? 1 : 0;

    return true;
}

void phy_hardware_reset(void)
{
    /* Enable PHY reset GPIO clock */
    rcu_periph_clock_enable(PHY_RESET_RCU_CLOCK);

    /* Configure reset pin as push-pull output */
    gpio_mode_set(PHY_RESET_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PHY_RESET_PIN);
    gpio_output_options_set(PHY_RESET_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                             PHY_RESET_PIN);

    /* Assert reset (active low) for at least 100 us */
    gpio_bit_reset(PHY_RESET_PORT, PHY_RESET_PIN);
    board_delay_ms(1);

    /* De-assert reset */
    gpio_bit_set(PHY_RESET_PORT, PHY_RESET_PIN);

    /* Wait for PHY to stabilize after reset (~100 ms for LAN8720) */
    board_delay_ms(100);
}

/*===========================================================================
 * SMI (MDC/MDIO) access via GD32 enet_phy_write_read()
 *===========================================================================*/
bool phy_read_reg(uint8_t phy_addr, uint8_t reg_addr, uint16_t *value)
{
    if (value == NULL)
    {
        return false;
    }
    return (SUCCESS == enet_phy_write_read(ENET_PHY_READ, phy_addr, reg_addr, value));
}

bool phy_write_reg(uint8_t phy_addr, uint8_t reg_addr, uint16_t value)
{
    uint16_t val = value;
    return (SUCCESS == enet_phy_write_read(ENET_PHY_WRITE, phy_addr, reg_addr, &val));
}
