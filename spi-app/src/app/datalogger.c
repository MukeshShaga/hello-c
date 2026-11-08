/* datalogger.c - append-only sample log on SPI NOR flash */
#include "datalogger.h"
#include "crc16.h"
#include "log.h"

#include <errno.h>
#include <string.h>

static uint32_t record_addr(const datalogger_t *dl, uint32_t index)
{
    return dl->base_addr + index * (uint32_t)sizeof(dl_record_t);
}

static int is_erased(const dl_record_t *r)
{
    const uint8_t *p = (const uint8_t *)r;
    for (size_t i = 0; i < sizeof(*r); i++)
        if (p[i] != 0xFF)
            return 0;
    return 1;
}

int datalogger_init(datalogger_t *dl, w25q_t *flash, uint32_t base_addr)
{
    if (!dl || !flash || base_addr % W25Q_SECTOR_SIZE)
        return -EINVAL;

    dl->flash     = flash;
    dl->base_addr = base_addr;
    dl->capacity  = W25Q_SECTOR_SIZE / sizeof(dl_record_t);
    dl->next_seq  = 0;

    /* scan for the first free slot so we resume an existing log */
    dl_record_t r;
    for (uint32_t i = 0; i < dl->capacity; i++) {
        int ret = w25q_read(flash, record_addr(dl, i), &r, sizeof(r));
        if (ret < 0)
            return ret;
        if (is_erased(&r))
            break;
        dl->next_seq = i + 1;
    }
    LOG_INF("datalogger: %u/%u records in use", dl->next_seq, dl->capacity);
    return 0;
}

int datalogger_format(datalogger_t *dl)
{
    int ret = w25q_erase_sector(dl->flash, dl->base_addr);
    if (ret == 0)
        dl->next_seq = 0;
    return ret;
}

int datalogger_append(datalogger_t *dl, const uint16_t raw[MCP3008_NUM_CHANNELS])
{
    if (dl->next_seq >= dl->capacity)
        return -ENOSPC;

    dl_record_t r;
    r.seq = dl->next_seq;
    memcpy(r.raw, raw, sizeof(r.raw));
    r.crc = crc16_ccitt(&r, offsetof(dl_record_t, crc));

    int ret = w25q_write(dl->flash, record_addr(dl, dl->next_seq), &r, sizeof(r));
    if (ret < 0)
        return ret;

    dl->next_seq++;
    return 0;
}

int datalogger_read(datalogger_t *dl, uint32_t index, dl_record_t *out)
{
    if (index >= dl->next_seq)
        return -ENOENT;

    int ret = w25q_read(dl->flash, record_addr(dl, index), out, sizeof(*out));
    if (ret < 0)
        return ret;

    uint16_t crc = crc16_ccitt(out, offsetof(dl_record_t, crc));
    if (crc != out->crc || out->seq != index) {
        LOG_WRN("datalogger: record %u corrupt (crc %04X != %04X)",
                index, crc, out->crc);
        return -EBADMSG;
    }
    return 0;
}
