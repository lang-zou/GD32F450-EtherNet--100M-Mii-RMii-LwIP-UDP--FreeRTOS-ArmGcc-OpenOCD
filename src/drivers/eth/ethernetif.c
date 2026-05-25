/**
 * @file    ethernetif.c
 * @brief   LwIP network interface port — bridges LwIP netif to eth_mac driver
 *
 * This is the glue layer between LwIP stack and the GD32 ENET MAC driver.
 * Provides low_level_init(), low_level_output(), low_level_input().
 */

#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/ethip6.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "netif/ethernet.h"
#include "eth_mac.h"
#include "log.h"
#include "FreeRTOS.h"
#include "task.h"

/*===========================================================================
 * Forward declarations
 *===========================================================================*/
static err_t low_level_init(struct netif *netif);
static err_t low_level_output(struct netif *netif, struct pbuf *p);
static struct pbuf *low_level_input(struct netif *netif);
static void ethernetif_link_check(struct netif *netif);

/*===========================================================================
 * netif->state structure (holds device-specific data)
 *===========================================================================*/
struct ethernetif_data
{
    uint8_t  mac_addr[6];
    bool     link_up;
};

/*===========================================================================
 * ethernetif_init() — Called by netif_add()
 *===========================================================================*/
err_t ethernetif_init(struct netif *netif)
{
    struct ethernetif_data *eth_data;

    LWIP_ASSERT("netif != NULL", (netif != NULL));

    /* Allocate device state */
    eth_data = (struct ethernetif_data *)mem_malloc(sizeof(struct ethernetif_data));
    if (eth_data == NULL)
    {
        LOG_E("ethernetif_init: failed to alloc eth_data");
        return ERR_MEM;
    }
    netif->state = eth_data;

    /* Set netif fields */
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->output  = etharp_output;
#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif
    netif->linkoutput = low_level_output;
    netif->mtu        = 1500;
    netif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    /* Initialize MAC hardware */
    if (ERR_OK != low_level_init(netif))
    {
        mem_free(eth_data);
        netif->state = NULL;
        return ERR_IF;
    }

    return ERR_OK;
}

/*===========================================================================
 * low_level_init() — Initialize the Ethernet MAC hardware
 *===========================================================================*/
static err_t low_level_init(struct netif *netif)
{
    struct ethernetif_data *eth_data = (struct ethernetif_data *)netif->state;

    /* Initialize MAC + PHY */
#if (ETH_PHY_MODE == ETH_PHY_RMII)
    if (!eth_mac_init(ETH_MAC_MODE_RMII))
#else
    if (!eth_mac_init(ETH_MAC_MODE_MII))
#endif
    {
        LOG_E("low_level_init: eth_mac_init failed");
        return ERR_IF;
    }

    /* Get MAC address from driver */
    eth_mac_get_addr(eth_data->mac_addr);

    /* Set MAC address in netif */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    for (uint8_t i = 0; i < ETHARP_HWADDR_LEN; i++)
    {
        netif->hwaddr[i] = eth_data->mac_addr[i];
    }

    eth_data->link_up = true;

    LOG_I("low_level_init: MAC %02X:%02X:%02X:%02X:%02X:%02X",
          eth_data->mac_addr[0], eth_data->mac_addr[1],
          eth_data->mac_addr[2], eth_data->mac_addr[3],
          eth_data->mac_addr[4], eth_data->mac_addr[5]);

    return ERR_OK;
}

/*===========================================================================
 * low_level_output() — Send a packet
 *===========================================================================*/
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    struct ethernetif_data *eth_data = (struct ethernetif_data *)netif->state;
    (void)eth_data;

    if (p == NULL)
    {
        return ERR_ARG;
    }

    /* Copy pbuf chain to linear buffer */
    uint8_t  tx_buf[ETH_MAX_FRAME_SIZE];
    uint16_t total_len = 0;

    for (struct pbuf *q = p; q != NULL; q = q->next)
    {
        if (total_len + q->len > sizeof(tx_buf))
        {
            LOG_E("low_level_output: frame too large");
            LINK_STATS_INC(link.err);
            return ERR_BUF;
        }

        for (uint16_t i = 0; i < q->len; i++)
        {
            tx_buf[total_len + i] = ((uint8_t *)q->payload)[i];
        }
        total_len += q->len;
    }

    /* Send the frame */
    if (!eth_mac_send(tx_buf, total_len))
    {
        LOG_W("low_level_output: send failed");
        LINK_STATS_INC(link.err);
        return ERR_BUF;
    }

    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}

/*===========================================================================
 * low_level_input() — Receive a packet (polling mode)
 * Called periodically from LwIP thread or main loop
 *===========================================================================*/
static struct pbuf *low_level_input(struct netif *netif)
{
    struct ethernetif_data *eth_data = (struct ethernetif_data *)netif->state;
    (void)eth_data;

    uint8_t  rx_buf[ETH_MAX_FRAME_SIZE];
    uint16_t len = eth_mac_recv(rx_buf, sizeof(rx_buf));

    if (len == 0)
    {
        return NULL;
    }

    /* Allocate pbuf */
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p == NULL)
    {
        LOG_W("low_level_input: pbuf_alloc failed");
        LINK_STATS_INC(link.memerr);
        return NULL;
    }

    /* Copy data to pbuf */
    if (pbuf_take(p, rx_buf, len) != ERR_OK)
    {
        pbuf_free(p);
        return NULL;
    }

    LINK_STATS_INC(link.recv);
    return p;
}

/*===========================================================================
 * ethernetif_input() — Process received packets and feed to LwIP
 * Call this from the LwIP thread or main loop periodically
 *===========================================================================*/
void ethernetif_input(struct netif *netif)
{
    struct pbuf *p;
    static uint32_t link_check_counter = 0;

    /* Periodic link status check (every ~1000 polls) */
    if (++link_check_counter >= 1000)
    {
        link_check_counter = 0;
        ethernetif_link_check(netif);
    }

    /* Process all pending frames */
    while ((p = low_level_input(netif)) != NULL)
    {
        /* Pass to LwIP stack */
        if (ERR_OK != netif->input(p, netif))
        {
            pbuf_free(p);
        }
    }
}

/*===========================================================================
 * ethernetif_link_check() — Periodic link status update
 *===========================================================================*/
static void ethernetif_link_check(struct netif *netif)
{
    uint8_t speed, duplex;

    if (eth_mac_link_status(&speed, &duplex))
    {
        netif->flags |= NETIF_FLAG_LINK_UP;
    }
    else
    {
        netif->flags = (u8_t)(netif->flags & ~NETIF_FLAG_LINK_UP);
    }
}

/*===========================================================================
 * sys_now() — LwIP timekeeping (millisecond counter)
 *===========================================================================*/
u32_t sys_now(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}
