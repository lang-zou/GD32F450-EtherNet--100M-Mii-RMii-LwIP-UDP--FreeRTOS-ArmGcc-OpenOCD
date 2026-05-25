/**
 * @file    log.c
 * @brief   Logging implementation — output via UART console
 */

#include "log.h"
#include "uart_console.h"
#include <stdio.h>
#include <stdarg.h>

/* Buffer for formatted log messages */
#define LOG_BUF_SIZE 256

/*===========================================================================
 * Core Output
 *===========================================================================*/
void log_output(const char *level_str, const char *file, int line,
                const char *fmt, ...)
{
    char buf[LOG_BUF_SIZE];
    int  len;

    /* Extract filename from full path */
    const char *fname = file;
    const char *p     = file;
    while (*p)
    {
        if (*p == '/' || *p == '\\')
        {
            fname = p + 1;
        }
        p++;
    }

    /* Format: "[L] file:line — message\r\n" */
    len = snprintf(buf, sizeof(buf), "[%s] %s:%d — ", level_str, fname, line);
    if (len < 0 || (size_t)len >= sizeof(buf))
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int remain = (int)sizeof(buf) - len;
    int msg_len = vsnprintf(buf + len, (size_t)(remain > 0 ? remain : 0), fmt, args);
    va_end(args);

    if (msg_len < 0)
    {
        return;
    }

    len += msg_len;
    if (len >= (int)sizeof(buf) - 2)
    {
        len = (int)sizeof(buf) - 3;
    }
    buf[len++] = '\r';
    buf[len++] = '\n';
    buf[len]   = '\0';

    uart_console_send((const uint8_t *)buf, (uint32_t)len);
}
