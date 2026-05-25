/**
 * @file    flash_if.c
 * @brief   Internal Flash operations for OTA firmware updates
 *
 * Handles Flash unlock/lock, sector erase, and word-aligned programming.
 * All operations check address boundaries against flash partitions.
 */

#include "flash_if.h"
#include "app_config.h"
#include "gd32f4xx_fmc.h"
#include "log.h"

/* Flash base address for sector calculations */
#define FLASH_BASE_ADDR  0x08000000UL

/* Sector numbers for GD32F450 (1 MB range) */
#define SECTOR_16K_MIN   0
#define SECTOR_16K_MAX   3
#define SECTOR_64K       4
#define SECTOR_128K_MIN  5
#define SECTOR_128K_MAX  11

/*===========================================================================
 * Helper: Convert address to sector number
 *===========================================================================*/
static int32_t flash_addr_to_sector(uint32_t addr)
{
    uint32_t offset = addr - FLASH_BASE_ADDR;

    if (offset < 0x10000UL)
    {
        /* First 64 KB: 4 sectors of 16 KB */
        return SECTOR_16K_MIN + (int32_t)(offset / 0x4000UL);
    }
    else if (offset < 0x20000UL)
    {
        /* Next 64 KB: 1 sector of 64 KB */
        return SECTOR_64K;
    }
    else if (offset < 0x100000UL)
    {
        /* Remaining: 128 KB sectors */
        uint32_t base = offset - 0x20000UL;
        return SECTOR_128K_MIN + (int32_t)(base / 0x20000UL);
    }

    return -1; /* Out of range */
}

/*===========================================================================
 * Flash Unlock / Lock
 *===========================================================================*/
bool flash_unlock(void)
{
    fmc_unlock();
    return true;
}

bool flash_lock(void)
{
    fmc_lock();
    return true;
}

/*===========================================================================
 * Sector Erase
 *===========================================================================*/
bool flash_erase_sector(uint32_t addr)
{
    int32_t sector = flash_addr_to_sector(addr);
    if (sector < 0)
    {
        LOG_E("flash_erase_sector: invalid address 0x%08lX", addr);
        return false;
    }

    flash_unlock();

    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
                   FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);

    if (fmc_sector_erase((uint32_t)sector) != FMC_READY)
    {
        LOG_E("flash_erase_sector: erase failed for sector %ld", sector);
        flash_lock();
        return false;
    }

    flash_lock();
    return true;
}

/*===========================================================================
 * Range Erase
 *===========================================================================*/
bool flash_erase_range(uint32_t start_addr, uint32_t size)
{
    uint32_t end_addr = start_addr + size;

    LOG_I("flash_erase_range: 0x%08lX - 0x%08lX (%lu bytes)",
          start_addr, end_addr, size);

    for (uint32_t addr = start_addr; addr < end_addr; )
    {
        if (!flash_erase_sector(addr))
        {
            return false;
        }

        int32_t sector = flash_addr_to_sector(addr);
        if (sector < 0)
        {
            break;
        }

        /* Advance by sector size */
        if (sector <= SECTOR_16K_MAX)
        {
            addr += 0x4000UL;   /* 16 KB */
        }
        else if (sector == SECTOR_64K)
        {
            addr += 0x10000UL;  /* 64 KB */
        }
        else
        {
            addr += 0x20000UL;  /* 128 KB */
        }
    }

    return true;
}

/*===========================================================================
 * Word Write
 *===========================================================================*/
bool flash_write_word(uint32_t addr, uint32_t data)
{
    flash_unlock();

    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
                   FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);

    if (fmc_word_program(addr, data) != FMC_READY)
    {
        LOG_E("flash_write_word: write failed at 0x%08lX", addr);
        flash_lock();
        return false;
    }

    flash_lock();
    return true;
}

/*===========================================================================
 * Buffer Write
 * Handles alignment: writes 32-bit words, handles remainder bytes
 *===========================================================================*/
bool flash_write_buf(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (len == 0)
    {
        return true;
    }

    uint32_t word_count = len / 4;
    uint32_t remainder  = len % 4;

    /* Write word-aligned data */
    for (uint32_t i = 0; i < word_count; i++)
    {
        uint32_t word = ((uint32_t)data[i * 4 + 0] << 0)  |
                        ((uint32_t)data[i * 4 + 1] << 8)  |
                        ((uint32_t)data[i * 4 + 2] << 16) |
                        ((uint32_t)data[i * 4 + 3] << 24);

        if (!flash_write_word(addr + i * 4, word))
        {
            return false;
        }
    }

    /* Write remaining bytes (padded with 0xFF) */
    if (remainder > 0)
    {
        uint32_t word = 0xFFFFFFFF;
        for (uint32_t j = 0; j < remainder; j++)
        {
            ((uint8_t *)&word)[j] = data[word_count * 4 + j];
        }

        if (!flash_write_word(addr + word_count * 4, word))
        {
            return false;
        }
    }

    return true;
}

/*===========================================================================
 * Memory-mapped Read
 * Flash is memory-mapped, so reading is a simple memcpy
 *===========================================================================*/
void flash_read_buf(uint32_t addr, uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = *((volatile uint8_t *)(addr + i));
    }
}
