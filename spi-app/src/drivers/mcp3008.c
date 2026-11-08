/* mcp3008.c - MCP3008 ADC driver (single-ended conversions) */
#include "mcp3008.h"
#include "log.h"

#include <errno.h>
#include <string.h>

int mcp3008_init(mcp3008_t *adc, const char *spi_path, float vref)
{
    if (!adc || !spi_path)
        return -EINVAL;

    memset(adc, 0, sizeof(*adc));
    adc->vref = vref;

    /* MCP3008 supports modes 0 and 3; 1 MHz is safe at 2.7-5 V */
    const spi_config_t cfg = {
        .mode          = SPI_HAL_MODE_0,
        .bits_per_word = 8,
        .speed_hz      = 1000000,
    };
    return spi_open(&adc->spi, spi_path, &cfg);
}

void mcp3008_deinit(mcp3008_t *adc)
{
    if (adc && adc->spi) {
        spi_close(adc->spi);
        adc->spi = NULL;
    }
}

int mcp3008_read_raw(mcp3008_t *adc, unsigned channel, uint16_t *raw)
{
    if (!adc || !adc->spi || !raw || channel >= MCP3008_NUM_CHANNELS)
        return -EINVAL;

    /* start bit, then SGL=1 + 3-bit channel, then 8 clocks for data */
    const uint8_t tx[3] = { 0x01, (uint8_t)(0x80 | (channel << 4)), 0x00 };
    uint8_t rx[3] = { 0 };

    int ret = spi_transfer(adc->spi, tx, rx, sizeof(tx));
    if (ret < 0)
        return ret;

    *raw = (uint16_t)(((rx[1] & 0x03) << 8) | rx[2]);
    LOG_DBG("mcp3008 ch%u raw=%u", channel, *raw);
    return 0;
}

int mcp3008_read_all(mcp3008_t *adc, uint16_t raw[MCP3008_NUM_CHANNELS])
{
    for (unsigned ch = 0; ch < MCP3008_NUM_CHANNELS; ch++) {
        int ret = mcp3008_read_raw(adc, ch, &raw[ch]);
        if (ret < 0)
            return ret;
    }
    return 0;
}

float mcp3008_raw_to_volts(const mcp3008_t *adc, uint16_t raw)
{
    return (float)raw * adc->vref / (float)(MCP3008_MAX_RAW + 1);
}
