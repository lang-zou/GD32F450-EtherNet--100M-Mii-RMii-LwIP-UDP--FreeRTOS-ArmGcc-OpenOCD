/**
 * @file    ringbuf.c
 * @brief   Lock-free SPSC ring buffer implementation
 *
 * All size calculations use power-of-2 masking for efficiency on Cortex-M4.
 * Head is written only by the producer (interrupt context).
 * Tail is written only by the consumer (task context).
 */

#include "ringbuf.h"

/*===========================================================================
 * Public API
 *===========================================================================*/
void ringbuf_init(ringbuf_t *rb, uint8_t *buffer, uint32_t size)
{
    rb->buffer = buffer;
    rb->size   = size;
    rb->head   = 0;
    rb->tail   = 0;
}

bool ringbuf_put(ringbuf_t *rb, uint8_t byte)
{
    uint32_t head = rb->head;
    uint32_t next = (head + 1) & (rb->size - 1);

    if (next == rb->tail)
    {
        return false;  /* Full */
    }

    rb->buffer[head] = byte;
    rb->head = next;
    return true;
}

bool ringbuf_get(ringbuf_t *rb, uint8_t *byte)
{
    uint32_t tail = rb->tail;

    if (tail == rb->head)
    {
        return false;  /* Empty */
    }

    *byte = rb->buffer[tail];
    rb->tail = (tail + 1) & (rb->size - 1);
    return true;
}

uint32_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint32_t len)
{
    uint32_t written = 0;

    while (written < len && !ringbuf_is_full(rb))
    {
        uint32_t head = rb->head;
        uint32_t next = (head + 1) & (rb->size - 1);

        rb->buffer[head] = data[written];
        rb->head = next;
        written++;
    }

    return written;
}

uint32_t ringbuf_read(ringbuf_t *rb, uint8_t *data, uint32_t len)
{
    uint32_t read = 0;

    while (read < len && !ringbuf_is_empty(rb))
    {
        uint32_t tail = rb->tail;
        data[read] = rb->buffer[tail];
        rb->tail = (tail + 1) & (rb->size - 1);
        read++;
    }

    return read;
}

uint32_t ringbuf_available(const ringbuf_t *rb)
{
    return (rb->head - rb->tail) & (rb->size - 1);
}

uint32_t ringbuf_free(const ringbuf_t *rb)
{
    return (rb->size - 1) - ringbuf_available(rb);
}

bool ringbuf_is_empty(const ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

bool ringbuf_is_full(const ringbuf_t *rb)
{
    return ((rb->head + 1) & (rb->size - 1)) == rb->tail;
}

void ringbuf_reset(ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}
