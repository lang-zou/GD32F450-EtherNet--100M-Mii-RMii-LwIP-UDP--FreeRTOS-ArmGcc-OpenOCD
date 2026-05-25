/**
 * @file    systick_rtos.c
 * @brief   SysTick interrupt handler for FreeRTOS tick
 *
 * FreeRTOS port.c provides the weak vPortSetupTimerInterrupt() which
 * configures SysTick. This file provides SysTick_Handler() which calls
 * xPortSysTickHandler() to advance the RTOS tick.
 */

#include "systick_rtos.h"
#include "FreeRTOS.h"
#include "task.h"

extern void xPortSysTickHandler(void);

/*===========================================================================
 * SysTick Interrupt Handler
 *===========================================================================*/
void SysTick_Handler(void)
{
    /* Notify FreeRTOS that a tick has occurred */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}

/*===========================================================================
 * Dummy init — actual SysTick setup is done by FreeRTOS port
 *===========================================================================*/
void systick_rtos_init(void)
{
    /* FreeRTOS port.c:vPortSetupTimerInterrupt() handles SysTick config */
}
