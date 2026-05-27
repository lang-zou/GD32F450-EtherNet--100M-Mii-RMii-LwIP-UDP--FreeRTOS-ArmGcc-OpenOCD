/**
 * @file    board.c
 * @brief   Board-level initialization — clock tree, peripheral clocks, delay
 */

#include "board.h"

/* SystemCoreClock variable updated by system_gd32f4xx.c */
extern uint32_t SystemCoreClock;

/*===========================================================================
 * Clock Initialization
 * HSE (25 MHz) → PLL → 200 MHz SYSCLK
 * Configuration:
 *   HSE = 25 MHz
 *   PLL = HSE / pll_m * pll_n / pll_p = 25 / 25 * 400 / 2 = 200 MHz
 *   AHB  = SYSCLK / 1 = 200 MHz
 *   APB1 = AHB / 4   = 50 MHz  (must not exceed 50 MHz)
 *   APB2 = AHB / 2   = 100 MHz (must not exceed 100 MHz)
 *
 * This board_clock_init() supplements SystemInit() with the full PLL config.
 * SystemInit() is called from startup before main() and sets the default
 * HSI clock; this function reconfigures to HSE + PLL for 200 MHz operation.
 *===========================================================================*/
void board_clock_init(void)
{
    /* Enable HXTAL and wait for it to be ready */
    rcu_osci_on(RCU_HXTAL);
    while (SUCCESS != rcu_osci_stab_wait(RCU_HXTAL))
    {
        /* Wait for HSE stabilization */
    }

    /* Configure PLL */
    rcu_pll_config(RCU_PLLSRC_HXTAL,  /* PLL source = HXTAL (25 MHz) */
                   25,               /* PLLM = 25  → VCO input = 1 MHz */
                   400,              /* PLLN = 400 → VCO = 400 MHz */
                   2,                /* PLLP = 2   → SYSCLK = VCO / 2 = 200 MHz */
                   8);               /* PLLQ = 8   → USB/ETH clocks */

    /* Enable PLL */
    rcu_osci_on(RCU_PLL);
    while (SUCCESS != rcu_osci_stab_wait(RCU_PLL))
    {
        /* Wait for PLL stabilization */
    }

    /* Configure AHB, APB1, APB2 prescalers
     * AHB  = SYSCLK / 1 = 200 MHz
     * APB1 = AHB / 4    = 50 MHz
     * APB2 = AHB / 2    = 100 MHz
     */
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV4);
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV2);

    /* Configure Flash wait states for 200 MHz
     * VDD = 3.3V: 5 WS for 200-240 MHz range on GD32F4xx
     */
    fmc_unlock();
    fmc_wscnt_set(WS_WSCNT_5);
    fmc_lock();

    /* Switch system clock to PLL */
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLLP);

    /* Update SystemCoreClock variable */
    SystemCoreClock = SYSTEM_CLOCK;

    /* Enable DWT cycle counter once for all delay functions */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/*===========================================================================
 * Peripheral Clock Initialization
 * Enable clocks for all required peripherals. Called once at startup.
 *===========================================================================*/
void board_periph_init(void)
{
    /* GPIO clocks */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_GPIOF);
    rcu_periph_clock_enable(RCU_GPIOG);
    rcu_periph_clock_enable(RCU_GPIOH);
    rcu_periph_clock_enable(RCU_GPIOI);

    /* Essential peripheral clocks */
    rcu_periph_clock_enable(RCU_SYSCFG);
    rcu_periph_clock_enable(RCU_PMU);

}

/*===========================================================================
 * Delay Functions (blocking, for boot phase before RTOS starts)
 * Uses DWT cycle counter for microsecond resolution
 *===========================================================================*/
void board_delay_us(uint32_t us)
{
    if (0U == us) return;

    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    uint32_t start = DWT->CYCCNT;

    while ((DWT->CYCCNT - start) < ticks)
    {
        /* Busy-wait */
    }
}

void board_delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        board_delay_us(1000);
    }
}
