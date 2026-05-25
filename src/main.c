/**
 * @file    main.c
 * @brief   Single entry point — bootloader and app share this main()
 *
 * Boot flow:
 *   1. Hardware init (clock + peripherals)
 *   2. boot_get_mode() determines path
 *   3. Bootloader path: ota_bootloader_entry() → receive firmware → reset
 *   4. App path: FreeRTOS init → create tasks → vTaskStartScheduler()
 */

#include "board.h"
#include "bsp_led.h"
#include "boot.h"
#include "ota.h"
#include "udp_echo.h"
#include "cli.h"
#include "cli_cmds_ext.h"
#include "uart_console.h"
#include "log.h"
#include "app_config.h"

#include "FreeRTOS.h"
#include "task.h"

/*===========================================================================
 * Task Definitions
 *===========================================================================*/

#if ENABLE_LED_DEMO
static void led_task(void *pvParameters)
{
    (void)pvParameters;
    while (1)
    {
        bsp_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(LED_BLINK_PERIOD_MS));
    }
}
#endif /* ENABLE_LED_DEMO */

#if ENABLE_SHELL
static void cli_task(void *pvParameters)
{
    (void)pvParameters;
    cli_init();
    cli_register_builtin_cmds();

    while (1)
    {
        cli_process();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif /* ENABLE_SHELL */

/*===========================================================================
 * App Entry
 *===========================================================================*/
static void app_entry(void)
{
    LOG_I("========================================");
    LOG_I("  GD32F450 FreeRTOS Application");
    LOG_I("  MCU: 200 MHz, RTOS: FreeRTOS");
    LOG_I("========================================");

    /* Initialize all peripherals */
    board_periph_init();
    bsp_led_init();
    uart_console_init(CONSOLE_BAUDRATE);

    LOG_I("Hardware initialized.");

    /* Initialize Ethernet + LwIP */
#if ENABLE_UDP_DEMO
    if (network_init())
    {
        LOG_I("Network initialized.");
    }
    else
    {
        LOG_E("Network init failed!");
    }
#endif

    /* Create tasks — lower priority number = lower priority */
    uint32_t prio = tskIDLE_PRIORITY;

#if ENABLE_LED_DEMO
    xTaskCreate(led_task, "led", 128, NULL, prio + 1, NULL);
#endif

#if ENABLE_SHELL
    xTaskCreate(cli_task, "cli", 512, NULL, prio + 1, NULL);
#endif

#if ENABLE_UDP_DEMO
    xTaskCreate(udp_echo_task, "udp_echo", 1024, NULL, prio + 2, NULL);
#endif

    LOG_I("Starting FreeRTOS scheduler...");

    /* Start the scheduler — never returns */
    vTaskStartScheduler();

    /* Should never reach here */
    LOG_E("Scheduler returned!");
    while (1)
    {
    }
}

/*===========================================================================
 * main() — Single Entry Point
 *===========================================================================*/
int main(void)
{
    /* Phase 1: Hardware initialization (shared by both paths) */
    board_clock_init();

    /* Phase 2: Determine boot mode */
    boot_mode_t mode = boot_get_mode();

    /* Phase 3a: Bootloader path */
    if (mode == BOOT_MODE_BOOTLOADER)
    {
        /* Minimal init for OTA */
        board_periph_init();
        uart_console_init(CONSOLE_BAUDRATE);
        LOG_I("Entering bootloader mode...");

        ota_bootloader_entry();

        /* If OTA times out, fall through to app mode */
        LOG_I("OTA timeout — falling through to app mode");
    }

    /* Phase 3b: Application path */
    app_entry();

    /* Unreachable */
    return 0;
}

/*===========================================================================
 * FreeRTOS Hooks (required when stack overflow checking enabled)
 *===========================================================================*/
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /* Stack overflow detected — log and halt */
    LOG_E("STACK OVERFLOW in task: %s", pcTaskName);
    taskDISABLE_INTERRUPTS();
    while (1) { }
}
