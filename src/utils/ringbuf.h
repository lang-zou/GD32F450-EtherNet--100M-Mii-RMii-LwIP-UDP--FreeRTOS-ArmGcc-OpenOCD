/**
 * @file    ringbuf.h
 * @brief   Lock-free single-producer single-consumer ring buffer
 *
 * Suitable for interrupt-to-task data transfer (e.g., UART RX, ETH frames).
 * Thread-safe as long as only one producer and one consumer operate at a time.
 */

#ifndef RINGBUF_H_
#define RINGBUF_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t  *buffer;
    uint32_t  size;
    volatile uint32_t head;   /* Producer index */
    volatile uint32_t tail;   /* Consumer index */
} ringbuf_t;

/**
 * @brief Initialize a ring buffer
 * @param rb      Pointer to ringbuf_t structure
 * @param buffer  Pre-allocated memory buffer
 * @param size    Size of the buffer in bytes (must be power of 2)
 */
void ringbuf_init(ringbuf_t *rb, uint8_t *buffer, uint32_t size);

/**
 * @brief Write a single byte to the ring buffer
 * @return true if byte was written, false if buffer is full
 */
bool ringbuf_put(ringbuf_t *rb, uint8_t byte);

/**
 * @brief Read a single byte from the ring buffer
 * @param byte  Pointer to store the read byte
 * @return true if byte was read, false if buffer is empty
 */
bool ringbuf_get(ringbuf_t *rb, uint8_t *byte);

/**
 * @brief Write multiple bytes to the ring buffer
 * @return Number of bytes actually written
 */
uint32_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint32_t len);

/**
 * @brief Read multiple bytes from the ring buffer
 * @return Number of bytes actually read
 */
uint32_t ringbuf_read(ringbuf_t *rb, uint8_t *data, uint32_t len);

/**
 * @brief Get number of bytes currently in the ring buffer
 */
uint32_t ringbuf_available(const ringbuf_t *rb);

/**
 * @brief Get number of free bytes in the ring buffer
 */
uint32_t ringbuf_free(const ringbuf_t *rb);

/**
 * @brief Check if the ring buffer is empty
 */
bool ringbuf_is_empty(const ringbuf_t *rb);

/**
 * @brief Check if the ring buffer is full
 */
bool ringbuf_is_full(const ringbuf_t *rb);

/**
 * @brief Discard all data in the ring buffer
 */
void ringbuf_reset(ringbuf_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* RINGBUF_H_ */
