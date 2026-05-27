/**
 * @file    ota.c
 * @brief   OTA firmware update implementation — UDP-based protocol with LwIP raw API
 *
 * During OTA, firmware is received to the OTA buffer flash area first,
 * then copied to the app area after successful verification. This prevents
 * bricking if the update is interrupted.
 *
 * Uses LwIP raw API (NO_SYS=1) — no RTOS threads needed in bootloader mode.
 * Packets are queued from the UDP receive callback and processed in a poll loop.
 */

#include "ota.h"
#include "boot.h"
#include "app_config.h"
#include "flash_if.h"
#include "crc32.h"
#include "log.h"
#include "board.h"

/* LwIP raw API headers */
#include "lwip/init.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"

#include <string.h>

/*===========================================================================
 * External Declarations
 *===========================================================================*/
extern err_t ethernetif_init(struct netif *netif);

/*===========================================================================
 * Static Netif for Bootloader Mode
 *===========================================================================*/
static struct netif ota_netif;

/*===========================================================================
 * Packet Queue
 * Packets from the UDP receive callback are queued here and processed
 * by the main poll loop. Single-threaded (no RTOS in bootloader), so
 * no concurrency protection needed.
 *===========================================================================*/
#define OTA_PKT_QUEUE_SIZE 8

typedef struct {
    struct pbuf *p;
    ip_addr_t    src_addr;
    uint16_t     src_port;
} ota_pkt_t;

static ota_pkt_t  ota_pkt_queue[OTA_PKT_QUEUE_SIZE];
static uint8_t    ota_pkt_head  = 0;
static uint8_t    ota_pkt_tail  = 0;
static uint8_t    ota_pkt_count = 0;

/*===========================================================================
 * OTA State
 *===========================================================================*/
typedef struct {
    uint32_t total_size;
    uint32_t bytes_written;
    uint32_t crc;           /* Running CRC32 (initial = 0xFFFFFFFF) */
    bool     active;
} ota_session_state_t;

static ota_session_state_t ota_sess;
static struct udp_pcb     *ota_pcb = NULL;

/* Last known sender — reply ACKs to whoever sent the REQ */
static ip_addr_t ota_src_addr;
static uint16_t  ota_src_port;

/*===========================================================================
 * UDP Receive Callback
 * Queues incoming packets for main-loop processing.
 *===========================================================================*/
static void ota_recv_callback(void *arg, struct udp_pcb *pcb,
                               struct pbuf *p, const ip_addr_t *addr,
                               uint16_t port)
{
    (void)arg;
    (void)pcb;

    if (ota_pkt_count >= OTA_PKT_QUEUE_SIZE) {
        /* Queue full — drop the packet */
        pbuf_free(p);
        return;
    }

    ota_pkt_queue[ota_pkt_head].p        = p;
    ota_pkt_queue[ota_pkt_head].src_addr = *addr;
    ota_pkt_queue[ota_pkt_head].src_port = port;
    ota_pkt_head = (ota_pkt_head + 1) % OTA_PKT_QUEUE_SIZE;
    ota_pkt_count++;
}

/*===========================================================================
 * Dequeue one packet (returns NULL if queue empty)
 *===========================================================================*/
static struct pbuf *ota_dequeue_pkt(ip_addr_t *addr, uint16_t *port)
{
    if (ota_pkt_count == 0) {
        return NULL;
    }

    ota_pkt_t pkt = ota_pkt_queue[ota_pkt_tail];
    ota_pkt_tail = (ota_pkt_tail + 1) % OTA_PKT_QUEUE_SIZE;
    ota_pkt_count--;

    *addr = pkt.src_addr;
    *port = pkt.src_port;
    return pkt.p;
}

/*===========================================================================
 * Send ACK via UDP
 *===========================================================================*/
static void ota_send_ack(uint32_t seq, uint8_t status)
{
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, OTA_ACK_SIZE, PBUF_RAM);
    if (p == NULL) {
        LOG_E("OTA: pbuf_alloc failed for ACK");
        return;
    }

    uint8_t *out = (uint8_t *)p->payload;
    out[0] = (uint8_t)(OTA_MAGIC_ACK >> 24);
    out[1] = (uint8_t)(OTA_MAGIC_ACK >> 16);
    out[2] = (uint8_t)(OTA_MAGIC_ACK >> 8);
    out[3] = (uint8_t)(OTA_MAGIC_ACK);
    out[4] = (uint8_t)(seq >> 24);
    out[5] = (uint8_t)(seq >> 16);
    out[6] = (uint8_t)(seq >> 8);
    out[7] = (uint8_t)(seq);
    out[8] = status;

    udp_sendto(ota_pcb, p, &ota_src_addr, ota_src_port);
    pbuf_free(p);
}

/*===========================================================================
 * Parse REQ packet: extract total firmware size
 *===========================================================================*/
static bool ota_parse_req(struct pbuf *p, uint32_t *total_size)
{
    if (p->tot_len < OTA_REQ_SIZE) return false;

    uint8_t buf[OTA_REQ_SIZE];
    pbuf_copy_partial(p, buf, OTA_REQ_SIZE, 0);

    uint32_t magic = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                     ((uint32_t)buf[2] << 8)  | ((uint32_t)buf[3]);
    if (magic != OTA_MAGIC_REQ) return false;

    *total_size = ((uint32_t)buf[9]  << 24) | ((uint32_t)buf[10] << 16) |
                  ((uint32_t)buf[11] << 8)  | ((uint32_t)buf[12]);
    return true;
}

/*===========================================================================
 * Parse DATA packet header: extract seq and payload size
 *===========================================================================*/
static bool ota_parse_data_hdr(struct pbuf *p, uint32_t *seq, uint16_t *payload_size)
{
    if (p->tot_len < OTA_DATA_HDR) return false;

    uint8_t buf[OTA_DATA_HDR];
    pbuf_copy_partial(p, buf, OTA_DATA_HDR, 0);

    uint32_t magic = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                     ((uint32_t)buf[2] << 8)  | ((uint32_t)buf[3]);
    if (magic != OTA_MAGIC_DATA) return false;

    *seq          = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                    ((uint32_t)buf[6] << 8)  | ((uint32_t)buf[7]);
    *payload_size = ((uint16_t)buf[8] << 8)  | ((uint16_t)buf[9]);
    return true;
}

/*===========================================================================
 * Parse DONE packet: extract total_size and expected CRC32
 *===========================================================================*/
static bool ota_parse_done(struct pbuf *p, uint32_t *total_size, uint32_t *crc)
{
    if (p->tot_len < OTA_DONE_SIZE) return false;

    uint8_t buf[OTA_DONE_SIZE];
    pbuf_copy_partial(p, buf, OTA_DONE_SIZE, 0);

    uint32_t magic = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                     ((uint32_t)buf[2] << 8)  | ((uint32_t)buf[3]);
    if (magic != OTA_MAGIC_DONE) return false;

    *total_size = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                  ((uint32_t)buf[6] << 8)  | ((uint32_t)buf[7]);
    *crc = ((uint32_t)buf[8]  << 24) | ((uint32_t)buf[9]  << 16) |
           ((uint32_t)buf[10] << 8)  | ((uint32_t)buf[11]);
    return true;
}

/*===========================================================================
 * Copy verified firmware from OTA buffer to app flash area
 *===========================================================================*/
static bool ota_copy_to_app(uint32_t size)
{
    LOG_I("OTA: Copying %lu bytes from 0x%08lX to 0x%08lX",
          (unsigned long)size, (unsigned long)FLASH_OTA_BUFFER_BASE,
          (unsigned long)FLASH_APP_BASE);

    /* Erase the entire app area */
    if (!flash_erase_range(FLASH_APP_BASE, FLASH_APP_SIZE)) {
        LOG_E("OTA: Failed to erase app area");
        return false;
    }

    /* Copy in 1 KB chunks (RAM-limited) */
    #define COPY_BUF_SIZE 1024
    static uint8_t copy_buf[COPY_BUF_SIZE];

    for (uint32_t offset = 0; offset < size; offset += COPY_BUF_SIZE) {
        uint32_t chunk = COPY_BUF_SIZE;
        if (size - offset < COPY_BUF_SIZE) {
            chunk = size - offset;
        }

        flash_read_buf(FLASH_OTA_BUFFER_BASE + offset, copy_buf, chunk);
        if (!flash_write_buf(FLASH_APP_BASE + offset, copy_buf, chunk)) {
            LOG_E("OTA: Flash write failed at offset 0x%08lX",
                  (unsigned long)offset);
            return false;
        }
    }

    LOG_I("OTA: Copy complete — %lu bytes written", (unsigned long)size);
    return true;
}

/*===========================================================================
 * ota_network_init() — Set up minimal LwIP netif for bootloader
 *===========================================================================*/
bool ota_network_init(void)
{
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

    /* Add the netif — ethernetif_init() calls eth_mac_init() internally */
    struct netif *nf = netif_add(&ota_netif, &ipaddr, &netmask, &gw,
                                  NULL, ethernetif_init, netif_input);
    if (nf == NULL) {
        LOG_E("OTA: netif_add failed");
        return false;
    }

    netif_set_default(&ota_netif);
    netif_set_up(&ota_netif);

    LOG_I("OTA: Netif up — IP: %d.%d.%d.%d",
          NET_DEFAULT_IP_ADDR0, NET_DEFAULT_IP_ADDR1,
          NET_DEFAULT_IP_ADDR2, NET_DEFAULT_IP_ADDR3);

    return true;
}

/*===========================================================================
 * ota_bootloader_entry() — Main OTA flow
 *
 * Called from main() when boot mode is BOOT_MODE_BOOTLOADER.
 * Uses LwIP raw API with a poll loop. Blocks until update received
 * or timeout expires.
 *===========================================================================*/
void ota_bootloader_entry(void)
{
    LOG_I("========================================");
    LOG_I("OTA Bootloader Mode");
    LOG_I("Waiting for firmware update (timeout: %d ms)", OTA_TIMEOUT_MS);
    LOG_I("Send firmware via UDP port %d", NET_OTA_PORT);
    LOG_I("========================================");

    /* Create UDP PCB */
    ota_pcb = udp_new();
    if (ota_pcb == NULL) {
        LOG_E("OTA: Failed to create UDP PCB");
        return;
    }

    /* Bind to OTA port */
    ip_addr_t bind_addr = IPADDR4_INIT(IPADDR_ANY);
    if (udp_bind(ota_pcb, &bind_addr, NET_OTA_PORT) != ERR_OK) {
        LOG_E("OTA: Failed to bind UDP port %d", NET_OTA_PORT);
        udp_remove(ota_pcb);
        ota_pcb = NULL;
        return;
    }

    /* Register receive callback */
    udp_recv(ota_pcb, ota_recv_callback, NULL);

    LOG_I("OTA: UDP port %d bound — waiting for REQ packet...", NET_OTA_PORT);

    /* Reset session state */
    memset(&ota_sess, 0, sizeof(ota_sess));
    ota_pkt_head  = 0;
    ota_pkt_tail  = 0;
    ota_pkt_count = 0;

    /* OTA protocol phases */
    typedef enum { PHASE_REQ, PHASE_DATA, PHASE_DONE } phase_t;
    phase_t  phase        = PHASE_REQ;
    uint32_t expected_seq = 0;
    uint32_t deadline_ms  = (uint32_t)OTA_TIMEOUT_MS;

    while (deadline_ms > 0) {
        /* Poll LwIP internal timeouts (ARP, DHCP, etc.) */
        sys_check_timeouts();

        /* Process all queued packets */
        while (ota_pkt_count > 0) {
            ip_addr_t addr;
            uint16_t  port;
            struct pbuf *pkt = ota_dequeue_pkt(&addr, &port);

            switch (phase) {

            /* --------------------------------------------------------
             * PHASE_REQ: Wait for OTA REQ with total firmware size
             * -------------------------------------------------------- */
            case PHASE_REQ: {
                uint32_t total_size;
                if (!ota_parse_req(pkt, &total_size)) {
                    pbuf_free(pkt);
                    continue;  /* Ignore non-REQ packets during this phase */
                }

                /* Validate firmware size */
                if (total_size == 0 || total_size > FLASH_OTA_BUFFER_SIZE) {
                    LOG_E("OTA: Invalid firmware size: %lu bytes (max %lu)",
                          (unsigned long)total_size,
                          (unsigned long)FLASH_OTA_BUFFER_SIZE);
                    ota_src_addr = addr;
                    ota_src_port = port;
                    ota_send_ack(0, OTA_ACK_ERR_SIZE);
                    pbuf_free(pkt);
                    continue;
                }

                LOG_I("OTA: REQ received — firmware size: %lu bytes",
                      (unsigned long)total_size);

                /* Erase OTA buffer area */
                flash_erase_range(FLASH_OTA_BUFFER_BASE, FLASH_OTA_BUFFER_SIZE);

                /* Enter data transfer phase */
                ota_sess.total_size    = total_size;
                ota_sess.bytes_written = 0;
                ota_sess.crc           = 0xFFFFFFFF;
                ota_sess.active        = true;
                ota_src_addr           = addr;
                ota_src_port           = port;

                ota_send_ack(0, OTA_ACK_OK);
                phase        = PHASE_DATA;
                expected_seq = 1;
                deadline_ms  = (uint32_t)OTA_CHUNK_TIMEOUT_MS;

                LOG_I("OTA: ACK sent — entering data phase, waiting for seq %lu...",
                      (unsigned long)expected_seq);
                break;
            }

            /* --------------------------------------------------------
             * PHASE_DATA: Receive DATA chunks, write to flash
             * -------------------------------------------------------- */
            case PHASE_DATA: {
                uint32_t seq;
                uint16_t payload_size;

                if (!ota_parse_data_hdr(pkt, &seq, &payload_size)) {
                    pbuf_free(pkt);
                    continue;
                }

                /* Verify sequence number */
                if (seq != expected_seq) {
                    LOG_W("OTA: Seq mismatch — expected %lu, got %lu",
                          (unsigned long)expected_seq, (unsigned long)seq);
                    ota_send_ack(seq, OTA_ACK_ERR_SEQ);
                    pbuf_free(pkt);
                    continue;
                }

                /* Check for overflow */
                if (ota_sess.bytes_written + payload_size > ota_sess.total_size) {
                    LOG_E("OTA: Data overflow");
                    ota_send_ack(seq, OTA_ACK_ERR_OVERFLOW);
                    phase = PHASE_REQ;  /* Abort — back to waiting for new REQ */
                    pbuf_free(pkt);
                    break;
                }

                /* Extract payload and write to flash */
                uint8_t payload[OTA_MAX_PAYLOAD];
                pbuf_copy_partial(pkt, payload, payload_size, OTA_DATA_HDR);

                if (!flash_write_buf(FLASH_OTA_BUFFER_BASE + ota_sess.bytes_written,
                                     payload, payload_size)) {
                    LOG_E("OTA: Flash write failed at offset 0x%08lX",
                          (unsigned long)ota_sess.bytes_written);
                    ota_send_ack(seq, OTA_ACK_ERR_FLASH);
                    ota_sess.active = false;
                    pbuf_free(pkt);
                    return;
                }

                /* Update running CRC32 */
                ota_sess.crc = crc32_update(ota_sess.crc, payload, payload_size);
                ota_sess.bytes_written += payload_size;

                ota_send_ack(seq, OTA_ACK_OK);
                expected_seq++;
                deadline_ms = (uint32_t)OTA_CHUNK_TIMEOUT_MS;

                /* Check if all data received */
                if (ota_sess.bytes_written >= ota_sess.total_size) {
                    LOG_I("OTA: All %lu data bytes received — waiting for DONE...",
                          (unsigned long)ota_sess.total_size);
                    phase = PHASE_DONE;
                }
                break;
            }

            /* --------------------------------------------------------
             * PHASE_DONE: Verify CRC32, copy to app, reboot
             * -------------------------------------------------------- */
            case PHASE_DONE: {
                uint32_t total_size, expected_crc;

                if (!ota_parse_done(pkt, &total_size, &expected_crc)) {
                    pbuf_free(pkt);
                    continue;
                }

                uint32_t actual_crc = ota_sess.crc ^ 0xFFFFFFFF;

                LOG_I("OTA: DONE received");
                LOG_I("OTA: CRC32 expected=0x%08lX  actual=0x%08lX",
                      (unsigned long)expected_crc, (unsigned long)actual_crc);

                if (expected_crc == actual_crc && total_size == ota_sess.total_size) {
                    LOG_I("OTA: CRC32 OK — flashing application area...");

                    if (ota_copy_to_app(total_size)) {
                        ota_send_ack(0, OTA_ACK_OK);
                        LOG_I("OTA: Update complete — rebooting in 100 ms...");
                        board_delay_ms(100);
                        udp_remove(ota_pcb);
                        ota_pcb = NULL;
                        NVIC_SystemReset();
                    } else {
                        LOG_E("OTA: Copy to app area failed!");
                        ota_send_ack(0, OTA_ACK_ERR_COPY);
                    }
                } else {
                    LOG_E("OTA: CRC32 verification failed!");
                    ota_send_ack(0, OTA_ACK_ERR_CRC);
                }

                ota_sess.active = false;
                pbuf_free(pkt);
                return;
            }

            } /* switch(phase) */

            pbuf_free(pkt);
        } /* while packets */

        /* ~1 ms delay per iteration (DWT-based) */
        board_delay_us(1000);
        deadline_ms--;
    } /* while deadline */

    /* ── Timeout ── */
    if (ota_sess.active) {
        LOG_W("OTA: Timeout — update incomplete (%lu / %lu bytes received)",
              (unsigned long)ota_sess.bytes_written,
              (unsigned long)ota_sess.total_size);
    } else {
        LOG_I("OTA: No update received — booting application...");
    }

    /* Cleanup */
    if (ota_pcb != NULL) {
        udp_remove(ota_pcb);
        ota_pcb = NULL;
    }
}

/*===========================================================================
 * ota_enter_from_cli() — CLI-triggered OTA entry
 *===========================================================================*/
void ota_enter_from_cli(void)
{
    LOG_I("OTA: Manual entry from CLI");
    boot_set_ota_flag();
    LOG_I("OTA: Flag set — resetting to enter bootloader...");
    board_delay_ms(100);
    NVIC_SystemReset();
}
