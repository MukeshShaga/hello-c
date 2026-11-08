/* mcp3008.h - driver for the Microchip MCP3008 8-channel 10-bit SPI ADC */
#ifndef MCP3008_H
#define MCP3008_H

#include "spi.h"

#include <stdint.h>

#define MCP3008_NUM_CHANNELS 8
#define MCP3008_MAX_RAW      1023

typedef struct {
    spi_dev_t *spi;
    float      vref; /* reference voltage in volts, e.g. 3.3 */
} mcp3008_t;

int   mcp3008_init(mcp3008_t *adc, const char *spi_path, float vref);
void  mcp3008_deinit(mcp3008_t *adc);
int   mcp3008_read_raw(mcp3008_t *adc, unsigned channel, uint16_t *raw);
int   mcp3008_read_all(mcp3008_t *adc, uint16_t raw[MCP3008_NUM_CHANNELS]);
float mcp3008_raw_to_volts(const mcp3008_t *adc, uint16_t raw);

#endif /* MCP3008_H */
