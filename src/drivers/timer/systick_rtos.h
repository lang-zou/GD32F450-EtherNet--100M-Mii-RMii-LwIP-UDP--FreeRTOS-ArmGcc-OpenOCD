/**
 * @file    systick_rtos.h
 * @brief   SysTick timer configuration for FreeRTOS tick
 */

#ifndef SYSTICK_RTOS_H_
#define SYSTICK_RTOS_H_

#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SysTick for FreeRTOS
 * Configures SysTick to generate an interrupt at configTICK_RATE_HZ (1 kHz).
 * Called by FreeRTOS kernel via vPortSetupTimerInterrupt().
 */
void systick_rtos_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTICK_RTOS_H_ */
