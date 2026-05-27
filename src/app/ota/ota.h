/**
 * @file    ota.h
 * @brief   Over-the-Air firmware update via UDP
 *
 * Simple TLV protocol:
 *   REQ:  magic(4B) "OTAQ" | cmd(1B) | seq(4B) | total_size(4B)
 *   ACK:  magic(4B) "OTAA" | seq(4B) | status(1B)
 *   DATA: magic(4B) "OTAD" | seq(4B) | payload_size(2B) | payload(N)
 *   DONE: magic(4B) "OTAO" | total_size(4B) | crc32(4B)
 *
 * Status codes:
 *   0 = OK
 *   1 = Invalid size
 *   2 = Wrong sequence number
 *   3 = Overflow
 *   4 = Flash write error
 *   5 = Copy-to-app error
 *   6 = CRC mismatch
 *
 * Flow:
 *   1. Device enters bootloader → binds UDP port
 *   2. PC tool sends REQ with total firmware size
 *   3. Device ACKs → enters data transfer phase
 *   4. PC sends DATA chunks sequentially
 *   5. Device ACKs each chunk, writes to OTA buffer
 *   6. PC sends DONE with CRC32
 *   7. Device verifies CRC32 → copies to app area → resets
 */

#ifndef OTA_H_
#define OTA_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Protocol Constants — shared with Python OTA tool
 *===========================================================================*/
#define OTA_MAGIC_REQ   0x4F544151U  /* "OTAQ" */
#define OTA_MAGIC_ACK   0x4F544141U  /* "OTAA" */
#define OTA_MAGIC_DATA  0x4F544144U  /* "OTAD" */
#define OTA_MAGIC_DONE  0x4F54414FU  /* "OTAO" */

#define OTA_REQ_SIZE    13   /* magic(4) + cmd(1) + seq(4) + total_size(4) */
#define OTA_ACK_SIZE    9    /* magic(4) + seq(4) + status(1) */
#define OTA_DATA_HDR    10   /* magic(4) + seq(4) + payload_size(2) */
#define OTA_DONE_SIZE   12   /* magic(4) + total_size(4) + crc32(4) */
#define OTA_MAX_PAYLOAD 1024
#define OTA_MAX_PACKET  (OTA_DATA_HDR + OTA_MAX_PAYLOAD)

/* ACK status codes */
#define OTA_ACK_OK             0
#define OTA_ACK_ERR_SIZE       1
#define OTA_ACK_ERR_SEQ        2
#define OTA_ACK_ERR_OVERFLOW   3
#define OTA_ACK_ERR_FLASH      4
#define OTA_ACK_ERR_COPY       5
#define OTA_ACK_ERR_CRC        6

/**
 * @brief Initialize minimal LwIP netif for OTA bootloader mode
 * Must be called after eth_mac_init() and before ota_bootloader_entry().
 * Sets up a static IP netif on the Ethernet interface.
 * @return true on success
 */
bool ota_network_init(void);

/**
 * @brief Enter bootloader mode and wait for OTA firmware update
 * Blocks for OTA_TIMEOUT_MS. Returns if timeout expires without valid update.
 * Requires eth_mac_init() and ota_network_init() to be called first.
 */
void ota_bootloader_entry(void);

/**
 * @brief Enter OTA mode from CLI command
 * Sets RTC backup flag and triggers NVIC_SystemReset().
 */
void ota_enter_from_cli(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_H_ */
