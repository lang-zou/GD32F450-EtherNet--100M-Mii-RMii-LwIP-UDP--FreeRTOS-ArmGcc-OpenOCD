/**
 * @file    app_config.h
 * @brief   Application-level feature switches and compile-time configuration
 *
 * Central configuration file for the entire project. All feature toggles,
 * hardware selections, and tunable parameters are defined here.
 */

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Ethernet PHY Selection
 *===========================================================================*/
#define ETH_PHY_RMII  1
#define ETH_PHY_MII   2

#ifndef ETH_PHY_MODE
#define ETH_PHY_MODE  ETH_PHY_RMII
#endif

/*===========================================================================
 * Feature Switches
 *===========================================================================*/
#define ENABLE_SHELL         1   /* UART command-line shell */
#define ENABLE_OTA           1   /* Over-the-air firmware update */
#define ENABLE_WATCHDOG      0   /* Independent watchdog (enable after HW test) */
#define ENABLE_UDP_DEMO      1   /* UDP echo demo task */
#define ENABLE_LED_DEMO      1   /* LED blink demo task */

/*===========================================================================
 * Timing Parameters (ms)
 *===========================================================================*/
#define OTA_TIMEOUT_MS        5000   /* OTA receive window timeout */
#define OTA_CHUNK_TIMEOUT_MS  2000   /* Max wait between OTA data chunks */
#define LED_BLINK_PERIOD_MS   500    /* LED toggle interval */
#define WATCHDOG_TIMEOUT_MS   2000   /* IWDG reset timeout */

/*===========================================================================
 * Logging Configuration
 *===========================================================================*/
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

#ifndef LOG_LEVEL
#define LOG_LEVEL       LOG_LEVEL_INFO
#endif

/*===========================================================================
 * Network Configuration (defaults, can be changed via CLI)
 *===========================================================================*/
#define NET_DEFAULT_IP_ADDR0   192
#define NET_DEFAULT_IP_ADDR1   168
#define NET_DEFAULT_IP_ADDR2   1
#define NET_DEFAULT_IP_ADDR3   100

#define NET_DEFAULT_MASK0      255
#define NET_DEFAULT_MASK1      255
#define NET_DEFAULT_MASK2      255
#define NET_DEFAULT_MASK3      0

#define NET_DEFAULT_GW0        192
#define NET_DEFAULT_GW1        168
#define NET_DEFAULT_GW2        1
#define NET_DEFAULT_GW3        1

#define NET_UDP_ECHO_PORT      5000
#define NET_OTA_PORT           5001

/*===========================================================================
 * Flash Partition (see linker script for authoritative addresses)
 *===========================================================================*/
#define FLASH_BOOT_BASE       0x08000000UL
#define FLASH_APP_BASE        0x08020000UL
#define FLASH_OTA_BUFFER_BASE 0x08080000UL
#define FLASH_CONFIG_BASE     0x080F0000UL

#define FLASH_BOOT_SIZE       0x20000UL   /* 128 KB */
#define FLASH_APP_SIZE        0x60000UL   /* 384 KB */
#define FLASH_OTA_BUFFER_SIZE 0x70000UL   /* 448 KB */
#define FLASH_CONFIG_SIZE     0x10000UL   /* 64 KB  */

/*===========================================================================
 * Bootloader Configuration
 *===========================================================================*/
#define BOOT_FLAG_MAGIC        0x4F544142  /* "OTAB" */
#define BOOT_BKP_REG           BKP_DATA_0  /* RTC backup register index */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
