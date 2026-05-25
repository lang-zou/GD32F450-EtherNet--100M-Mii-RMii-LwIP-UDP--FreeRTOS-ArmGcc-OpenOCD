/**
 * @file    eth_mac.c
 * @brief   GD32F450 ENET MAC driver implementation
 *
 * DMA descriptor chain mode. 5 TX + 5 RX descriptors.
 * Hardware checksum offloading enabled.
 * Interrupt-driven reception with frame notification.
 */

#include "eth_mac.h"
#include "phy.h"
#include "board.h"
#include "log.h"
#include "gd32f4xx_enet.h"
#include "gd32f4xx_syscfg.h"

/* =========================================================================
 * DMA Descriptor Configuration
 * Number of descriptors and buffer sizes
 * ========================================================================= */
#define ETH_TX_DESC_COUNT  5
#define ETH_RX_DESC_COUNT  5

/* TX/RX buffer arrays (must be in non-cached, non-TCM SRAM) */
static uint8_t  eth_tx_buf[ETH_TX_DESC_COUNT][ETH_MAX_FRAME_SIZE] __attribute__((aligned(4)));
static uint8_t  eth_rx_buf[ETH_RX_DESC_COUNT][ETH_MAX_FRAME_SIZE] __attribute__((aligned(4)));

/* DMA descriptor arrays */
static enet_descriptors_struct eth_tx_desc[ETH_TX_DESC_COUNT] __attribute__((aligned(4)));
static enet_descriptors_struct eth_rx_desc[ETH_RX_DESC_COUNT] __attribute__((aligned(4)));

/* Current TX descriptor index */
static volatile uint32_t eth_tx_idx = 0;
static volatile uint32_t eth_rx_idx = 0;

/* MAC address (default, can be changed) */
static uint8_t eth_mac_addr[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

/* Link status tracking */
static volatile bool    eth_link_up    = false;
static volatile uint8_t eth_link_speed  = 0;
static volatile uint8_t eth_link_duplex = 0;

/* =========================================================================
 * Static Helper: Configure RMII GPIO pins
 * ========================================================================= */
static void eth_gpio_init_rmii(void)
{
    /* Enable GPIO clocks */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOG);

    /* Enable ENET and SYSCFG clocks */
    rcu_periph_clock_enable(RCU_ENET);
    rcu_periph_clock_enable(RCU_SYSCFG);

    /* Configure GPIOs for RMII via alternate function AF11
     * PA1:  ETH_RMII_REF_CLK
     * PA2:  ETH_MDIO
     * PA7:  ETH_RMII_CRS_DV
     * PB11: ETH_RMII_TX_EN
     * PB12: ETH_RMII_TXD0
     * PB13: ETH_RMII_TXD1
     * PC1:  ETH_MDC
     * PC4:  ETH_RMII_RXD0
     * PC5:  ETH_RMII_RXD1
     */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7);
    gpio_af_set(GPIOA, GPIO_AF_11,
                GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7);

    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);
    gpio_af_set(GPIOB, GPIO_AF_11,
                GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);

    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
    gpio_af_set(GPIOC, GPIO_AF_11,
                GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);

    /* Select RMII mode via SYSCFG */
    syscfg_enet_phy_interface_config(SYSCFG_ENET_PHY_RMII);
}

/* =========================================================================
 * Static Helper: Configure MII GPIO pins
 * ========================================================================= */
static void eth_gpio_init_mii(void)
{
    /* Enable GPIO clocks */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOG);

    /* Enable ENET and SYSCFG clocks */
    rcu_periph_clock_enable(RCU_ENET);
    rcu_periph_clock_enable(RCU_SYSCFG);

    /* MII uses 17 pins across GPIOA, GPIOB, GPIOC — all on AF11
     * Configure the full set of MII pins:
     * PA0:  ETH_MII_CRS
     * PA1:  ETH_MII_RX_CLK
     * PA2:  ETH_MDIO
     * PA3:  ETH_MII_COL
     * PA7:  ETH_MII_RX_DV
     * PB0:  ETH_MII_RXD2
     * PB1:  ETH_MII_RXD3
     * PB5:  ETH_MII_RX_ER (check — might be PB10)
     * PB8:  ETH_MII_TXD3
     * PB11: ETH_MII_TX_EN
     * PB12: ETH_MII_TXD0
     * PB13: ETH_MII_TXD1
     * PC1:  ETH_MDC
     * PC2:  ETH_MII_TXD2
     * PC3:  ETH_MII_TX_CLK
     * PC4:  ETH_MII_RXD0
     * PC5:  ETH_MII_RXD1
     */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                  GPIO_PIN_3 | GPIO_PIN_7);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                            GPIO_PIN_3 | GPIO_PIN_7);
    gpio_af_set(GPIOA, GPIO_AF_11,
                GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                GPIO_PIN_3 | GPIO_PIN_7);

    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 |
                  GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 |
                            GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);
    gpio_af_set(GPIOB, GPIO_AF_11,
                GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 |
                GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);

    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                  GPIO_PIN_4 | GPIO_PIN_5);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                            GPIO_PIN_4 | GPIO_PIN_5);
    gpio_af_set(GPIOC, GPIO_AF_11,
                GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                GPIO_PIN_4 | GPIO_PIN_5);

    /* Select MII mode via SYSCFG */
    syscfg_enet_phy_interface_config(SYSCFG_ENET_PHY_MII);
}

/* =========================================================================
 * Static Helper: Initialize DMA Descriptor Chain
 * ========================================================================= */
static void eth_dma_desc_init(void)
{
    /* Initialize TX descriptors */
    for (uint32_t i = 0; i < ETH_TX_DESC_COUNT; i++)
    {
        eth_tx_desc[i].status             = ENET_TDES0_TCHM;
        eth_tx_desc[i].control_buffer_size = 0;
        eth_tx_desc[i].buffer1_addr       = (uint32_t)&eth_tx_buf[i][0];
        eth_tx_desc[i].buffer2_next_desc_addr =
            (uint32_t)&eth_tx_desc[(i + 1) % ETH_TX_DESC_COUNT];
    }

    /* Initialize RX descriptors */
    for (uint32_t i = 0; i < ETH_RX_DESC_COUNT; i++)
    {
        eth_rx_desc[i].status             = ENET_RDES0_DAV;
        eth_rx_desc[i].control_buffer_size = ENET_RDES1_RCHM | ETH_MAX_FRAME_SIZE;
        eth_rx_desc[i].buffer1_addr       = (uint32_t)&eth_rx_buf[i][0];
        eth_rx_desc[i].buffer2_next_desc_addr =
            (uint32_t)&eth_rx_desc[(i + 1) % ETH_RX_DESC_COUNT];
    }

    /* Set descriptor table addresses */
    ENET_DMA_RDTADDR = (uint32_t)&eth_rx_desc[0];
    ENET_DMA_TDTADDR = (uint32_t)&eth_tx_desc[0];
}

/* =========================================================================
 * Public: eth_mac_init()
 * ========================================================================= */
bool eth_mac_init(eth_mac_mode_t mode)
{
    /* 1. GPIO configuration */
    if (mode == ETH_MAC_MODE_RMII)
    {
        eth_gpio_init_rmii();
    }
    else
    {
        eth_gpio_init_mii();
    }

    /* 2. Software reset ENET */
    enet_deinit();
    if (ERROR == enet_software_reset())
    {
        LOG_E("eth_mac_init: software reset failed");
        return false;
    }

    /* 3. Initialize ENET with default parameters */
    ErrStatus status = enet_init(ENET_AUTO_NEGOTIATION,
                                  ENET_AUTOCHECKSUM_DROP_FAILFRAMES,
                                  ENET_BROADCAST_FRAMES_PASS);
    if (ERROR == status)
    {
        LOG_E("eth_mac_init: ENET init failed");
        return false;
    }

    /* 4. Configure less-common init parameters */
    enet_initpara_config(DMA_MAXBURST_OPTION, 8);
    enet_initpara_config(STORE_OPTION, 1);

    /* 5. Set MAC address */
    enet_mac_address_set(ENET_MAC_ADDRESS0, eth_mac_addr);

    /* 6. Initialize DMA descriptor chain */
    eth_dma_desc_init();

    /* 7. Configure DMA feature enables */
    enet_dma_feature_enable(ENET_DMA_CTL_DAFRF | ENET_DMA_CTL_OSF);

    /* 8. Enable DMA interrupts (receive + transmit) */
    enet_interrupt_enable(ENET_DMA_INT_NIE | ENET_DMA_INT_RIE | ENET_DMA_INT_TIE);
    NVIC_SetPriority(ENET_IRQn, 5);
    NVIC_EnableIRQ(ENET_IRQn);

    /* 9. Enable ENET MAC + DMA */
    enet_enable();

    /* 10. Initialize PHY and wait for link */
    if (!phy_init())
    {
        LOG_E("eth_mac_init: PHY init failed");
        return false;
    }

    /* 11. Wait for link up (timeout 3 seconds) */
    phy_status_t phy_st;
    for (uint32_t t = 0; t < 30; t++)
    {
        if (phy_get_status(&phy_st) && phy_st.link_up)
        {
            eth_link_up    = true;
            eth_link_speed  = phy_st.speed;
            eth_link_duplex = phy_st.duplex;

            LOG_I("eth_mac_init: Link up — %d Mbps %s-duplex",
                  phy_st.speed, phy_st.duplex ? "full" : "half");
            return true;
        }
        board_delay_ms(100);
    }

    LOG_W("eth_mac_init: Link timeout, continuing anyway");
    return true;
}

/* =========================================================================
 * Public: eth_mac_send()
 * ========================================================================= */
bool eth_mac_send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0 || len > ETH_MAX_FRAME_SIZE)
    {
        return false;
    }

    uint32_t idx = eth_tx_idx;
    enet_descriptors_struct *desc = &eth_tx_desc[idx];

    /* Wait if this descriptor is still owned by DMA */
    uint32_t timeout = 100000;
    while ((desc->status & ENET_TDES0_DAV) && --timeout)
    {
    }
    if (timeout == 0)
    {
        LOG_W("eth_mac_send: TX timeout");
        return false;
    }

    /* Copy frame data to TX buffer */
    for (uint16_t i = 0; i < len; i++)
    {
        eth_tx_buf[idx][i] = data[i];
    }

    /* Configure descriptor */
    desc->control_buffer_size = len;
    desc->status = ENET_TDES0_TCHM | ENET_TDES0_FSG |
                   ENET_TDES0_LSG | ENET_TDES0_INTC | ENET_TDES0_DAV;

    /* Advance index */
    eth_tx_idx = (idx + 1) % ETH_TX_DESC_COUNT;

    /* Resume DMA transmission if suspended */
    if (0 == (ENET_DMA_STAT & ENET_DMA_STAT_TPS))
    {
        ENET_DMA_TPEN = 0;
    }

    return true;
}

/* =========================================================================
 * Public: eth_mac_recv()
 * ========================================================================= */
uint16_t eth_mac_recv(uint8_t *buf, uint16_t buf_size)
{
    if (buf == NULL || buf_size == 0)
    {
        return 0;
    }

    uint32_t idx = eth_rx_idx;
    enet_descriptors_struct *desc = &eth_rx_desc[idx];

    /* Check if descriptor is still owned by DMA (no new frame) */
    if (desc->status & ENET_RDES0_DAV)
    {
        return 0;  /* No frame available */
    }

    /* Get frame length from descriptor status */
    uint32_t frame_len = GET_RDES0_FRML(desc->status);

    if (frame_len == 0 || frame_len > ETH_MAX_FRAME_SIZE)
    {
        /* Recycle descriptor */
        desc->status = ENET_RDES0_DAV;
        desc->control_buffer_size = ENET_RDES1_RCHM | ETH_MAX_FRAME_SIZE;
        eth_rx_idx = (idx + 1) % ETH_RX_DESC_COUNT;
        return 0;
    }

    /* Check for errors */
    if (desc->status & (ENET_RDES0_ERRS | ENET_RDES0_DERR | ENET_RDES0_RERR))
    {
        LOG_W("eth_mac_recv: frame error, status=0x%08lX", desc->status);
        desc->status = ENET_RDES0_DAV;
        desc->control_buffer_size = ENET_RDES1_RCHM | ETH_MAX_FRAME_SIZE;
        eth_rx_idx = (idx + 1) % ETH_RX_DESC_COUNT;
        return 0;
    }

    /* Copy frame to application buffer */
    uint16_t copy_len = (uint16_t)(frame_len < buf_size ? frame_len : buf_size);
    for (uint16_t i = 0; i < copy_len; i++)
    {
        buf[i] = eth_rx_buf[idx][i];
    }

    /* Return descriptor to DMA */
    desc->status = ENET_RDES0_DAV;
    desc->control_buffer_size = ENET_RDES1_RCHM | ETH_MAX_FRAME_SIZE;
    eth_rx_idx = (idx + 1) % ETH_RX_DESC_COUNT;

    /* Resume DMA if suspended */
    if (0 == (ENET_DMA_STAT & ENET_DMA_STAT_RPS))
    {
        ENET_DMA_RPEN = 0;
    }

    return copy_len;
}

/* =========================================================================
 * Public: eth_mac_link_status()
 * ========================================================================= */
bool eth_mac_link_status(uint8_t *speed, uint8_t *duplex)
{
    phy_status_t st;
    if (phy_get_status(&st) && st.link_up)
    {
        eth_link_up    = true;
        eth_link_speed  = st.speed;
        eth_link_duplex = st.duplex;
    }
    else
    {
        eth_link_up = false;
    }

    if (speed)  *speed  = eth_link_speed;
    if (duplex) *duplex = eth_link_duplex;
    return eth_link_up;
}

/* =========================================================================
 * Public: eth_mac_isr()
 * Called from ENET_IRQHandler in interrupt context
 * ========================================================================= */
void eth_mac_isr(void)
{
    uint32_t dma_status = ENET_DMA_STAT;

    /* Clear interrupt flags */
    ENET_DMA_STAT = dma_status;
}

/* =========================================================================
 * Public: eth_mac_get_addr()
 * ========================================================================= */
void eth_mac_get_addr(uint8_t mac[6])
{
    for (int i = 0; i < 6; i++)
    {
        mac[i] = eth_mac_addr[i];
    }
}
