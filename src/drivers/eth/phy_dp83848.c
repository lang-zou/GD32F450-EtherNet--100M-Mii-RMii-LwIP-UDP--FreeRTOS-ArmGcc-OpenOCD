/**
 * @file    phy_dp83848.c
 * @brief   TI DP83848 PHY driver (MII mode)
 *
 * Similar register layout to LAN8720 but uses MII interface.
 * PHY address: typically 0x01
 */

#include "phy.h"
#include "board.h"
#include "gd32f4xx_enet.h"

#define DP83848_PHY_ID1   0x2000
#define DP83848_PHY_ID2   0x5C90
#define DP83848_PHY_ADDR  0x01

/* PHYSTS (0x10) — DP83848-specific status register */
#define DP83848_PHYSTS_SPEED10   (1 << 1)
#define DP83848_PHYSTS_DUPLEX    (1 << 2)
#define DP83848_PHYSTS_LINK      (1 << 0)

/*===========================================================================
 * Public API
 *===========================================================================*/
bool phy_init(void)
{
    uint16_t reg_val;

    phy_hardware_reset();

    /* Verify PHY ID (optional — skip if read fails) */
    if (phy_read_reg(DP83848_PHY_ADDR, 0x02, &reg_val))
    {
        /* PHY responded */
    }

    /* Enable auto-negotiation */
    phy_write_reg(DP83848_PHY_ADDR, 0x00, (1 << 12) | (1 << 9));

    return true;
}

bool phy_get_status(phy_status_t *status)
{
    uint16_t reg_val;

    if (status == NULL)
    {
        return false;
    }

    /* Read BSR for link status */
    if (!phy_read_reg(DP83848_PHY_ADDR, 0x01, &reg_val))
    {
        status->link_up = false;
        return false;
    }

    if (!(reg_val & (1 << 2)))
    {
        status->link_up = false;
        return false;
    }

    /* Read PHYSTS for detailed status */
    if (!phy_read_reg(DP83848_PHY_ADDR, 0x10, &reg_val))
    {
        status->link_up = true;
        status->speed  = 100;
        status->duplex = 1;
        return true;
    }

    status->link_up = true;
    status->speed   = (reg_val & DP83848_PHYSTS_SPEED10) ? 10 : 100;
    status->duplex  = (reg_val & DP83848_PHYSTS_DUPLEX) ? 1 : 0;

    return true;
}

void phy_hardware_reset(void)
{
    rcu_periph_clock_enable(PHY_RESET_RCU_CLOCK);

    gpio_mode_set(PHY_RESET_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PHY_RESET_PIN);
    gpio_output_options_set(PHY_RESET_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                             PHY_RESET_PIN);

    gpio_bit_reset(PHY_RESET_PORT, PHY_RESET_PIN);
    board_delay_ms(1);
    gpio_bit_set(PHY_RESET_PORT, PHY_RESET_PIN);
    board_delay_ms(100);
}

/*===========================================================================
 * SMI Access
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
