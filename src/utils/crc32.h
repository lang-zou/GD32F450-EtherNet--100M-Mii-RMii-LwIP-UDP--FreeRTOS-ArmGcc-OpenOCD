/**
 * @file    crc32.h
 * @brief   CRC32 calculation (IEEE 802.3 polynomial) for firmware verification
 *
 * Polynomial: 0xEDB88320 (reflected), same as Ethernet CRC32
 */

#ifndef CRC32_H_
#define CRC32_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculate CRC32 over a data buffer
 * @param data  Pointer to data buffer
 * @param len   Number of bytes to process
 * @return      CRC32 checksum (initial value = 0xFFFFFFFF)
 */
uint32_t crc32_calc(const uint8_t *data, size_t len);

/**
 * @brief Continue a CRC32 calculation (for chunked processing)
 * @param crc   Previous CRC32 value (0xFFFFFFFF for first chunk)
 * @param data  Pointer to next data chunk
 * @param len   Length of this chunk
 * @return      Updated CRC32 value
 */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CRC32_H_ */
