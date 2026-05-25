/**
 * @file    log.h
 * @brief   Lightweight logging system with compile-time level filtering
 *
 * Usage:
 *   LOG_E("error: %d", err);      // Error — always compiled
 *   LOG_W("retry %d", n);         // Warning — LOG_LEVEL >= 2
 *   LOG_I("init done");           // Info — LOG_LEVEL >= 3
 *   LOG_D("val=0x%02X", reg);     // Debug — LOG_LEVEL >= 4
 *
 * Output goes to UART console via uart_console_send().
 */

#ifndef LOG_H_
#define LOG_H_

#include <stdint.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Log Levels
 *===========================================================================*/
#define LOG_LVL_ERROR 1
#define LOG_LVL_WARN  2
#define LOG_LVL_INFO  3
#define LOG_LVL_DEBUG 4

/*===========================================================================
 * Core Log Function
 *===========================================================================*/
void log_output(const char *level_str, const char *file, int line, const char *fmt, ...);

/*===========================================================================
 * Level-based Macros
 *===========================================================================*/
#define LOG_E(fmt, ...) \
    log_output("E", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#if LOG_LEVEL >= LOG_LVL_WARN
#define LOG_W(fmt, ...) \
    log_output("W", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_W(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LVL_INFO
#define LOG_I(fmt, ...) \
    log_output("I", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_I(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LVL_DEBUG
#define LOG_D(fmt, ...) \
    log_output("D", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_D(fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* LOG_H_ */
