/*
 * test_main.c - unit tests, always linked against the mock SPI backend.
 * Run with: make test
 */
#include "crc16.h"
#include "datalogger.h"
#include "log.h"
#include "mcp3008.h"
#include "spi_sim.h"
#include "w25q.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int tests_run, tests_failed;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "    FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
                    #cond);                                                \
            return 1;                                                      \
        }                                                                  \
    } while (0)

#define RUN(fn)                                                            \
    do {                                                                   \
        spi_mock_reset();                                                  \
        tests_run++;                                                       \
        int failed_ = fn();                                                \
        tests_failed += failed_;                                           \
        printf("  %-4s %s\n", failed_ ? "FAIL" : "ok", #fn);               \
    } while (0)

static int test_crc16_known_vector(void)
{
    CHECK(crc16_ccitt("123456789", 9) == 0x29B1);
    return 0;
}

static int test_mcp3008_reads_each_channel(void)
{
    mcp3008_t adc;
    CHECK(mcp3008_init(&adc, "/dev/spidev0.0", 3.3f) == 0);

    for (unsigned ch = 0; ch < MCP3008_NUM_CHANNELS; ch++)
        mcp3008_sim_set_channel(ch, (uint16_t)(1000 - ch * 100));

    uint16_t raw;
    for (unsigned ch = 0; ch < MCP3008_NUM_CHANNELS; ch++) {
        CHECK(mcp3008_read_raw(&adc, ch, &raw) == 0);
        CHECK(raw == 1000 - ch * 100);
    }
    CHECK(mcp3008_read_raw(&adc, 8, &raw) == -EINVAL);

    mcp3008_deinit(&adc);
    return 0;
}

static int test_w25q_jedec_and_size(void)
{
    w25q_t flash;
    CHECK(w25q_init(&flash, "/dev/spidev0.1") == 0);
    CHECK(flash.jedec_id == 0xEF4014);
    CHECK(flash.size_bytes == 1024 * 1024);
    w25q_deinit(&flash);
    return 0;
}

static int test_w25q_write_across_page_boundary(void)
{
    w25q_t flash;
    CHECK(w25q_init(&flash, "/dev/spidev0.1") == 0);
    CHECK(w25q_erase_sector(&flash, 0x1000) == 0);

    uint8_t out[600], in[600];
    for (size_t i = 0; i < sizeof(out); i++)
        out[i] = (uint8_t)(i * 7 + 3);

    const uint32_t addr = 0x1000 + 200; /* spans 3 pages */
    CHECK(w25q_write(&flash, addr, out, sizeof(out)) == 0);
    CHECK(w25q_read(&flash, addr, in, sizeof(in)) == 0);
    CHECK(memcmp(out, in, sizeof(out)) == 0);

    /* erase brings it back to 0xFF */
    CHECK(w25q_erase_sector(&flash, 0x1000) == 0);
    CHECK(w25q_read(&flash, addr, in, 16) == 0);
    for (int i = 0; i < 16; i++)
        CHECK(in[i] == 0xFF);

    CHECK(w25q_read(&flash, flash.size_bytes - 1, in, 2) == -ERANGE);
    w25q_deinit(&flash);
    return 0;
}

static int test_datalogger_roundtrip_and_resume(void)
{
    w25q_t flash;
    datalogger_t dl;
    CHECK(w25q_init(&flash, "/dev/spidev0.1") == 0);
    CHECK(datalogger_init(&dl, &flash, 0x2000) == 0);
    CHECK(datalogger_format(&dl) == 0);

    uint16_t raw[MCP3008_NUM_CHANNELS];
    for (uint16_t n = 0; n < 10; n++) {
        for (unsigned ch = 0; ch < MCP3008_NUM_CHANNELS; ch++)
            raw[ch] = (uint16_t)(n * 10 + ch);
        CHECK(datalogger_append(&dl, raw) == 0);
    }

    /* a fresh init must find the 10 existing records */
    datalogger_t dl2;
    CHECK(datalogger_init(&dl2, &flash, 0x2000) == 0);
    CHECK(dl2.next_seq == 10);

    dl_record_t rec;
    CHECK(datalogger_read(&dl2, 9, &rec) == 0);
    CHECK(rec.seq == 9 && rec.raw[3] == 93);
    CHECK(datalogger_read(&dl2, 10, &rec) == -ENOENT);

    w25q_deinit(&flash);
    return 0;
}

int main(void)
{
    log_set_level(LOG_LEVEL_WARN);
    printf("Running tests:\n");

    RUN(test_crc16_known_vector);
    RUN(test_mcp3008_reads_each_channel);
    RUN(test_w25q_jedec_and_size);
    RUN(test_w25q_write_across_page_boundary);
    RUN(test_datalogger_roundtrip_and_resume);

    printf("\n%d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed ? 1 : 0;
}
