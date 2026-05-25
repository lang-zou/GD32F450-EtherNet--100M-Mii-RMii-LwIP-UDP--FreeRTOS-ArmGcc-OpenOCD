/**
 * @file    flash_if.h
 * @brief   Internal Flash memory interface — read/write/erase for OTA operations
 *
 * GD32F450 Flash characteristics:
 *   - Page size varies: 16KB (sector 0-3), 64KB (sector 4), 128KB (sector 5+)
 *   - Write granularity: 32-bit word
 *   - Must be erased (to 0xFF) before writing
 *   - Programming must be 32-bit aligned
 */

#ifndef FLASH_IF_H_
#define FLASH_IF_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool flash_unlock(void);
bool flash_lock(void);
bool flash_erase_sector(uint32_t addr);
bool flash_erase_range(uint32_t start_addr, uint32_t size);
bool flash_write_word(uint32_t addr, uint32_t data);
bool flash_write_buf(uint32_t addr, const uint8_t *data, uint32_t len);
void flash_read_buf(uint32_t addr, uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_IF_H_ */
