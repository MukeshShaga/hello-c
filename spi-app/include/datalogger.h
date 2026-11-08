/*
 * datalogger.h - stores ADC samples as CRC-protected records in SPI flash.
 *
 * Layout: records are packed back-to-back starting at `base_addr` inside
 * a single erased 4 KiB sector. An erased (0xFF) record marks the end.
 */
#ifndef DATALOGGER_H
#define DATALOGGER_H

#include "mcp3008.h"
#include "w25q.h"

#include <stdint.h>

typedef struct {
    uint32_t seq;
    uint16_t raw[MCP3008_NUM_CHANNELS];
    uint16_t crc; /* CRC-16 over seq + raw */
} __attribute__((packed)) dl_record_t;

typedef struct {
    w25q_t  *flash;
    uint32_t base_addr;
    uint32_t next_seq;
    uint32_t capacity; /* max records in the sector */
} datalogger_t;

int datalogger_init(datalogger_t *dl, w25q_t *flash, uint32_t base_addr);
int datalogger_format(datalogger_t *dl); /* erase the log sector */
int datalogger_append(datalogger_t *dl, const uint16_t raw[MCP3008_NUM_CHANNELS]);

/* Read record `index`; returns -ENOENT past the end, -EBADMSG on bad CRC. */
int datalogger_read(datalogger_t *dl, uint32_t index, dl_record_t *out);

#endif /* DATALOGGER_H */
