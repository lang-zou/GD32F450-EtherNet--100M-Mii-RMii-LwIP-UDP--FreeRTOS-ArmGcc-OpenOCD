/**
 * @file    cli.h
 * @brief   Lightweight UART command-line shell
 *
 * Simple command parser with registration API. Commands are added via
 * cli_register_cmd() and dispatched by cli_process() which reads from
 * the UART console ring buffer.
 */

#ifndef CLI_H_
#define CLI_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Types
 *===========================================================================*/
typedef void (*cli_handler_t)(int argc, char *argv[]);

typedef struct
{
    const char     *name;
    const char     *help;
    cli_handler_t   handler;
} cli_cmd_t;

/*===========================================================================
 * API
 *===========================================================================*/

/**
 * @brief Initialize CLI subsystem (display banner, set up line editing)
 */
void cli_init(void);

/**
 * @brief Register a command
 * @param cmd  Pointer to command descriptor (must be statically allocated)
 */
void cli_register_cmd(const cli_cmd_t *cmd);

/**
 * @brief Process CLI input — call periodically from a FreeRTOS task
 * Reads available characters from UART console and dispatches commands
 * when a complete line (terminated by \r or \n) is received.
 */
void cli_process(void);

/**
 * @brief Print CLI prompt
 */
void cli_prompt(void);

/**
 * @brief Print a formatted response line via the CLI
 */
void cli_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H_ */
