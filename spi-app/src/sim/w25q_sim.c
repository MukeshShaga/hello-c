/*
 * w25q_sim.c - byte-level model of a Winbond W25Q80 (1 MiB) SPI NOR flash.
 *
 * Supported commands: 0x9F JEDEC ID, 0x05 read status, 0x06/0x04 write
 * enable/disable, 0x03 read, 0x02 page program, 0x20 4 KiB sector erase.
 * Like real NOR flash, programming can only clear bits (1 -> 0), page
 * programs wrap within a 256-byte page, and WEL is cleared after each
 * program/erase.
 */
#include "spi_sim.h"

#include <stdbool.h>
#include <string.h>

#define FLASH_SIZE   (1024u * 1024u)
#define PAGE_SIZE    256u
#define SECTOR_SIZE  4096u

#define SR_BUSY 0x01
#define SR_WEL  0x02

static uint8_t  mem[FLASH_SIZE];
static bool     mem_ready;   /* lazily erased to 0xFF on first use */
static bool     wel;         /* write-enable latch */
static unsigned byte_idx;
static uint8_t  cmd;
static uint32_t addr;
static uint32_t prog_offset;

static const uint8_t jedec_id[3] = { 0xEF, 0x40, 0x14 }; /* Winbond W25Q80 */

static void ensure_ready(void)
{
    if (!mem_ready) {
        memset(mem, 0xFF, sizeof(mem));
        mem_ready = true;
    }
}

static void sim_reset(void)
{
    memset(mem, 0xFF, sizeof(mem));
    mem_ready = true;
    wel = false;
    byte_idx = 0;
}

static void sim_cs_assert(void)
{
    ensure_ready();
    byte_idx    = 0;
    cmd         = 0;
    addr        = 0;
    prog_offset = 0;
}

static uint8_t sim_xfer_byte(uint8_t mosi)
{
    uint8_t miso = 0xFF;

    if (byte_idx == 0) {
        cmd = mosi;
    } else {
        switch (cmd) {
        case 0x9F: /* JEDEC ID */
            if (byte_idx <= 3)
                miso = jedec_id[byte_idx - 1];
            break;
        case 0x05: /* read status register 1 (repeats) */
            miso = wel ? SR_WEL : 0;
            break;
        case 0x03: /* read data */
        case 0x02: /* page program */
        case 0x20: /* sector erase */
            if (byte_idx <= 3) {
                addr = ((addr << 8) | mosi) & (FLASH_SIZE - 1);
            } else if (cmd == 0x03) {
                miso = mem[addr];
                addr = (addr + 1) & (FLASH_SIZE - 1);
            } else if (cmd == 0x02 && wel) {
                uint32_t page = addr & ~(PAGE_SIZE - 1);
                uint32_t off  = (addr + prog_offset) & (PAGE_SIZE - 1);
                mem[page + off] &= mosi; /* NOR: can only clear bits */
                prog_offset++;
            }
            break;
        default:
            break;
        }
    }
    byte_idx++;
    return miso;
}

static void sim_cs_release(void)
{
    switch (cmd) {
    case 0x06:
        wel = true;
        break;
    case 0x04:
        wel = false;
        break;
    case 0x02:
        wel = false;
        break;
    case 0x20:
        if (wel && byte_idx >= 4) {
            uint32_t base = addr & ~(SECTOR_SIZE - 1);
            memset(&mem[base], 0xFF, SECTOR_SIZE);
        }
        wel = false;
        break;
    default:
        break;
    }
    byte_idx = 0;
}

const spi_sim_ops_t w25q_sim_ops = {
    .name       = "W25Q80 flash (simulated)",
    .cs_assert  = sim_cs_assert,
    .xfer_byte  = sim_xfer_byte,
    .cs_release = sim_cs_release,
    .reset      = sim_reset,
};
