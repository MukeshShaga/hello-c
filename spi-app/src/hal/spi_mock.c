/*
 * spi_mock.c - SPI backend that routes transfers to simulated peripherals.
 *
 * Device path -> simulator mapping (mirrors a typical board wiring):
 *   /dev/spidev0.0 -> MCP3008 8-channel 10-bit ADC
 *   /dev/spidev0.1 -> W25Qxx SPI NOR flash
 */
#include "spi.h"
#include "spi_sim.h"
#include "log.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct spi_dev {
    const spi_sim_ops_t *sim;
    spi_config_t         cfg;
};

static const struct {
    const char          *suffix;
    const spi_sim_ops_t *ops;
} sim_map[] = {
    { "spidev0.0", &mcp3008_sim_ops },
    { "spidev0.1", &w25q_sim_ops },
};

const char *spi_backend_name(void) { return "mock"; }

void spi_mock_reset(void)
{
    for (size_t i = 0; i < sizeof(sim_map) / sizeof(sim_map[0]); i++)
        sim_map[i].ops->reset();
}

int spi_open(spi_dev_t **out, const char *path, const spi_config_t *cfg)
{
    if (!out || !path || !cfg)
        return -EINVAL;

    const spi_sim_ops_t *ops = NULL;
    for (size_t i = 0; i < sizeof(sim_map) / sizeof(sim_map[0]); i++) {
        if (strstr(path, sim_map[i].suffix)) {
            ops = sim_map[i].ops;
            break;
        }
    }
    if (!ops) {
        LOG_ERR("mock: no simulated device at %s", path);
        return -ENODEV;
    }

    spi_dev_t *dev = calloc(1, sizeof(*dev));
    if (!dev)
        return -ENOMEM;

    dev->sim = ops;
    dev->cfg = *cfg;
    LOG_INF("mock: opened %s -> %s (mode %u, %u Hz)", path, ops->name,
            cfg->mode, cfg->speed_hz);
    *out = dev;
    return 0;
}

void spi_close(spi_dev_t *dev)
{
    free(dev);
}

int spi_transfer_multi(spi_dev_t *dev, const spi_xfer_t *xfers, size_t count)
{
    if (!dev || !xfers || count == 0)
        return -EINVAL;

    dev->sim->cs_assert();
    for (size_t i = 0; i < count; i++) {
        const uint8_t *tx = xfers[i].tx;
        uint8_t       *rx = xfers[i].rx;

        for (size_t n = 0; n < xfers[i].len; n++) {
            uint8_t miso = dev->sim->xfer_byte(tx ? tx[n] : 0x00);
            if (rx)
                rx[n] = miso;
        }
        if (log_get_level() >= LOG_LEVEL_TRACE) {
            if (tx) log_hexdump("  MOSI", tx, xfers[i].len);
            if (rx) log_hexdump("  MISO", rx, xfers[i].len);
        }
    }
    dev->sim->cs_release();
    return 0;
}
