/**
 * @file    bsp_led.h
 * @brief   Simple on-board LED abstraction
 */

#ifndef BSP_LED_H_
#define BSP_LED_H_

#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

void bsp_led_init(void);
void bsp_led_on(void);
void bsp_led_off(void);
void bsp_led_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_H_ */
