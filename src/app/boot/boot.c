/**
 * @file    boot.c
 * @brief   Boot mode detection and validation logic
 */

#include "boot.h"
#include "app_config.h"
#include "board.h"
#include "crc32.h"
#include "flash_if.h"
#include "log.h"

/* Boot magic value stored in RTC backup register */
#define BOOT_FLAG_MAGIC 0x4F544142  /* "OTAB" */

/* RTC backup register address for boot flag
 * BKP_DATA_0 through BKP_DATA_31 are at offsets 0x50-0xCC in the BKP registers
 * Using BKP_DATA_0 as the boot flag register
 */
#define BKP_BASE     0x40002800UL
#define BKP_DATA_0   (*(volatile uint32_t *)(BKP_BASE + 0x50UL))

/* Size of app code area header to verify */
#define APP_HEADER_CHECK_SIZE 256

/*===========================================================================
 * RTC Backup Register Access
 * Enables PWR + BKP clocks for backup domain access
 *===========================================================================*/
static void bkp_init(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();
    rcu_periph_clock_enable(RCU_BKPSRAM);
}

static uint32_t bkp_read(uint32_t offset)
{
    return *(volatile uint32_t *)(BKP_BASE + offset);
}

static void bkp_write(uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(BKP_BASE + offset) = value;
}

/*===========================================================================
 * Boot Mode Detection
 * Returns the appropriate boot mode based on priority logic
 *===========================================================================*/
boot_mode_t boot_get_mode(void)
{
    bkp_init();

    /* Priority 1: Check backup register for forced bootloader flag */
    if (bkp_read(0x50) == BOOT_FLAG_MAGIC)
    {
        LOG_I("boot: Forced bootloader mode (BKP flag)");
        bkp_write(0x50, 0);  /* Clear the flag */
        return BOOT_MODE_BOOTLOADER;
    }

    /* Priority 2: Check GPIO boot button */
    rcu_periph_clock_enable(BOOT_BUTTON_RCU_CLOCK);
    gpio_mode_set(BOOT_BUTTON_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP,
                  BOOT_BUTTON_PIN);

    if (BOOT_BUTTON_PRESSED())
    {
        LOG_I("boot: Bootloader mode (button pressed)");
        return BOOT_MODE_BOOTLOADER;
    }

    /* Priority 3: Check application validity */
    if (!boot_is_app_valid())
    {
        LOG_W("boot: App invalid — entering bootloader for recovery");
        return BOOT_MODE_BOOTLOADER;
    }

    /* Default: Application mode */
    LOG_I("boot: Application mode");
    return BOOT_MODE_APP;
}

/*===========================================================================
 * OTA Flag Management
 *===========================================================================*/
void boot_set_ota_flag(void)
{
    bkp_init();
    bkp_write(0x50, BOOT_FLAG_MAGIC);
}

void boot_clear_ota_flag(void)
{
    bkp_init();
    bkp_write(0x50, 0);
}

/*===========================================================================
 * Application Validity Check
 * Reads the first bytes of the app area and checks for non-0xFF content
 * as a simplified validity check. Full CRC verification can be added.
 *===========================================================================*/
/*===========================================================================
 * Pure header validation (testable without flash access)
 * Checks stack pointer range and reset vector validity.
 *===========================================================================*/
bool boot_validate_header(const uint8_t *buf, uint32_t app_base)
{
    /* Check if the app area has been programmed (not all 0xFF) */
    bool has_content = false;
    for (uint32_t i = 0; i < APP_HEADER_CHECK_SIZE; i++)
    {
        if (buf[i] != 0xFF)
        {
            has_content = true;
            break;
        }
    }

    if (!has_content)
    {
        return false;
    }

    /* Check for valid stack pointer (within SRAM range: 0x20000000-0x20020000) */
    uint32_t sp = *(const uint32_t *)&buf[0];
    if (sp < 0x20000000UL || sp > 0x20020000UL)
    {
        return false;
    }

    /* Check for valid reset vector (within flash range, Thumb bit set) */
    uint32_t pc = *(const uint32_t *)&buf[4];
    if ((pc & 1) == 0 || pc < app_base || pc > (app_base + FLASH_APP_SIZE))
    {
        return false;
    }

    return true;
}

bool boot_is_app_valid(void)
{
    uint8_t buf[APP_HEADER_CHECK_SIZE];
    uint32_t app_base = FLASH_APP_BASE;

    flash_read_buf(app_base, buf, sizeof(buf));
    return boot_validate_header(buf, app_base);
}
