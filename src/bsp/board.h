/**
 * @file    board.h
 * @brief   Board-level hardware definitions for GD32F450IK6
 *
 * Central place for all pin mappings, clock configuration, and board-specific
 * settings. Change pin assignments here when adapting to different hardware.
 */

#ifndef BOARD_H_
#define BOARD_H_

#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Clock Configuration
 * HSE: 25 MHz → PLL → 200 MHz SYSCLK
 * AHB:  200 MHz
 * APB1: 50 MHz  (max 50 MHz)
 * APB2: 100 MHz (max 100 MHz)
 *===========================================================================*/
#define HSE_VALUE          ((uint32_t)25000000)
#define HSI_VALUE          ((uint32_t)16000000)
#define SYSTEM_CLOCK       ((uint32_t)200000000)

/*===========================================================================
 * LED Pin Definitions
 * Default: PC6 (commonly used on GD32 dev boards)
 * Adjust based on your actual board schematic
 *===========================================================================*/
#define LED_PORT           GPIOC
#define LED_PIN            GPIO_PIN_6
#define LED_RCU_CLOCK      RCU_GPIOC
#define LED_ON()           gpio_bit_set(LED_PORT, LED_PIN)
#define LED_OFF()          gpio_bit_reset(LED_PORT, LED_PIN)
#define LED_TOGGLE()       gpio_bit_toggle(LED_PORT, LED_PIN)

/*===========================================================================
 * Boot Button Pin
 * Hold this button during reset to enter bootloader mode
 * Default: PA0 (commonly available as user button)
 *===========================================================================*/
#define BOOT_BUTTON_PORT      GPIOA
#define BOOT_BUTTON_PIN       GPIO_PIN_0
#define BOOT_BUTTON_RCU_CLOCK RCU_GPIOA
#define BOOT_BUTTON_PRESSED() (gpio_input_bit_get(BOOT_BUTTON_PORT, BOOT_BUTTON_PIN) == RESET)

/*===========================================================================
 * PHY Reset Pin
 * Used to hardware-reset the Ethernet PHY chip
 * Default: PD3
 *===========================================================================*/
#define PHY_RESET_PORT        GPIOD
#define PHY_RESET_PIN         GPIO_PIN_3
#define PHY_RESET_RCU_CLOCK   RCU_GPIOD

/*===========================================================================
 * Debug UART (USART0)
 * Default: PA9 (TX), PA10 (RX) — 115200-8-N-1
 *===========================================================================*/
#define CONSOLE_USART         USART0
#define CONSOLE_USART_RCU     RCU_USART0
#define CONSOLE_TX_PORT       GPIOA
#define CONSOLE_TX_PIN        GPIO_PIN_9
#define CONSOLE_RX_PORT       GPIOA
#define CONSOLE_RX_PIN        GPIO_PIN_10
#define CONSOLE_GPIO_AF       GPIO_AF_7
#define CONSOLE_BAUDRATE      115200

/*===========================================================================
 * Ethernet RMII Pin Mapping (9 pins)
 * These are fixed pin assignments for GD32F450 RMII mode
 *   - PA1:  ETH_RMII_REF_CLK
 *   - PA2:  ETH_MDIO
 *   - PA7:  ETH_RMII_CRS_DV
 *   - PB11: ETH_RMII_TX_EN
 *   - PB12: ETH_RMII_TXD0
 *   - PB13: ETH_RMII_TXD1
 *   - PC1:  ETH_MDC
 *   - PC4:  ETH_RMII_RXD0
 *   - PC5:  ETH_RMII_RXD1
 *===========================================================================*/
#define ETH_RMII_REF_CLK_PORT     GPIOA
#define ETH_RMII_REF_CLK_PIN      GPIO_PIN_1
#define ETH_MDIO_PORT             GPIOA
#define ETH_MDIO_PIN              GPIO_PIN_2
#define ETH_RMII_CRS_DV_PORT      GPIOA
#define ETH_RMII_CRS_DV_PIN       GPIO_PIN_7
#define ETH_RMII_TX_EN_PORT       GPIOB
#define ETH_RMII_TX_EN_PIN        GPIO_PIN_11
#define ETH_RMII_TXD0_PORT        GPIOB
#define ETH_RMII_TXD0_PIN         GPIO_PIN_12
#define ETH_RMII_TXD1_PORT        GPIOB
#define ETH_RMII_TXD1_PIN         GPIO_PIN_13
#define ETH_MDC_PORT              GPIOC
#define ETH_MDC_PIN               GPIO_PIN_1
#define ETH_RMII_RXD0_PORT        GPIOC
#define ETH_RMII_RXD0_PIN         GPIO_PIN_4
#define ETH_RMII_RXD1_PORT        GPIOC
#define ETH_RMII_RXD1_PIN         GPIO_PIN_5
#define ETH_GPIO_AF               GPIO_AF_11

/*===========================================================================
 * Ethernet MII Pin Mapping (17 pins) — additional pins beyond RMII
 *===========================================================================*/
#define ETH_MII_TX_CLK_PORT       GPIOC
#define ETH_MII_TX_CLK_PIN        GPIO_PIN_3
#define ETH_MII_TXD2_PORT         GPIOC
#define ETH_MII_TXD2_PIN          GPIO_PIN_2
#define ETH_MII_TXD3_PORT         GPIOB
#define ETH_MII_TXD3_PIN          GPIO_PIN_8
#define ETH_MII_RX_CLK_PORT       GPIOA
#define ETH_MII_RX_CLK_PIN        GPIO_PIN_1
#define ETH_MII_RX_DV_PORT        GPIOA
#define ETH_MII_RX_DV_PIN         GPIO_PIN_7
#define ETH_MII_RXD2_PORT         GPIOB
#define ETH_MII_RXD2_PIN          GPIO_PIN_0
#define ETH_MII_RXD3_PORT         GPIOB
#define ETH_MII_RXD3_PIN          GPIO_PIN_1
#define ETH_MII_RX_ER_PORT        GPIOB
#define ETH_MII_RX_ER_PIN         GPIO_PIN_10
#define ETH_MII_COL_PORT          GPIOA
#define ETH_MII_COL_PIN           GPIO_PIN_3
#define ETH_MII_CRS_PORT          GPIOA
#define ETH_MII_CRS_PIN           GPIO_PIN_0

/*===========================================================================
 * Function Declarations
 *===========================================================================*/
void board_clock_init(void);
void board_periph_init(void);
void board_delay_ms(uint32_t ms);
void board_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
