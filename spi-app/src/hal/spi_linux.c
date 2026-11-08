/*
 * spi_linux.c - SPI backend using the Linux spidev userspace driver.
 *
 * Requires a kernel with CONFIG_SPI_SPIDEV and a device node such as
 * /dev/spidev0.0 (on a Raspberry Pi: enable SPI with raspi-config or add
 * "dtparam=spi=on" to /boot/firmware/config.txt).
 */
#include "spi.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/spi/spidev.h>

#define SPI_MAX_SEGMENTS 8

struct spi_dev {
    int          fd;
    spi_config_t cfg;
};

const char *spi_backend_name(void) { return "linux-spidev"; }

int spi_open(spi_dev_t **out, const char *path, const spi_config_t *cfg)
{
    if (!out || !path || !cfg)
        return -EINVAL;

    spi_dev_t *dev = calloc(1, sizeof(*dev));
    if (!dev)
        return -ENOMEM;

    dev->cfg = *cfg;
    dev->fd  = open(path, O_RDWR);
    if (dev->fd < 0) {
        int err = -errno;
        LOG_ERR("open(%s) failed: %s", path, strerror(errno));
        free(dev);
        return err;
    }

    uint8_t  mode  = cfg->mode;
    uint8_t  bits  = cfg->bits_per_word;
    uint32_t speed = cfg->speed_hz;

    if (ioctl(dev->fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(dev->fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(dev->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        int err = -errno;
        LOG_ERR("configuring %s failed: %s", path, strerror(errno));
        close(dev->fd);
        free(dev);
        return err;
    }

    LOG_INF("opened %s (mode %u, %u bits, %u Hz)", path, mode, bits, speed);
    *out = dev;
    return 0;
}

void spi_close(spi_dev_t *dev)
{
    if (!dev)
        return;
    close(dev->fd);
    free(dev);
}

int spi_transfer_multi(spi_dev_t *dev, const spi_xfer_t *xfers, size_t count)
{
    if (!dev || !xfers || count == 0 || count > SPI_MAX_SEGMENTS)
        return -EINVAL;

    struct spi_ioc_transfer tr[SPI_MAX_SEGMENTS];
    memset(tr, 0, sizeof(tr));

    for (size_t i = 0; i < count; i++) {
        tr[i].tx_buf        = (unsigned long)xfers[i].tx;
        tr[i].rx_buf        = (unsigned long)xfers[i].rx;
        tr[i].len           = (uint32_t)xfers[i].len;
        tr[i].speed_hz      = dev->cfg.speed_hz;
        tr[i].bits_per_word = dev->cfg.bits_per_word;
        tr[i].cs_change     = 0; /* keep CS asserted between segments */
    }

    int ret = ioctl(dev->fd, SPI_IOC_MESSAGE(count), tr);
    if (ret < 0) {
        LOG_ERR("SPI_IOC_MESSAGE failed: %s", strerror(errno));
        return -errno;
    }
    return 0;
}
