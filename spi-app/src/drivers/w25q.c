/* w25q.c - Winbond W25Qxx SPI NOR flash driver */
#include "w25q.h"
#include "log.h"

#include <errno.h>
#include <string.h>
#include <time.h>

#define CMD_WRITE_ENABLE  0x06
#define CMD_READ_STATUS1  0x05
#define CMD_READ_DATA     0x03
#define CMD_PAGE_PROGRAM  0x02
#define CMD_SECTOR_ERASE  0x20
#define CMD_JEDEC_ID      0x9F

#define SR1_BUSY 0x01

#define WINBOND_MFR_ID 0xEF

/* Datasheet worst cases: page program 3 ms, sector erase 400 ms */
#define TIMEOUT_PROGRAM_MS 10
#define TIMEOUT_ERASE_MS   500

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static void fill_cmd_addr(uint8_t out[4], uint8_t cmd, uint32_t addr)
{
    out[0] = cmd;
    out[1] = (uint8_t)(addr >> 16);
    out[2] = (uint8_t)(addr >> 8);
    out[3] = (uint8_t)addr;
}

static int write_enable(w25q_t *flash)
{
    const uint8_t cmd = CMD_WRITE_ENABLE;
    return spi_transfer(flash->spi, &cmd, NULL, 1);
}

static int wait_ready(w25q_t *flash, unsigned timeout_ms)
{
    const uint64_t deadline = now_ms() + timeout_ms;

    for (;;) {
        uint8_t tx[2] = { CMD_READ_STATUS1, 0x00 };
        uint8_t rx[2];
        int ret = spi_transfer(flash->spi, tx, rx, sizeof(tx));
        if (ret < 0)
            return ret;
        if (!(rx[1] & SR1_BUSY))
            return 0;
        if (now_ms() > deadline) {
            LOG_ERR("w25q: timeout waiting for BUSY to clear");
            return -ETIMEDOUT;
        }
        struct timespec pause = { 0, 100000 }; /* 100 us */
        nanosleep(&pause, NULL);
    }
}

int w25q_init(w25q_t *flash, const char *spi_path)
{
    if (!flash || !spi_path)
        return -EINVAL;

    memset(flash, 0, sizeof(*flash));

    const spi_config_t cfg = {
        .mode          = SPI_HAL_MODE_0,
        .bits_per_word = 8,
        .speed_hz      = 10000000,
    };
    int ret = spi_open(&flash->spi, spi_path, &cfg);
    if (ret < 0)
        return ret;

    uint8_t tx[4] = { CMD_JEDEC_ID, 0, 0, 0 };
    uint8_t rx[4] = { 0 };
    ret = spi_transfer(flash->spi, tx, rx, sizeof(tx));
    if (ret < 0)
        goto fail;

    flash->jedec_id = ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
    if (rx[1] != WINBOND_MFR_ID || rx[3] < 0x10 || rx[3] > 0x20) {
        LOG_ERR("w25q: unexpected JEDEC ID 0x%06X (check wiring/CS)",
                flash->jedec_id);
        ret = -ENODEV;
        goto fail;
    }
    flash->size_bytes = 1u << rx[3]; /* capacity byte = log2(size) */

    LOG_INF("w25q: JEDEC ID 0x%06X, %u KiB", flash->jedec_id,
            flash->size_bytes / 1024);
    return 0;

fail:
    spi_close(flash->spi);
    flash->spi = NULL;
    return ret;
}

void w25q_deinit(w25q_t *flash)
{
    if (flash && flash->spi) {
        spi_close(flash->spi);
        flash->spi = NULL;
    }
}

static int check_range(const w25q_t *flash, uint32_t addr, size_t len)
{
    if (!flash || !flash->spi)
        return -EINVAL;
    if (addr >= flash->size_bytes || len > flash->size_bytes - addr)
        return -ERANGE;
    return 0;
}

int w25q_read(w25q_t *flash, uint32_t addr, void *buf, size_t len)
{
    int ret = check_range(flash, addr, len);
    if (ret < 0 || len == 0)
        return ret;

    uint8_t hdr[4];
    fill_cmd_addr(hdr, CMD_READ_DATA, addr);

    const spi_xfer_t xfers[2] = {
        { .tx = hdr,  .rx = NULL, .len = sizeof(hdr) },
        { .tx = NULL, .rx = buf,  .len = len },
    };
    return spi_transfer_multi(flash->spi, xfers, 2);
}

int w25q_write(w25q_t *flash, uint32_t addr, const void *buf, size_t len)
{
    int ret = check_range(flash, addr, len);
    if (ret < 0)
        return ret;

    const uint8_t *src = buf;
    while (len > 0) {
        /* never cross a page boundary in one program command */
        size_t room  = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
        size_t chunk = len < room ? len : room;

        if ((ret = write_enable(flash)) < 0)
            return ret;

        uint8_t hdr[4];
        fill_cmd_addr(hdr, CMD_PAGE_PROGRAM, addr);
        const spi_xfer_t xfers[2] = {
            { .tx = hdr, .rx = NULL, .len = sizeof(hdr) },
            { .tx = src, .rx = NULL, .len = chunk },
        };
        if ((ret = spi_transfer_multi(flash->spi, xfers, 2)) < 0)
            return ret;
        if ((ret = wait_ready(flash, TIMEOUT_PROGRAM_MS)) < 0)
            return ret;

        LOG_DBG("w25q: programmed %zu bytes @ 0x%06X", chunk, addr);
        addr += (uint32_t)chunk;
        src  += chunk;
        len  -= chunk;
    }
    return 0;
}

int w25q_erase_sector(w25q_t *flash, uint32_t addr)
{
    int ret = check_range(flash, addr, 1);
    if (ret < 0)
        return ret;

    addr &= ~(W25Q_SECTOR_SIZE - 1);
    if ((ret = write_enable(flash)) < 0)
        return ret;

    uint8_t cmd[4];
    fill_cmd_addr(cmd, CMD_SECTOR_ERASE, addr);
    if ((ret = spi_transfer(flash->spi, cmd, NULL, sizeof(cmd))) < 0)
        return ret;

    LOG_DBG("w25q: erased sector @ 0x%06X", addr);
    return wait_ready(flash, TIMEOUT_ERASE_MS);
}
