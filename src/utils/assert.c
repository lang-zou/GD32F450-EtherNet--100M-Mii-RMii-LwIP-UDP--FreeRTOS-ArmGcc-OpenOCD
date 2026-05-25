/**
 * @file    assert.c
 * @brief   Assertion handler — logs the failure and halts the system
 */

#include "assert.h"
#include "log.h"

/*===========================================================================
 * Assert Failed Handler
 * Logs the file, line, and expression that failed, then enters an infinite
 * loop with interrupts disabled. A debugger can inspect the state at the
 * breakpoint location.
 *===========================================================================*/
void assert_failed(const char *file, int line, const char *expr)
{
    LOG_E("ASSERT FAILED: %s", expr);
    LOG_E("  Location: %s:%d", file, line);

    /* Disable interrupts and halt */
    __disable_irq();

    /* If debugger is attached, this is a good breakpoint location */
    while (1)
    {
        __asm volatile ("nop");
    }
}
