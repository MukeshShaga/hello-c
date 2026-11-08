/*
 * spi.h - SPI hardware abstraction layer (HAL)
 *
 * Two backends implement this interface:
 *   - src/hal/spi_linux.c : real hardware via the Linux spidev driver (/dev/spidevB.C)
 *   - src/hal/spi_mock.c  : simulated devices, so the app runs/debugs on any host
 *
 * All functions return 0 on success or a negative errno value on failure.
 */
#ifndef SPI_H
#define SPI_H

#include <stddef.h>
#include <stdint.h>

/* SPI clock polarity / phase modes (same values as linux/spi/spidev.h) */
#define SPI_HAL_MODE_0 0x00 /* CPOL=0, CPHA=0 */
#define SPI_HAL_MODE_1 0x01 /* CPOL=0, CPHA=1 */
#define SPI_HAL_MODE_2 0x02 /* CPOL=1, CPHA=0 */
#define SPI_HAL_MODE_3 0x03 /* CPOL=1, CPHA=1 */

typedef struct spi_dev spi_dev_t; /* opaque, defined by the backend */

typedef struct {
    uint8_t  mode;          /* SPI_HAL_MODE_x */
    uint8_t  bits_per_word; /* usually 8 */
    uint32_t speed_hz;      /* max clock speed */
} spi_config_t;

/*
 * One segment of a transaction. Chip-select stays asserted across all
 * segments passed to spi_transfer_multi().
 *   tx == NULL -> clock out zeros
 *   rx == NULL -> discard received bytes
 */
typedef struct {
    const void *tx;
    void       *rx;
    size_t      len;
} spi_xfer_t;

int  spi_open(spi_dev_t **out, const char *path, const spi_config_t *cfg);
void spi_close(spi_dev_t *dev);

/* Full-duplex transfer of len bytes in a single CS assertion. */
int spi_transfer(spi_dev_t *dev, const void *tx, void *rx, size_t len);

/* Several segments in a single CS assertion (e.g. command, then data). */
int spi_transfer_multi(spi_dev_t *dev, const spi_xfer_t *xfers, size_t count);

/* Name of the compiled-in backend ("linux-spidev" or "mock"). */
const char *spi_backend_name(void);

#endif /* SPI_H */
