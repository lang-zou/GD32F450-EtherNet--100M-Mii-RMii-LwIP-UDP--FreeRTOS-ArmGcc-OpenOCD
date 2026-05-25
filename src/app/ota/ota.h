/**
 * @file    ota.h
 * @brief   Over-the-Air firmware update via UDP
 *
 * Simple TLV protocol:
 *   REQ:  "OTAQ" | cmd(1B) | seq(4B) | size(4B)
 *   ACK:  "OTAA" | seq(4B) | status(1B)
 *   DATA: "OTAD" | seq(4B) | size(2B) | payload(N)
 *   DONE: "OTAO" | total_size(4B) | crc32(4B)
 *
 * Flow:
 *   1. Device enters bootloader → binds UDP port 5001
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enter bootloader mode and wait for OTA firmware update
 * Blocks for OTA_TIMEOUT_MS. Returns if timeout expires without valid update.
 */
void ota_bootloader_entry(void);

/**
 * @brief Enter OTA mode from CLI command
 */
void ota_enter_from_cli(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_H_ */
