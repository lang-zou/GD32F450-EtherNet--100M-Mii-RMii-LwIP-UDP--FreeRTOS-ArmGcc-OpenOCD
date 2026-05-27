/**
 * @file    test_boot.c
 * @brief   Unit tests for boot header validation logic
 *
 * Tests the Cortex-M4 vector table validation rules without hardware dependencies.
 * Validates the same logic as boot_validate_header() in boot.c.
 * Run on host PC with `make test`.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Constants from app_config.h (duplicated for standalone host test) */
#define FLASH_APP_BASE  0x08020000UL
#define FLASH_APP_SIZE  0x60000UL
#define HEADER_SIZE     256

/*===========================================================================
 * Pure function: validate Cortex-M4 vector table header
 * Same logic as boot_validate_header() in src/app/boot/boot.c
 *===========================================================================*/
static bool validate_header(const uint8_t *buf, uint32_t app_base)
{
    /* Check if the app area has been programmed (not all 0xFF) */
    bool has_content = false;
    for (uint32_t i = 0; i < HEADER_SIZE; i++) {
        if (buf[i] != 0xFF) {
            has_content = true;
            break;
        }
    }
    if (!has_content) return false;

    /* Check for valid stack pointer (within SRAM range: 0x20000000-0x20020000) */
    uint32_t sp;
    memcpy(&sp, &buf[0], sizeof(sp));
    if (sp < 0x20000000UL || sp > 0x20020000UL) return false;

    /* Check for valid reset vector (Thumb bit set, within flash range) */
    uint32_t pc;
    memcpy(&pc, &buf[4], sizeof(pc));
    if ((pc & 1) == 0 || pc < app_base || pc > (app_base + FLASH_APP_SIZE))
        return false;

    return true;
}

/*===========================================================================
 * Test Data
 *===========================================================================*/
static uint8_t test_buf[HEADER_SIZE];

void setUp(void)
{
    memset(test_buf, 0xFF, sizeof(test_buf));
}

void tearDown(void) { }

/*===========================================================================
 * Test Cases
 *===========================================================================*/

void test_boot_validate_empty(void)
{
    TEST_ASSERT_FALSE(validate_header(test_buf, FLASH_APP_BASE));
}

void test_boot_validate_valid(void)
{
    uint32_t sp = 0x20010000UL;
    uint32_t pc = 0x08020001UL;  /* Thumb bit set */

    memcpy(&test_buf[0], &sp, 4);
    memcpy(&test_buf[4], &pc, 4);

    TEST_ASSERT_TRUE(validate_header(test_buf, FLASH_APP_BASE));
}

void test_boot_validate_sp_low(void)
{
    uint32_t sp = 0x1FFFFFFFUL;
    uint32_t pc = 0x08020001UL;

    memcpy(&test_buf[0], &sp, 4);
    memcpy(&test_buf[4], &pc, 4);

    TEST_ASSERT_FALSE(validate_header(test_buf, FLASH_APP_BASE));
}

void test_boot_validate_sp_high(void)
{
    uint32_t sp = 0x20030000UL;
    uint32_t pc = 0x08020001UL;

    memcpy(&test_buf[0], &sp, 4);
    memcpy(&test_buf[4], &pc, 4);

    TEST_ASSERT_FALSE(validate_header(test_buf, FLASH_APP_BASE));
}

void test_boot_validate_sp_boundary(void)
{
    uint32_t sp = 0x20020000UL;
    uint32_t pc = 0x08020001UL;

    memcpy(&test_buf[0], &sp, 4);
    memcpy(&test_buf[4], &pc, 4);

    TEST_ASSERT_TRUE(validate_header(test_buf, FLASH_APP_BASE));
}

void test_boot_validate_pc_no_thumb(void)
{
    uint32_t sp = 0x20010000UL;
    uint32_t pc = 0x08020000UL;  /* Thumb bit not set */

    memcpy(&test_buf[0], &sp, 4);
    memcpy(&test_buf[4], &pc, 4);

    TEST_ASSERT_FALSE(validate_header(test_buf, FLASH_APP_BASE));
}

void test_boot_validate_pc_out_of_range_low(void)
{
    uint32_t sp = 0x20010000UL;
    uint32_t pc = 0x08000001UL;  /* In boot area */

    memcpy(&test_buf[0], &sp, 4);
    memcpy(&test_buf[4], &pc, 4);

    TEST_ASSERT_FALSE(validate_header(test_buf, FLASH_APP_BASE));
}

void test_boot_validate_pc_out_of_range_high(void)
{
    uint32_t sp = 0x20010000UL;
    uint32_t pc = 0x08080001UL;  /* Beyond app area */

    memcpy(&test_buf[0], &sp, 4);
    memcpy(&test_buf[4], &pc, 4);

    TEST_ASSERT_FALSE(validate_header(test_buf, FLASH_APP_BASE));
}

void test_boot_validate_partial_ff(void)
{
    /* First 4 bytes programmed, rest is 0xFF — should pass content check */
    uint32_t sp = 0x20010000UL;
    memcpy(&test_buf[0], &sp, 4);

    /* But PC is at offset 4 (still 0xFF) → invalid vector */
    TEST_ASSERT_FALSE(validate_header(test_buf, FLASH_APP_BASE));
}

/*===========================================================================
 * Test Runner
 *===========================================================================*/
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_boot_validate_empty);
    RUN_TEST(test_boot_validate_valid);
    RUN_TEST(test_boot_validate_sp_low);
    RUN_TEST(test_boot_validate_sp_high);
    RUN_TEST(test_boot_validate_sp_boundary);
    RUN_TEST(test_boot_validate_pc_no_thumb);
    RUN_TEST(test_boot_validate_pc_out_of_range_low);
    RUN_TEST(test_boot_validate_pc_out_of_range_high);
    RUN_TEST(test_boot_validate_partial_ff);

    return UNITY_END();
}
