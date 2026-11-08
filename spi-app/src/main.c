/*
 * main.c - SPI demo application
 *
 *   1. Reads all 8 channels of an MCP3008 ADC          (SPI device A)
 *   2. Identifies a W25Qxx NOR flash via JEDEC ID      (SPI device B)
 *   3. Logs N samples to flash as CRC-checked records, then reads them back
 */
#include "datalogger.h"
#include "log.h"
#include "mcp3008.h"
#include "spi.h"
#include "w25q.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_ADC_DEV   "/dev/spidev0.0"
#define DEFAULT_FLASH_DEV "/dev/spidev0.1"
#define LOG_BASE_ADDR     0x000000u

typedef struct {
    const char *adc_dev;
    const char *flash_dev;
    unsigned    samples;
    float       vref;
} app_opts_t;

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  -a <dev>   ADC spidev node    (default %s)\n"
            "  -f <dev>   flash spidev node  (default %s)\n"
            "  -n <num>   samples to log     (default 5)\n"
            "  -r <volts> ADC reference      (default 3.3)\n"
            "  -v         verbose (repeat for SPI byte trace)\n"
            "  -h         this help\n",
            prog, DEFAULT_ADC_DEV, DEFAULT_FLASH_DEV);
}

static int parse_args(int argc, char **argv, app_opts_t *o)
{
    int opt, verbosity = 0;

    while ((opt = getopt(argc, argv, "a:f:n:r:vh")) != -1) {
        switch (opt) {
        case 'a': o->adc_dev   = optarg; break;
        case 'f': o->flash_dev = optarg; break;
        case 'n': o->samples   = (unsigned)strtoul(optarg, NULL, 0); break;
        case 'r': o->vref      = strtof(optarg, NULL); break;
        case 'v': verbosity++; break;
        case 'h': usage(argv[0]); exit(EXIT_SUCCESS);
        default:  usage(argv[0]); return -EINVAL;
        }
    }
    log_set_level(verbosity >= 2 ? LOG_LEVEL_TRACE
                : verbosity == 1 ? LOG_LEVEL_DEBUG
                                 : LOG_LEVEL_INFO);
    return 0;
}

static void print_channels(const mcp3008_t *adc, const uint16_t raw[])
{
    for (unsigned ch = 0; ch < MCP3008_NUM_CHANNELS; ch++)
        printf("  ch%u: %4u  (%.3f V)\n", ch, raw[ch],
               mcp3008_raw_to_volts(adc, raw[ch]));
}

static int run(const app_opts_t *o)
{
    mcp3008_t    adc;
    w25q_t       flash;
    datalogger_t dl;
    uint16_t     raw[MCP3008_NUM_CHANNELS];
    int          ret;

    if ((ret = mcp3008_init(&adc, o->adc_dev, o->vref)) < 0) {
        LOG_ERR("ADC init failed: %s", strerror(-ret));
        return ret;
    }
    if ((ret = w25q_init(&flash, o->flash_dev)) < 0) {
        LOG_ERR("flash init failed: %s", strerror(-ret));
        goto out_adc;
    }

    printf("== ADC snapshot ==\n");
    if ((ret = mcp3008_read_all(&adc, raw)) < 0)
        goto out_flash;
    print_channels(&adc, raw);

    printf("\n== Logging %u samples to flash @ 0x%06X ==\n",
           o->samples, LOG_BASE_ADDR);
    if ((ret = datalogger_init(&dl, &flash, LOG_BASE_ADDR)) < 0 ||
        (ret = datalogger_format(&dl)) < 0)
        goto out_flash;

    for (unsigned i = 0; i < o->samples; i++) {
        if ((ret = mcp3008_read_all(&adc, raw)) < 0 ||
            (ret = datalogger_append(&dl, raw)) < 0)
            goto out_flash;
    }

    printf("\n== Reading log back ==\n");
    for (unsigned i = 0; i < o->samples; i++) {
        dl_record_t rec;
        if ((ret = datalogger_read(&dl, i, &rec)) < 0) {
            LOG_ERR("record %u: %s", i, strerror(-ret));
            goto out_flash;
        }
        printf("  #%u crc=%04X ch0=%u ch7=%u\n",
               rec.seq, rec.crc, rec.raw[0], rec.raw[7]);
    }
    printf("\nAll %u records verified OK.\n", o->samples);

out_flash:
    w25q_deinit(&flash);
out_adc:
    mcp3008_deinit(&adc);
    return ret;
}

int main(int argc, char **argv)
{
    app_opts_t opts = {
        .adc_dev   = DEFAULT_ADC_DEV,
        .flash_dev = DEFAULT_FLASH_DEV,
        .samples   = 5,
        .vref      = 3.3f,
    };

    setvbuf(stdout, NULL, _IOLBF, 0); /* keep stdout/stderr ordered */

    if (parse_args(argc, argv, &opts) < 0)
        return EXIT_FAILURE;

    printf("spi-app (SPI backend: %s)\n\n", spi_backend_name());
    return run(&opts) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
