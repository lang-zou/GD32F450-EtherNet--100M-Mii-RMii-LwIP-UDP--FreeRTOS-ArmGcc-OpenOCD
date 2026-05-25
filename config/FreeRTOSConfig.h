/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS Kernel Configuration for GD32F450
 *
 * Tailored for:
 *   - GD32F450IK6 Cortex-M4F @ 200 MHz
 *   - 512 KB total SRAM, ~128-256 KB available for FreeRTOS heap
 *   - Ethernet + LwIP workload, primarily UDP
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Scheduler Configuration
 *===========================================================================*/
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      (200000000UL)
#define configSYSTICK_CLOCK_HZ                  (configCPU_CLOCK_HZ)
#define configTICK_RATE_HZ                      (1000)
#define configMAX_PRIORITIES                    (8)
#define configMINIMAL_STACK_SIZE                (128)
#define configMAX_TASK_NAME_LEN                 (16)
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0

/*===========================================================================
 * Memory Allocation
 * Using heap_4.c — best fit algorithm, supports freeing
 * 64 KB heap is sufficient for Ethernet + moderate application
 *===========================================================================*/
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (64 * 1024)
#define configAPPLICATION_ALLOCATED_HEAP         0

/*===========================================================================
 * Hook Functions
 *===========================================================================*/
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/*===========================================================================
 * Runtime Statistics
 *===========================================================================*/
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/*===========================================================================
 * Software Timer Configuration
 *===========================================================================*/
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            256

/*===========================================================================
 * Co-routine Configuration (not used)
 *===========================================================================*/
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         (2)

/*===========================================================================
 * Interrupt Parameters
 * Priority 4 (0xC0) or higher are safe for FreeRTOS API calls.
 * Priority 0-3 (0x00-0x30) are for time-critical interrupts.
 *
 * NVIC priority bits: GD32F4 uses 4 bits → 16 priority levels
 * configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 4
 * Meaning: priority 5-15 may safely call FreeRTOS API
 *===========================================================================*/
#define configKERNEL_INTERRUPT_PRIORITY         (15 << 4)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (4 << 4)

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  0x0F
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 4

/*===========================================================================
 * Assert / Debug
 *===========================================================================*/
#define configASSERT(x) \
    if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;); }

/*===========================================================================
 * FreeRTOS API Includes
 *===========================================================================*/
#include "FreeRTOS.h"

/* Optional API functions */
#define INCLUDE_vTaskDelay                      1

/* FreeRTOS portable memory management */
#define pvPortMalloc  pvPortMalloc
#define vPortFree     vPortFree

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
