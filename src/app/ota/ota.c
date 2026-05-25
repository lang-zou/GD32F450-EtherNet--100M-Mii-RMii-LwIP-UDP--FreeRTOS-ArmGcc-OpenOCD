/**
 * @file    ota.c
 * @brief   OTA firmware update implementation — UDP-based protocol
 *
 * During OTA, firmware is received to the OTA buffer flash area first,
 * then copied to the app area after successful verification. This prevents
 * bricking if the update is interrupted.
 */

#include "ota.h"
#include "boot.h"
#include "app_config.h"
#include "flash_if.h"
#include "crc32.h"
#include "log.h"
#include "board.h"
#include "gd32f4xx_enet.h"

/* OTA protocol magic numbers */
#define OTA_MAGIC_REQ   0x4F544151  /* "OTAQ" */
#define OTA_MAGIC_ACK   0x4F544141  /* "OTAA" */
#define OTA_MAGIC_DATA  0x4F544144  /* "OTAD" */
#define OTA_MAGIC_DONE  0x4F54414F  /* "OTAO" */

/* Protocol packet sizes */
#define OTA_REQ_SIZE    13   /* magic(4) + cmd(1) + seq(4) + size(4) */
#define OTA_ACK_SIZE    9    /* magic(4) + seq(4) + status(1) */
#define OTA_DATA_HDR    10   /* magic(4) + seq(4) + size(2) */
#define OTA_DONE_SIZE   12   /* magic(4) + total_size(4) + crc32(4) */
#define OTA_MAX_PAYLOAD 1024
#define OTA_MAX_PACKET  (OTA_DATA_HDR + OTA_MAX_PAYLOAD)

/*===========================================================================
 * Static State
 *===========================================================================*/
typedef struct
{
    uint32_t total_size;
    uint32_t bytes_written;
    uint32_t crc;
    bool     active;
} ota_state_t;

static ota_state_t ota_state;

/*===========================================================================
 * Static Helpers
 *===========================================================================*/
static bool ota_send_ack(uint32_t seq, uint8_t status)
{
    /* Placeholder: In a full implementation, this sends a UDP ACK back.
     * For now, the protocol logic is documented but actual UDP socket
     * operations would require LwIP initialization which
     * is handled in the application layer.
     */
    (void)seq;
    (void)status;
    return true;
}

static void ota_abort(void)
{
    ota_state.active = false;
    LOG_E("OTA: Aborted");
}

/*===========================================================================
 * ota_bootloader_entry() — Main OTA flow
 *
 * Called from main() when boot mode is BOOT_MODE_BOOTLOADER.
 * This is a simplified version that prints instructions to the console.
 * The full implementation uses LwIP UDP sockets for actual data transfer.
 *===========================================================================*/
void ota_bootloader_entry(void)
{
    LOG_I("========================================");
    LOG_I("OTA Bootloader Mode");
    LOG_I("Waiting for firmware update (timeout: %d ms)", OTA_TIMEOUT_MS);
    LOG_I("Send firmware via UDP port %d", NET_OTA_PORT);
    LOG_I("========================================");

    /* Initialize minimum hardware for bootloader */
    /* PHY + MAC are assumed initialized from board_periph_init() */

    /* In a production implementation:
     * 1. Init LwIP with UDP-only config
     * 2. Bind UDP socket to NET_OTA_PORT
     * 3. Wait for OTA REQ packet (with timeout)
     * 4. Receive DATA chunks, write to OTA buffer flash area
     * 5. Receive DONE packet with CRC32
     * 6. Verify CRC32 of received data
     * 7. Copy from OTA buffer to app area
     * 8. Set boot flag and NVIC_SystemReset()
     */

    /* For this demo: timeout and fall through to app */
    board_delay_ms(OTA_TIMEOUT_MS);

    LOG_I("OTA: No update received, booting application...");
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
