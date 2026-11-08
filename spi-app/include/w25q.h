/* w25q.h - driver for Winbond W25Qxx SPI NOR flash */
#ifndef W25Q_H
#define W25Q_H

#include "spi.h"

#include <stddef.h>
#include <stdint.h>

#define W25Q_PAGE_SIZE   256u
#define W25Q_SECTOR_SIZE 4096u

typedef struct {
    spi_dev_t *spi;
    uint32_t   jedec_id;   /* manufacturer << 16 | type << 8 | capacity */
    uint32_t   size_bytes; /* derived from the capacity byte */
} w25q_t;

int  w25q_init(w25q_t *flash, const char *spi_path);
void w25q_deinit(w25q_t *flash);
int  w25q_read(w25q_t *flash, uint32_t addr, void *buf, size_t len);
/* Writes any length; splits on page boundaries. Target must be erased. */
int  w25q_write(w25q_t *flash, uint32_t addr, const void *buf, size_t len);
int  w25q_erase_sector(w25q_t *flash, uint32_t addr);

#endif /* W25Q_H */
