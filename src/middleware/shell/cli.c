/**
 * @file    cli.c
 * @brief   CLI shell implementation — line editing, history, command dispatch
 */

#include "cli.h"
#include "uart_console.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/*===========================================================================
 * Constants
 *===========================================================================*/
#define CLI_MAX_CMDS      16
#define CLI_LINE_BUF_SIZE 128
#define CLI_PROMPT        "gd32> "
#define CLI_BANNER        \
    "\r\n======================================\r\n" \
    "  GD32F450 FreeRTOS Project\r\n" \
    "  Type 'help' for available commands\r\n" \
    "======================================\r\n"

/*===========================================================================
 * Static State
 *===========================================================================*/
static const cli_cmd_t *cli_cmds[CLI_MAX_CMDS];
static uint8_t           cli_cmd_count = 0;

static char   cli_line_buf[CLI_LINE_BUF_SIZE];
static int    cli_line_pos = 0;

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/
static void cli_execute(char *line);

/*===========================================================================
 * Public API
 *===========================================================================*/
void cli_init(void)
{
    cli_cmd_count = 0;
    cli_line_pos  = 0;
    memset(cli_line_buf, 0, sizeof(cli_line_buf));

    uart_console_send_str(CLI_BANNER);
    cli_prompt();
}

void cli_register_cmd(const cli_cmd_t *cmd)
{
    if (cmd == NULL || cmd->name == NULL || cmd->handler == NULL)
    {
        return;
    }

    if (cli_cmd_count < CLI_MAX_CMDS)
    {
        cli_cmds[cli_cmd_count++] = cmd;
    }
}

void cli_process(void)
{
    uint8_t ch;

    while (uart_console_recv_byte(&ch))
    {
        /* Handle backspace (0x08 or 0x7F) */
        if (ch == 0x08 || ch == 0x7F)
        {
            if (cli_line_pos > 0)
            {
                cli_line_pos--;
                cli_line_buf[cli_line_pos] = '\0';
                /* Echo backspace sequence */
                uart_console_send_str("\b \b");
            }
            continue;
        }

        /* Handle Enter (CR or LF) */
        if (ch == '\r' || ch == '\n')
        {
            uart_console_send_str("\r\n");

            if (cli_line_pos > 0)
            {
                cli_line_buf[cli_line_pos] = '\0';
                cli_execute(cli_line_buf);
                cli_line_pos = 0;
                memset(cli_line_buf, 0, sizeof(cli_line_buf));
            }

            cli_prompt();
            continue;
        }

        /* Echo printable characters and store */
        if (ch >= 0x20 && ch < 0x7F && cli_line_pos < (CLI_LINE_BUF_SIZE - 1))
        {
            cli_line_buf[cli_line_pos++] = (char)ch;
            uart_console_send(&ch, 1);
        }
    }
}

void cli_prompt(void)
{
    uart_console_send_str(CLI_PROMPT);
}

void cli_printf(const char *fmt, ...)
{
    char buf[CLI_LINE_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    uart_console_send_str(buf);
}

/*===========================================================================
 * Static: Parse and execute a command line
 *===========================================================================*/
static void cli_execute(char *line)
{
    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t')
    {
        line++;
    }

    if (*line == '\0')
    {
        return;
    }

    /* Tokenize: split into argv[] */
    int    argc = 0;
    char  *argv[8];
    char  *token = line;
    bool   in_token = false;

    while (*line && argc < 8)
    {
        if (*line == ' ' || *line == '\t')
        {
            if (in_token)
            {
                *line   = '\0';
                in_token = false;
            }
        }
        else
        {
            if (!in_token)
            {
                argv[argc++] = line;
                in_token = true;
            }
        }
        line++;
    }

    if (argc == 0)
    {
        return;
    }

    /* Look up the command */
    for (uint8_t i = 0; i < cli_cmd_count; i++)
    {
        if (strcmp(argv[0], cli_cmds[i]->name) == 0)
        {
            cli_cmds[i]->handler(argc, argv);
            return;
        }
    }

    cli_printf("Unknown command: %s (try 'help')\r\n", argv[0]);
}
