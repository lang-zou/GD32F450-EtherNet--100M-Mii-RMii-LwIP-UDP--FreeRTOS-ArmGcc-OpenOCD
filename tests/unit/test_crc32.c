/**
 * @file    test_crc32.c
 * @brief   Unit tests for CRC32 module
 */

#include "unity.h"
#include "crc32.h"
#include <string.h>

/*===========================================================================
 * Setup / Teardown
 *===========================================================================*/
void setUp(void)   { }
void tearDown(void) { }

/*===========================================================================
 * Test Cases
 *===========================================================================*/

/**
 * @brief CRC32 of empty data should be 0x00000000 (after final XOR)
 */
void test_crc32_empty(void)
{
    uint32_t crc = crc32_calc(NULL, 0);
    TEST_ASSERT_EQUAL_UINT32(0x00000000, crc);
}

/**
 * @brief CRC32 of known test vector: "123456789"
 * Expected: 0xCBF43926 (IEEE 802.3 standard check value)
 */
void test_crc32_known_vector(void)
{
    const uint8_t data[] = "123456789";
    uint32_t crc = crc32_calc(data, 9);
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926, crc);
}

/**
 * @brief CRC32 of "A" single character
 * Expected: 0xD3D99E8B
 */
void test_crc32_single_char(void)
{
    const uint8_t data[] = "A";
    uint32_t crc = crc32_calc(data, 1);
    TEST_ASSERT_EQUAL_UINT32(0xD3D99E8B, crc);
}

/**
 * @brief CRC32 of zero-filled buffer
 */
void test_crc32_zeros(void)
{
    uint8_t data[16];
    memset(data, 0, sizeof(data));
    uint32_t crc = crc32_calc(data, 16);

    /* Running CRC32 of 16 zero bytes should be consistent */
    uint32_t crc2 = crc32_update(0xFFFFFFFF, data, 16) ^ 0xFFFFFFFF;
    TEST_ASSERT_EQUAL_UINT32(crc, crc2);
}

/**
 * @brief crc32_update() chaining produces same result as single calc
 */
void test_crc32_update_chaining(void)
{
    const uint8_t part1[] = "Hello, ";
    const uint8_t part2[] = "World!";
    const uint8_t full[]   = "Hello, World!";

    /* Single-pass CRC */
    uint32_t crc_full = crc32_calc(full, strlen((const char *)full));

    /* Multi-pass CRC */
    uint32_t crc = crc32_update(0xFFFFFFFF, part1, strlen((const char *)part1));
    crc = crc32_update(crc, part2, strlen((const char *)part2));
    crc ^= 0xFFFFFFFF;

    TEST_ASSERT_EQUAL_UINT32(crc_full, crc);
}

/**
 * @brief CRC32 is deterministic
 */
void test_crc32_deterministic(void)
{
    const uint8_t data[] = "Test data for CRC check";
    uint32_t crc1 = crc32_calc(data, strlen((const char *)data));
    uint32_t crc2 = crc32_calc(data, strlen((const char *)data));

    TEST_ASSERT_EQUAL_UINT32(crc1, crc2);
}

/**
 * @brief CRC32 of different data produces different results
 */
void test_crc32_different_data(void)
{
    const uint8_t data1[] = "ABCDEFGH";
    const uint8_t data2[] = "ABCDEFGI";

    uint32_t crc1 = crc32_calc(data1, 8);
    uint32_t crc2 = crc32_calc(data2, 8);

    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}

/**
 * @brief CRC32 of all 0xFF (common in flash checks)
 */
void test_crc32_all_ff(void)
{
    uint8_t data[64];
    memset(data, 0xFF, sizeof(data));
    uint32_t crc = crc32_calc(data, sizeof(data));

    /* Just verify it doesn't crash and produces a value */
    (void)crc;
}

/*===========================================================================
 * Test Runner
 *===========================================================================*/
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_crc32_empty);
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_crc32_single_char);
    RUN_TEST(test_crc32_zeros);
    RUN_TEST(test_crc32_update_chaining);
    RUN_TEST(test_crc32_deterministic);
    RUN_TEST(test_crc32_different_data);
    RUN_TEST(test_crc32_all_ff);

    return UNITY_END();
}
