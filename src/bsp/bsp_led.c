/**
 * @file    bsp_led.c
 * @brief   On-board LED initialization and control
 */

#include "bsp_led.h"

/*===========================================================================
 * Public API
 *===========================================================================*/
void bsp_led_init(void)
{
    rcu_periph_clock_enable(LED_RCU_CLOCK);

    gpio_mode_set(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_output_options_set(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LED_PIN);
    LED_OFF();
}

void bsp_led_on(void)
{
    LED_ON();
}

void bsp_led_off(void)
{
    LED_OFF();
}

void bsp_led_toggle(void)
{
    LED_TOGGLE();
}
