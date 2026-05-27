/**
 * @file    boot.h
 * @brief   Boot mode detection and bootloader entry logic
 *
 * Single main.c entry — bootloader and app share the same firmware image.
 * boot_get_mode() determines which code path main() should take.
 *
 * Boot mode priority:
 *   1. RTC BKP register magic → forced bootloader (set by OTA)
 *   2. GPIO boot pin low → hardware-triggered bootloader
 *   3. App area CRC invalid → recovery bootloader
 *   4. Default → normal app mode
 */

#ifndef BOOT_H_
#define BOOT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Types
 *===========================================================================*/
typedef enum
{
    BOOT_MODE_APP        = 0,
    BOOT_MODE_BOOTLOADER = 1,
} boot_mode_t;

/*===========================================================================
 * API
 *===========================================================================*/

/**
 * @brief Determine the boot mode at startup
 * Checks backup registers, GPIO pins, and firmware integrity.
 * @return BOOT_MODE_APP or BOOT_MODE_BOOTLOADER
 */
boot_mode_t boot_get_mode(void);

/**
 * @brief Set the OTA boot flag in backup register
 * Called by OTA module before system reset.
 */
void boot_set_ota_flag(void);

/**
 * @brief Clear the OTA boot flag
 */
void boot_clear_ota_flag(void);

/**
 * @brief Validate Cortex-M4 vector table header without flash access
 * Checks: non-empty (not all 0xFF), valid SP in SRAM, valid PC with Thumb bit
 * @param buf      256-byte buffer from app flash base
 * @param app_base Base address of app area
 * @return true if header appears valid
 */
bool boot_validate_header(const uint8_t *buf, uint32_t app_base);

/**
 * @brief Check if the application code area has a valid firmware image
 * Reads flash via flash_read_buf() and calls boot_validate_header().
 * @return true if app area contains valid firmware
 */
bool boot_is_app_valid(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_H_ */
