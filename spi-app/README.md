# spi-app — multi-module Linux SPI project

A userspace C application that talks to two SPI peripherals through a small HAL:

- **MCP3008**: 8-channel 10-bit ADC
- **W25Qxx**: SPI NOR flash

It reads the ADC, logs the samples to flash as CRC-checked records, and reads them back to check them.

## Modules

| Module      | Files                                   | Role                                          |
|-------------|-----------------------------------------|-----------------------------------------------|
| SPI HAL     | `include/spi.h`, `src/hal/spi_common.c` | Backend-independent SPI API                   |
| spidev      | `src/hal/spi_linux.c`                   | Real hardware via `/dev/spidevB.C` + ioctl    |
| mock SPI    | `src/hal/spi_mock.c`, `src/sim/*`       | Byte-level chip simulators (runs on any OS)   |
| ADC driver  | `src/drivers/mcp3008.c`                 | Single-ended conversions                      |
| Flash driver| `src/drivers/w25q.c`                    | JEDEC ID, read, page program, sector erase    |
| Datalogger  | `src/app/datalogger.c`                  | Append-only records with CRC-16 in flash      |
| Utils       | `src/utils/log.c`, `src/utils/crc16.c`  | Leveled logging + hexdump, CRC-16/CCITT       |
| App         | `src/main.c`                            | CLI entry point                               |
| Tests       | `tests/test_main.c`                     | Unit tests against the mock backend           |

```
main ──► datalogger ──► w25q ──┐
   └───────────────► mcp3008 ──┴─► spi.h ──► spi_linux.c (spidev)  | spi_mock.c ──► sim/*
```

## Build

```sh
make                 # Linux host -> spidev backend; macOS -> mock backend
make BACKEND=mock    # force simulated SPI (e.g. Linux PC with no SPI hardware)
make test            # unit tests (always mock)
make run ARGS="-v -n 10"
make DEBUG=0         # optimized build
make CROSS_COMPILE=aarch64-linux-gnu-     # cross-build for a Pi/ARM64 board
```

The output goes to `build/<backend>/spi-app`.

## Run on real hardware (e.g. Raspberry Pi)

1. Enable SPI: run `sudo raspi-config` → Interface Options → SPI, or add `dtparam=spi=on` to `/boot/firmware/config.txt`.
2. Wire the MCP3008 to CE0 (`/dev/spidev0.0`) and the W25Q flash to CE1 (`/dev/spidev0.1`).
3. Run `./spi-app -v`. Use `-vv` to also trace the raw SPI bytes (this works in the mock backend).

## Debug

- **VS Code** (needs the C/C++ extension). Open the `embedded` folder, press F5, and pick one of these:
  - *debug locally (mock SPI)*: builds, then stops at `main`. Set breakpoints in any driver.
  - *debug unit tests*: runs the tests under the debugger.
  - *remote debug on target*: on the board, run `gdbserver :2345 ./spi-app`. Debug from a Linux host that has `gdb-multiarch`.
- **CLI**: `lldb build/mock/spi-app` on macOS, or `gdb build/mock/spi-app` on Linux.
  - Example: `b w25q_write`, then `run`, then `bt`.
- **macOS**: the first time you debug, approve the "Developer Tools Access" prompt, or run `sudo DevToolsSecurity -enable` once.
