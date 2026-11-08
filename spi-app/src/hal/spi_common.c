/* spi_common.c - backend-independent helpers built on spi_transfer_multi() */
#include "spi.h"

#include <errno.h>

int spi_transfer(spi_dev_t *dev, const void *tx, void *rx, size_t len)
{
    if (!dev || len == 0)
        return -EINVAL;

    const spi_xfer_t x = { .tx = tx, .rx = rx, .len = len };
    return spi_transfer_multi(dev, &x, 1);
}
