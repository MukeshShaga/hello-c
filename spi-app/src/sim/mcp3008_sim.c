/*
 * mcp3008_sim.c - byte-level model of the Microchip MCP3008 ADC.
 *
 * Frame (3 bytes, MSB first):
 *   MOSI: 0x01 | SGL/DIFF D2 D1 D0 x x x x | xxxxxxxx
 *   MISO: ??   | ? ? ? ? ? 0 B9 B8         | B7..B0
 */
#include "spi_sim.h"

#include <string.h>

#define MCP3008_CHANNELS 8

/* Power-on values: a ramp across the channels, 0 .. 1022 */
static const uint16_t default_raw[MCP3008_CHANNELS] = {
    0, 146, 292, 438, 584, 730, 876, 1022,
};

static uint16_t channel_raw[MCP3008_CHANNELS] = {
    0, 146, 292, 438, 584, 730, 876, 1022,
};
static unsigned byte_idx;
static uint16_t sample;

void mcp3008_sim_set_channel(unsigned ch, uint16_t raw)
{
    if (ch < MCP3008_CHANNELS)
        channel_raw[ch] = raw & 0x3FF;
}

static void sim_reset(void)
{
    memcpy(channel_raw, default_raw, sizeof(channel_raw));
    byte_idx = 0;
}

static void sim_cs_assert(void)  { byte_idx = 0; sample = 0; }
static void sim_cs_release(void) { byte_idx = 0; }

static uint8_t sim_xfer_byte(uint8_t mosi)
{
    uint8_t miso = 0xFF;

    switch (byte_idx) {
    case 0: /* start bit; DOUT is high-Z */
        break;
    case 1: {
        unsigned ch = (mosi >> 4) & 0x07; /* single-ended only in this model */
        sample = channel_raw[ch];
        miso = (uint8_t)((sample >> 8) & 0x03); /* null bit + B9..B8 */
        break;
    }
    case 2:
        miso = (uint8_t)(sample & 0xFF);
        break;
    default:
        break;
    }
    byte_idx++;
    return miso;
}

const spi_sim_ops_t mcp3008_sim_ops = {
    .name       = "MCP3008 (simulated)",
    .cs_assert  = sim_cs_assert,
    .xfer_byte  = sim_xfer_byte,
    .cs_release = sim_cs_release,
    .reset      = sim_reset,
};
