/*
 * spi_sim.h - interface for simulated SPI peripherals used by the mock backend.
 *
 * A simulator sees the bus exactly like a real chip does: chip-select goes
 * low, bytes are shifted in/out one at a time, chip-select goes high.
 */
#ifndef SPI_SIM_H
#define SPI_SIM_H

#include <stdint.h>

typedef struct {
    const char *name;
    void    (*cs_assert)(void);
    uint8_t (*xfer_byte)(uint8_t mosi); /* returns MISO */
    void    (*cs_release)(void);
    void    (*reset)(void);
} spi_sim_ops_t;

extern const spi_sim_ops_t mcp3008_sim_ops;
extern const spi_sim_ops_t w25q_sim_ops;

/* Simulated ADC: set the raw 10-bit value returned for a channel. */
void mcp3008_sim_set_channel(unsigned ch, uint16_t raw);

/* Reset every simulator to power-on state (flash is erased to 0xFF). */
void spi_mock_reset(void);

#endif /* SPI_SIM_H */
