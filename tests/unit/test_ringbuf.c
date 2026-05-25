/**
 * @file    test_ringbuf.c
 * @brief   Unit tests for ring buffer module
 */

#include "unity.h"
#include "ringbuf.h"
#include <string.h>

/*===========================================================================
 * Test Data
 *===========================================================================*/
#define BUF_SIZE 256
static uint8_t test_buf[BUF_SIZE];
static ringbuf_t rb;

/*===========================================================================
 * Setup / Teardown
 *===========================================================================*/
void setUp(void)
{
    memset(test_buf, 0, sizeof(test_buf));
    ringbuf_init(&rb, test_buf, BUF_SIZE);
}

void tearDown(void) { }

/*===========================================================================
 * Test Cases
 *===========================================================================*/

/**
 * @brief Newly initialized ring buffer should be empty
 */
void test_ringbuf_init_empty(void)
{
    TEST_ASSERT_TRUE(ringbuf_is_empty(&rb));
    TEST_ASSERT_FALSE(ringbuf_is_full(&rb));
    TEST_ASSERT_EQUAL_UINT32(0, ringbuf_available(&rb));
}

/**
 * @brief Put and get single byte
 */
void test_ringbuf_put_get_single(void)
{
    uint8_t byte;

    TEST_ASSERT_TRUE(ringbuf_put(&rb, 0xAB));
    TEST_ASSERT_FALSE(ringbuf_is_empty(&rb));
    TEST_ASSERT_EQUAL_UINT32(1, ringbuf_available(&rb));

    TEST_ASSERT_TRUE(ringbuf_get(&rb, &byte));
    TEST_ASSERT_EQUAL_UINT8(0xAB, byte);
    TEST_ASSERT_TRUE(ringbuf_is_empty(&rb));
}

/**
 * @brief Put until full, then verify full state
 */
void test_ringbuf_full(void)
{
    /* Fill to capacity (BUF_SIZE - 1 effective slots) */
    for (uint32_t i = 0; i < BUF_SIZE - 1; i++)
    {
        TEST_ASSERT_TRUE(ringbuf_put(&rb, (uint8_t)i));
    }

    TEST_ASSERT_TRUE(ringbuf_is_full(&rb));
    TEST_ASSERT_EQUAL_UINT32(BUF_SIZE - 1, ringbuf_available(&rb));

    /* One more put should fail */
    TEST_ASSERT_FALSE(ringbuf_put(&rb, 0xFF));
}

/**
 * @brief Read from empty buffer should fail
 */
void test_ringbuf_get_empty(void)
{
    uint8_t byte;
    TEST_ASSERT_FALSE(ringbuf_get(&rb, &byte));
}

/**
 * @brief Bulk write and read
 */
void test_ringbuf_bulk_write_read(void)
{
    const uint8_t data[] = "Hello, Ring Buffer!";
    uint32_t len = strlen((const char *)data);

    uint32_t written = ringbuf_write(&rb, data, len);
    TEST_ASSERT_EQUAL_UINT32(len, written);
    TEST_ASSERT_EQUAL_UINT32(len, ringbuf_available(&rb));

    uint8_t readback[32];
    uint32_t read = ringbuf_read(&rb, readback, sizeof(readback));
    TEST_ASSERT_EQUAL_UINT32(len, read);
    TEST_ASSERT_EQUAL_MEMORY(data, readback, len);
    TEST_ASSERT_TRUE(ringbuf_is_empty(&rb));
}

/**
 * @brief Wrap-around behavior
 */
void test_ringbuf_wraparound(void)
{
    uint8_t result;

    /* Fill to near-full */
    for (uint32_t i = 0; i < BUF_SIZE / 2; i++)
    {
        ringbuf_put(&rb, 0x00);
    }

    /* Drain half */
    for (uint32_t i = 0; i < BUF_SIZE / 4; i++)
    {
        ringbuf_get(&rb, &result);
    }

    /* Now put more to force wrap-around */
    for (uint32_t i = 0; i < BUF_SIZE / 2; i++)
    {
        TEST_ASSERT_TRUE(ringbuf_put(&rb, (uint8_t)i));
    }

    /* Verify all bytes can be read in order */
    /* First batch: remaining zeros */
    uint32_t remain = ringbuf_available(&rb);
    for (uint32_t i = 0; i < remain; i++)
    {
        TEST_ASSERT_TRUE(ringbuf_get(&rb, &result));
    }
    TEST_ASSERT_TRUE(ringbuf_is_empty(&rb));
}

/**
 * @brief Reset clears all data
 */
void test_ringbuf_reset(void)
{
    for (uint32_t i = 0; i < 100; i++)
    {
        ringbuf_put(&rb, (uint8_t)i);
    }

    TEST_ASSERT_FALSE(ringbuf_is_empty(&rb));

    ringbuf_reset(&rb);

    TEST_ASSERT_TRUE(ringbuf_is_empty(&rb));
    TEST_ASSERT_EQUAL_UINT32(0, ringbuf_available(&rb));
}

/**
 * @brief Read buffer smaller than available data
 */
void test_ringbuf_partial_read(void)
{
    const uint8_t data[] = "1234567890";
    uint32_t len = strlen((const char *)data);

    ringbuf_write(&rb, data, len);

    uint8_t readback[5];
    uint32_t read = ringbuf_read(&rb, readback, 5);
    TEST_ASSERT_EQUAL_UINT32(5, read);
    TEST_ASSERT_EQUAL_MEMORY("12345", readback, 5);

    /* Remaining bytes still available */
    TEST_ASSERT_EQUAL_UINT32(5, ringbuf_available(&rb));

    read = ringbuf_read(&rb, readback, 5);
    TEST_ASSERT_EQUAL_UINT32(5, read);
    TEST_ASSERT_EQUAL_MEMORY("67890", readback, 5);

    TEST_ASSERT_TRUE(ringbuf_is_empty(&rb));
}

/**
 * @brief Free space calculation
 */
void test_ringbuf_free_space(void)
{
    TEST_ASSERT_EQUAL_UINT32(BUF_SIZE - 1, ringbuf_free(&rb));

    ringbuf_put(&rb, 0xAA);
    TEST_ASSERT_EQUAL_UINT32(BUF_SIZE - 2, ringbuf_free(&rb));
}

/*===========================================================================
 * Test Runner
 *===========================================================================*/
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ringbuf_init_empty);
    RUN_TEST(test_ringbuf_put_get_single);
    RUN_TEST(test_ringbuf_full);
    RUN_TEST(test_ringbuf_get_empty);
    RUN_TEST(test_ringbuf_bulk_write_read);
    RUN_TEST(test_ringbuf_wraparound);
    RUN_TEST(test_ringbuf_reset);
    RUN_TEST(test_ringbuf_partial_read);
    RUN_TEST(test_ringbuf_free_space);

    return UNITY_END();
}
