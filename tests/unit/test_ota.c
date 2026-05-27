/**
 * @file    test_ota.c
 * @brief   Unit tests for OTA protocol packet format validation
 *
 * Tests the byte-level packet encoding/decoding for REQ, ACK, DATA, DONE packets.
 * These tests run on the host PC without network hardware.
 */

#include "unity.h"
#include "ota.h"
#include <string.h>
#include <stdint.h>

/*===========================================================================
 * Test Data
 *===========================================================================*/
static uint8_t pkt_buf[OTA_MAX_PACKET];

void setUp(void)
{
    memset(pkt_buf, 0, sizeof(pkt_buf));
}

void tearDown(void) { }

/*===========================================================================
 * Helper: Build REQ packet in buffer
 *===========================================================================*/
static void build_req(uint8_t *buf, uint8_t cmd, uint32_t seq, uint32_t size)
{
    buf[0] = (uint8_t)(OTA_MAGIC_REQ >> 24);
    buf[1] = (uint8_t)(OTA_MAGIC_REQ >> 16);
    buf[2] = (uint8_t)(OTA_MAGIC_REQ >> 8);
    buf[3] = (uint8_t)(OTA_MAGIC_REQ);
    buf[4] = cmd;
    buf[5] = (uint8_t)(seq >> 24);
    buf[6] = (uint8_t)(seq >> 16);
    buf[7] = (uint8_t)(seq >> 8);
    buf[8] = (uint8_t)(seq);
    buf[9]  = (uint8_t)(size >> 24);
    buf[10] = (uint8_t)(size >> 16);
    buf[11] = (uint8_t)(size >> 8);
    buf[12] = (uint8_t)(size);
}

/*===========================================================================
 * Helper: Build ACK packet in buffer
 *===========================================================================*/
static void build_ack(uint8_t *buf, uint32_t seq, uint8_t status)
{
    buf[0] = (uint8_t)(OTA_MAGIC_ACK >> 24);
    buf[1] = (uint8_t)(OTA_MAGIC_ACK >> 16);
    buf[2] = (uint8_t)(OTA_MAGIC_ACK >> 8);
    buf[3] = (uint8_t)(OTA_MAGIC_ACK);
    buf[4] = (uint8_t)(seq >> 24);
    buf[5] = (uint8_t)(seq >> 16);
    buf[6] = (uint8_t)(seq >> 8);
    buf[7] = (uint8_t)(seq);
    buf[8] = status;
}

/*===========================================================================
 * Helper: Build DATA packet in buffer
 *===========================================================================*/
static void build_data(uint8_t *buf, uint32_t seq, const uint8_t *payload,
                       uint16_t payload_size)
{
    buf[0] = (uint8_t)(OTA_MAGIC_DATA >> 24);
    buf[1] = (uint8_t)(OTA_MAGIC_DATA >> 16);
    buf[2] = (uint8_t)(OTA_MAGIC_DATA >> 8);
    buf[3] = (uint8_t)(OTA_MAGIC_DATA);
    buf[4] = (uint8_t)(seq >> 24);
    buf[5] = (uint8_t)(seq >> 16);
    buf[6] = (uint8_t)(seq >> 8);
    buf[7] = (uint8_t)(seq);
    buf[8] = (uint8_t)(payload_size >> 8);
    buf[9] = (uint8_t)(payload_size);
    memcpy(&buf[OTA_DATA_HDR], payload, payload_size);
}

/*===========================================================================
 * Helper: Build DONE packet in buffer
 *===========================================================================*/
static void build_done(uint8_t *buf, uint32_t total_size, uint32_t crc)
{
    buf[0] = (uint8_t)(OTA_MAGIC_DONE >> 24);
    buf[1] = (uint8_t)(OTA_MAGIC_DONE >> 16);
    buf[2] = (uint8_t)(OTA_MAGIC_DONE >> 8);
    buf[3] = (uint8_t)(OTA_MAGIC_DONE);
    buf[4] = (uint8_t)(total_size >> 24);
    buf[5] = (uint8_t)(total_size >> 16);
    buf[6] = (uint8_t)(total_size >> 8);
    buf[7] = (uint8_t)(total_size);
    buf[8]  = (uint8_t)(crc >> 24);
    buf[9]  = (uint8_t)(crc >> 16);
    buf[10] = (uint8_t)(crc >> 8);
    buf[11] = (uint8_t)(crc);
}

/*===========================================================================
 * Test: Packet size constants are correct
 *===========================================================================*/
void test_ota_packet_sizes(void)
{
    TEST_ASSERT_EQUAL_UINT32(13, OTA_REQ_SIZE);
    TEST_ASSERT_EQUAL_UINT32(9,  OTA_ACK_SIZE);
    TEST_ASSERT_EQUAL_UINT32(10, OTA_DATA_HDR);
    TEST_ASSERT_EQUAL_UINT32(12, OTA_DONE_SIZE);
}

/*===========================================================================
 * Test: Magic numbers are correct ASCII values
 *===========================================================================*/
void test_ota_magic_numbers(void)
{
    /* Verify magic numbers match their ASCII representation */
    TEST_ASSERT_EQUAL_HEX32(0x4F544151, OTA_MAGIC_REQ);   /* "OTAQ" */
    TEST_ASSERT_EQUAL_HEX32(0x4F544141, OTA_MAGIC_ACK);   /* "OTAA" */
    TEST_ASSERT_EQUAL_HEX32(0x4F544144, OTA_MAGIC_DATA);  /* "OTAD" */
    TEST_ASSERT_EQUAL_HEX32(0x4F54414F, OTA_MAGIC_DONE);  /* "OTAO" */
}

/*===========================================================================
 * Test: REQ packet magic field is at correct offset
 *===========================================================================*/
void test_ota_req_magic_offset(void)
{
    build_req(pkt_buf, 0, 0, 1024);

    /* Verify magic bytes at offset 0-3 */
    TEST_ASSERT_EQUAL_UINT8(0x4F, pkt_buf[0]);  /* 'O' */
    TEST_ASSERT_EQUAL_UINT8(0x54, pkt_buf[1]);  /* 'T' */
    TEST_ASSERT_EQUAL_UINT8(0x41, pkt_buf[2]);  /* 'A' */
    TEST_ASSERT_EQUAL_UINT8(0x51, pkt_buf[3]);  /* 'Q' */
}

/*===========================================================================
 * Test: REQ packet size field is at correct offset (big-endian)
 *===========================================================================*/
void test_ota_req_size_field(void)
{
    build_req(pkt_buf, 0, 1, 0x00065432UL);

    /* Verify size field big-endian encoding */
    TEST_ASSERT_EQUAL_UINT8(0x00, pkt_buf[9]);
    TEST_ASSERT_EQUAL_UINT8(0x06, pkt_buf[10]);
    TEST_ASSERT_EQUAL_UINT8(0x54, pkt_buf[11]);
    TEST_ASSERT_EQUAL_UINT8(0x32, pkt_buf[12]);
}

/*===========================================================================
 * Test: ACK packet status byte at correct offset
 *===========================================================================*/
void test_ota_ack_status_offset(void)
{
    build_ack(pkt_buf, 42, OTA_ACK_OK);

    TEST_ASSERT_EQUAL_UINT8(OTA_ACK_OK, pkt_buf[8]);

    build_ack(pkt_buf, 42, OTA_ACK_ERR_CRC);
    TEST_ASSERT_EQUAL_UINT8(OTA_ACK_ERR_CRC, pkt_buf[8]);
}

/*===========================================================================
 * Test: ACK status codes are distinct
 *===========================================================================*/
void test_ota_ack_status_codes(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, OTA_ACK_OK);
    TEST_ASSERT_EQUAL_UINT8(1, OTA_ACK_ERR_SIZE);
    TEST_ASSERT_EQUAL_UINT8(2, OTA_ACK_ERR_SEQ);
    TEST_ASSERT_EQUAL_UINT8(3, OTA_ACK_ERR_OVERFLOW);
    TEST_ASSERT_EQUAL_UINT8(4, OTA_ACK_ERR_FLASH);
    TEST_ASSERT_EQUAL_UINT8(5, OTA_ACK_ERR_COPY);
    TEST_ASSERT_EQUAL_UINT8(6, OTA_ACK_ERR_CRC);
}

/*===========================================================================
 * Test: DATA packet payload size field (big-endian 16-bit)
 *===========================================================================*/
void test_ota_data_payload_size(void)
{
    uint8_t payload[64];
    memset(payload, 0xAB, sizeof(payload));

    build_data(pkt_buf, 3, payload, 64);

    TEST_ASSERT_EQUAL_UINT8(0x00, pkt_buf[8]);   /* High byte */
    TEST_ASSERT_EQUAL_UINT8(0x40, pkt_buf[9]);   /* Low byte (64 = 0x0040) */

    /* Verify payload copied after header */
    TEST_ASSERT_EQUAL_UINT8(0xAB, pkt_buf[OTA_DATA_HDR]);
    TEST_ASSERT_EQUAL_UINT8(0xAB, pkt_buf[OTA_DATA_HDR + 63]);
}

/*===========================================================================
 * Test: DONE packet CRC field order
 *===========================================================================*/
void test_ota_done_crc_field(void)
{
    build_done(pkt_buf, 65536, 0xDEADBEEFUL);

    TEST_ASSERT_EQUAL_UINT8(0xDE, pkt_buf[8]);
    TEST_ASSERT_EQUAL_UINT8(0xAD, pkt_buf[9]);
    TEST_ASSERT_EQUAL_UINT8(0xBE, pkt_buf[10]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, pkt_buf[11]);
}

/*===========================================================================
 * Test: MAX_PAYLOAD is within expected range
 *===========================================================================*/
void test_ota_max_payload(void)
{
    TEST_ASSERT_EQUAL_UINT16(1024, OTA_MAX_PAYLOAD);
    TEST_ASSERT_EQUAL_UINT16(1034, OTA_MAX_PACKET);  /* 10 + 1024 */
}

/*===========================================================================
 * Test: Full REQ → ACK → DATA → DONE round-trip values
 *===========================================================================*/
void test_ota_full_roundtrip_constants(void)
{
    /* Verify all packet types use consistent sizes */
    uint8_t req[OTA_REQ_SIZE];
    uint8_t ack[OTA_ACK_SIZE];
    uint8_t data_payload[OTA_MAX_PAYLOAD];
    uint8_t done[OTA_DONE_SIZE];

    uint32_t fw_size = 128 * 1024;  /* 128 KB firmware */
    uint32_t seq = 5;
    uint32_t crc = 0x12345678UL;

    /* Build and verify */
    memset(data_payload, 0xCC, sizeof(data_payload));

    build_req(req, 0, 0, fw_size);
    build_ack(ack, 0, OTA_ACK_OK);
    build_data(data_payload, seq, data_payload, 512);
    build_done(done, fw_size, crc);

    /* REQ: verify cmd byte */
    TEST_ASSERT_EQUAL_UINT8(0, req[4]);

    /* ACK: verify seq */
    TEST_ASSERT_EQUAL_UINT32(0,
        ((uint32_t)ack[4] << 24) | ((uint32_t)ack[5] << 16) |
        ((uint32_t)ack[6] << 8)  | ((uint32_t)ack[7]));

    /* DATA: verify seq */
    TEST_ASSERT_EQUAL_UINT32(seq,
        ((uint32_t)data_payload[4] << 24) | ((uint32_t)data_payload[5] << 16) |
        ((uint32_t)data_payload[6] << 8)  | ((uint32_t)data_payload[7]));

    /* DONE: verify total_size */
    TEST_ASSERT_EQUAL_UINT32(fw_size,
        ((uint32_t)done[4] << 24) | ((uint32_t)done[5] << 16) |
        ((uint32_t)done[6] << 8)  | ((uint32_t)done[7]));
}

/*===========================================================================
 * Test Runner
 *===========================================================================*/
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ota_packet_sizes);
    RUN_TEST(test_ota_magic_numbers);
    RUN_TEST(test_ota_req_magic_offset);
    RUN_TEST(test_ota_req_size_field);
    RUN_TEST(test_ota_ack_status_offset);
    RUN_TEST(test_ota_ack_status_codes);
    RUN_TEST(test_ota_data_payload_size);
    RUN_TEST(test_ota_done_crc_field);
    RUN_TEST(test_ota_max_payload);
    RUN_TEST(test_ota_full_roundtrip_constants);

    return UNITY_END();
}
