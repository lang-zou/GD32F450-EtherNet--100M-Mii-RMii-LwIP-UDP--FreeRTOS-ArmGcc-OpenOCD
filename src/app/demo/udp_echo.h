/**
 * @file    udp_echo.h
 * @brief   UDP Echo demo task — receives UDP and echoes back
 */

#ifndef UDP_ECHO_H_
#define UDP_ECHO_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UDP Echo task entry point
 * Binds UDP port 5000, echoes received data back to sender.
 * LED toggles on each received packet.
 */
void udp_echo_task(void *pvParameters);

/**
 * @brief Network initialization — call once before starting network tasks
 * Initializes ETH MAC + PHY + LwIP netif, sets up static IP.
 * @return true on success
 */
bool network_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UDP_ECHO_H_ */
