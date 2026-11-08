# spi-app Makefile
#
#   make                          build (Linux host -> spidev backend, others -> mock)
#   make BACKEND=mock             force the simulated SPI backend
#   make DEBUG=0                  optimized release build
#   make CROSS_COMPILE=aarch64-linux-gnu-   cross-compile for an ARM64 board
#   make test                     build + run unit tests (mock backend)
#   make run                      build + run the app
#   make clean

PROJECT := spi-app

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  BACKEND ?= linux
else
  BACKEND ?= mock
endif
ifneq ($(CROSS_COMPILE),)
  BACKEND := linux
  CC      := $(CROSS_COMPILE)gcc
endif

DEBUG ?= 1

BUILD_DIR := build/$(BACKEND)
TARGET    := $(BUILD_DIR)/$(PROJECT)
TEST_BIN  := $(BUILD_DIR)/test_runner

CPPFLAGS := -Iinclude -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -MMD -MP
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -Wshadow
ifeq ($(DEBUG),1)
  CFLAGS += -O0 -g3            # full debug info, no optimization -> easy stepping
else
  CFLAGS += -O2 -DNDEBUG
endif

# ---- modules ---------------------------------------------------------------
HAL_SRCS     := src/hal/spi_common.c
DRIVER_SRCS  := src/drivers/mcp3008.c src/drivers/w25q.c
UTIL_SRCS    := src/utils/log.c src/utils/crc16.c
APP_SRCS     := src/app/datalogger.c
MOCK_SRCS    := src/hal/spi_mock.c src/sim/mcp3008_sim.c src/sim/w25q_sim.c

ifeq ($(BACKEND),linux)
  BACKEND_SRCS := src/hal/spi_linux.c
else ifeq ($(BACKEND),mock)
  BACKEND_SRCS := $(MOCK_SRCS)
else
  $(error BACKEND must be 'linux' or 'mock')
endif

LIB_SRCS := $(HAL_SRCS) $(DRIVER_SRCS) $(UTIL_SRCS) $(APP_SRCS)
SRCS     := $(LIB_SRCS) $(BACKEND_SRCS) src/main.c
OBJS     := $(SRCS:%.c=$(BUILD_DIR)/%.o)

TEST_SRCS := $(LIB_SRCS) $(MOCK_SRCS) tests/test_main.c
TEST_OBJS := $(TEST_SRCS:%.c=$(BUILD_DIR)/%.o)

# ---- rules -----------------------------------------------------------------
.PHONY: all run test clean info

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Built $@ (backend=$(BACKEND), debug=$(DEBUG))"

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(ARGS)

# Tests always use the mock backend so they run anywhere.
ifeq ($(BACKEND),mock)
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
else
test:
	$(MAKE) BACKEND=mock CROSS_COMPILE= test
endif

info:
	@echo "CC=$(CC) BACKEND=$(BACKEND) DEBUG=$(DEBUG) TARGET=$(TARGET)"

clean:
	rm -rf build

-include $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)
