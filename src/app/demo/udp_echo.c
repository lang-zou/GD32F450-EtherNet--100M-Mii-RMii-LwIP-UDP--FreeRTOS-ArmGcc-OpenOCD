/**
 * @file    udp_echo.c
 * @brief   UDP Echo demo — uses LwIP raw API (netconn/socket for simplicity)
 *
 * Network configuration is read from app_config.h defaults.
 */

#include "udp_echo.h"
#include "eth_mac.h"
#include "ethernetif.h"
#include "bsp_led.h"
#include "log.h"
#include "app_config.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/netif.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"

/* Static network interface */
static struct netif g_netif;

/*===========================================================================
 * Network Initialization
 *===========================================================================*/
bool network_init(void)
{
    LOG_I("network_init: Starting LwIP...");

    /* Initialize LwIP */
    lwip_init();

    /* Configure static IP */
    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr,
             NET_DEFAULT_IP_ADDR0, NET_DEFAULT_IP_ADDR1,
             NET_DEFAULT_IP_ADDR2, NET_DEFAULT_IP_ADDR3);
    IP4_ADDR(&netmask,
             NET_DEFAULT_MASK0, NET_DEFAULT_MASK1,
             NET_DEFAULT_MASK2, NET_DEFAULT_MASK3);
    IP4_ADDR(&gw,
             NET_DEFAULT_GW0, NET_DEFAULT_GW1,
             NET_DEFAULT_GW2, NET_DEFAULT_GW3);

    /* Add network interface */
    struct netif *result = netif_add(&g_netif, &ipaddr, &netmask, &gw,
                                      NULL, ethernetif_init, ethernetif_input);
    if (result == NULL)
    {
        LOG_E("network_init: netif_add failed");
        return false;
    }

    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    LOG_I("network_init: IP %d.%d.%d.%d",
          NET_DEFAULT_IP_ADDR0, NET_DEFAULT_IP_ADDR1,
          NET_DEFAULT_IP_ADDR2, NET_DEFAULT_IP_ADDR3);

    return true;
}

/*===========================================================================
 * UDP Echo Callback (Raw API)
 *===========================================================================*/
static void udp_echo_callback(void *arg, struct udp_pcb *pcb,
                              struct pbuf *p,
                              const ip_addr_t *addr, u16_t port)
{
    (void)arg;

    if (p == NULL)
    {
        return;
    }

    LOG_I("UDP rx: %u bytes from %s:%u",
          (unsigned int)p->tot_len, ipaddr_ntoa(addr), (unsigned int)port);

    /* Echo the data back */
    udp_sendto(pcb, p, addr, port);

    /* Toggle LED to indicate activity */
    bsp_led_toggle();

    /* Free the pbuf */
    pbuf_free(p);
}

/*===========================================================================
 * UDP Echo Task
 *===========================================================================*/
void udp_echo_task(void *pvParameters)
{
    (void)pvParameters;

    LOG_I("udp_echo_task: Starting...");

    /* Create UDP PCB */
    struct udp_pcb *pcb = udp_new();
    if (pcb == NULL)
    {
        LOG_E("udp_echo_task: udp_new failed");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    /* Bind to echo port */
    err_t err = udp_bind(pcb, IP_ADDR_ANY, NET_UDP_ECHO_PORT);
    if (err != ERR_OK)
    {
        LOG_E("udp_echo_task: udp_bind failed, err=%d", err);
        udp_remove(pcb);
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    /* Register receive callback */
    udp_recv(pcb, udp_echo_callback, NULL);

    LOG_I("udp_echo_task: Listening on UDP port %d", NET_UDP_ECHO_PORT);

    /* Main loop — process LwIP timers and incoming packets */
    while (1)
    {
        /* Process incoming Ethernet frames */
        ethernetif_input(&g_netif);

        /* Process LwIP timers */
        sys_check_timeouts();

        /* Yield to other tasks */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
