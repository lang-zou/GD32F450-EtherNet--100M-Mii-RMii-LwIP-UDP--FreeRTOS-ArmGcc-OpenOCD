/**
 * @file    cli_cmds.c
 * @brief   Built-in CLI commands: help, info, eth, ota, reset, ping
 */

#include "cli.h"
#include "eth_mac.h"
#include "board.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* External declarations from other modules */
extern void ota_enter_from_cli(void);

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/
static void cmd_help(int argc, char *argv[]);
static void cmd_info(int argc, char *argv[]);
static void cmd_eth(int argc, char *argv[]);
static void cmd_ota(int argc, char *argv[]);
static void cmd_reset(int argc, char *argv[]);

/*===========================================================================
 * Command Table
 *===========================================================================*/
static const cli_cmd_t builtin_cmds[] = {
    { "help",  "Show available commands",                    cmd_help },
    { "info",  "Show system information",                    cmd_info },
    { "eth",   "Show Ethernet status",                       cmd_eth  },
    { "ota",   "Enter OTA firmware update mode",             cmd_ota  },
    { "reset", "Software reset the system",                  cmd_reset},
};

/*===========================================================================
 * Register Built-in Commands
 *===========================================================================*/
void cli_register_builtin_cmds(void)
{
    size_t count = sizeof(builtin_cmds) / sizeof(builtin_cmds[0]);
    for (size_t i = 0; i < count; i++)
    {
        cli_register_cmd(&builtin_cmds[i]);
    }
}

/*===========================================================================
 * Command Handlers
 *===========================================================================*/

static void cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    cli_printf("Available commands:\r\n");
    size_t count = sizeof(builtin_cmds) / sizeof(builtin_cmds[0]);
    for (size_t i = 0; i < count; i++)
    {
        cli_printf("  %-8s — %s\r\n", builtin_cmds[i].name, builtin_cmds[i].help);
    }
}

static void cmd_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    cli_printf("MCU:      GD32F450IK6 @ %lu MHz\r\n",
               SystemCoreClock / 1000000UL);

    /* Task count */
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    cli_printf("Tasks:    %lu\r\n", (unsigned long)task_count);

    /* Heap usage */
    size_t free_heap = xPortGetFreeHeapSize();
    size_t min_heap  = xPortGetMinimumEverFreeHeapSize();
    cli_printf("Heap:     %u free / %u min ever\r\n",
               (unsigned int)free_heap, (unsigned int)min_heap);

    cli_printf("Tick:     %lu ms\r\n",
               (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));

    /* Reset reason (simplified) */
    cli_printf("Reset:    Power-on / Software\r\n");
}

static void cmd_eth(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint8_t speed  = 0;
    uint8_t duplex = 0;
    bool    link   = eth_mac_link_status(&speed, &duplex);

    cli_printf("Ethernet Status:\r\n");

#if (ETH_PHY_MODE == ETH_PHY_RMII)
    cli_printf("  Mode:   RMII (LAN8720A)\r\n");
#else
    cli_printf("  Mode:   MII (DP83848)\r\n");
#endif

    if (link)
    {
        cli_printf("  Link:   Up\r\n");
        cli_printf("  Speed:  %d Mbps\r\n", speed);
        cli_printf("  Duplex: %s\r\n", duplex ? "Full" : "Half");
    }
    else
    {
        cli_printf("  Link:   Down\r\n");
    }

    cli_printf("  IP:     %d.%d.%d.%d\r\n",
               NET_DEFAULT_IP_ADDR0, NET_DEFAULT_IP_ADDR1,
               NET_DEFAULT_IP_ADDR2, NET_DEFAULT_IP_ADDR3);
}

static void cmd_ota(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    cli_printf("Entering OTA mode...\r\n");

#if ENABLE_OTA
    ota_enter_from_cli();
#else
    cli_printf("OTA feature is disabled in app_config.h\r\n");
#endif

    cli_printf("Returning from OTA mode.\r\n");
}

static void cmd_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_printf("Resetting system...\r\n");
    NVIC_SystemReset();
}
