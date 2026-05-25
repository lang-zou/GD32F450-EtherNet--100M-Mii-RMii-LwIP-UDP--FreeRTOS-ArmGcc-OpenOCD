/**
 * @file    uart_console.h
 * @brief   Debug console via USART0 — 115200-8-N-1
 */

#ifndef UART_CONSOLE_H_
#define UART_CONSOLE_H_

#include "board.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_console_init(uint32_t baudrate);
void uart_console_send(const uint8_t *data, uint32_t len);
void uart_console_send_str(const char *str);
uint32_t uart_console_recv(uint8_t *buf, uint32_t buf_size);
bool uart_console_recv_byte(uint8_t *byte);
bool uart_console_tx_done(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_CONSOLE_H_ */
