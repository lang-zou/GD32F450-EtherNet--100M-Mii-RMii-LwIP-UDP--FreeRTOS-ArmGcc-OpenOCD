/**
 * @file    uart_console.c
 * @brief   USART0 debug console — interrupt-driven RX with ring buffer
 */

#include "uart_console.h"
#include "ringbuf.h"
#include <string.h>

/* RX ring buffer: 1 KB for console input */
#define CONSOLE_RX_BUF_SIZE 1024
static uint8_t  console_rx_buf[CONSOLE_RX_BUF_SIZE];
static ringbuf_t console_rx_ring;

/*===========================================================================
 * Initialization
 *===========================================================================*/
void uart_console_init(uint32_t baudrate)
{
    /* Enable USART0 and GPIO clocks */
    rcu_periph_clock_enable(CONSOLE_USART_RCU);
    rcu_periph_clock_enable(RCU_GPIOA);

    /* Configure TX pin: PA9, push-pull, alternate function */
    gpio_mode_set(CONSOLE_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  CONSOLE_TX_PIN);
    gpio_output_options_set(CONSOLE_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            CONSOLE_TX_PIN);
    gpio_af_set(CONSOLE_TX_PORT, CONSOLE_GPIO_AF, CONSOLE_TX_PIN);

    /* Configure RX pin: PA10, alternate function */
    gpio_mode_set(CONSOLE_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  CONSOLE_RX_PIN);
    gpio_output_options_set(CONSOLE_RX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            CONSOLE_RX_PIN);
    gpio_af_set(CONSOLE_RX_PORT, CONSOLE_GPIO_AF, CONSOLE_RX_PIN);

    /* Configure USART0 */
    usart_deinit(CONSOLE_USART);
    usart_baudrate_set(CONSOLE_USART, baudrate);
    usart_parity_config(CONSOLE_USART, USART_PM_NONE);
    usart_word_length_set(CONSOLE_USART, USART_WL_8BIT);
    usart_stop_bit_set(CONSOLE_USART, USART_STB_1BIT);

    /* Enable RX interrupt (non-blocking receive) */
    usart_interrupt_enable(CONSOLE_USART, USART_INT_RBNE);
    nvic_irq_enable(USART0_IRQn, 4, 0);

    /* Enable USART */
    usart_enable(CONSOLE_USART);
    usart_transmit_config(CONSOLE_USART, USART_TRANSMIT_ENABLE);
    usart_receive_config(CONSOLE_USART, USART_RECEIVE_ENABLE);

    /* Initialize RX ring buffer */
    ringbuf_init(&console_rx_ring, console_rx_buf, CONSOLE_RX_BUF_SIZE);
}

/*===========================================================================
 * TX (Blocking — used during init and from task context)
 *===========================================================================*/
/* UART TX timeout in polling loops (prevents deadlock) */
#define UART_TX_TIMEOUT 0xFFFFFFUL

void uart_console_send(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        uint32_t timeout = UART_TX_TIMEOUT;
        while (RESET == usart_flag_get(CONSOLE_USART, USART_FLAG_TBE))
        {
            if (--timeout == 0)
            {
                return;  /* Hardware fault — abort TX */
            }
        }
        usart_data_transmit(CONSOLE_USART, data[i]);
    }

    /* Wait for transmission complete */
    uint32_t timeout = UART_TX_TIMEOUT;
    while (RESET == usart_flag_get(CONSOLE_USART, USART_FLAG_TC))
    {
        if (--timeout == 0)
        {
            return;  /* Hardware fault — abort */
        }
    }
}

void uart_console_send_str(const char *str)
{
    uart_console_send((const uint8_t *)str, strlen(str));
}

bool uart_console_tx_done(void)
{
    return (SET == usart_flag_get(CONSOLE_USART, USART_FLAG_TC));
}

/*===========================================================================
 * RX (Non-blocking via ring buffer)
 *===========================================================================*/
uint32_t uart_console_recv(uint8_t *buf, uint32_t buf_size)
{
    return ringbuf_read(&console_rx_ring, buf, buf_size);
}

bool uart_console_recv_byte(uint8_t *byte)
{
    return ringbuf_get(&console_rx_ring, byte);
}

/*===========================================================================
 * USART0 Interrupt Handler
 * Captures received bytes into the ring buffer
 *===========================================================================*/
void USART0_IRQHandler(void)
{
    /* Receive Data Register Not Empty */
    if (SET == usart_interrupt_flag_get(CONSOLE_USART, USART_INT_FLAG_RBNE))
    {
        uint8_t byte = (uint8_t)usart_data_receive(CONSOLE_USART);
        ringbuf_put(&console_rx_ring, byte);

        /* Clear the interrupt flag */
        usart_interrupt_flag_clear(CONSOLE_USART, USART_INT_FLAG_RBNE);
    }

    /* Overrun error — clear flag */
    if (SET == usart_flag_get(CONSOLE_USART, USART_FLAG_ORERR))
    {
        usart_flag_clear(CONSOLE_USART, USART_FLAG_ORERR);
    }
}
