# ============================================================================
# GD32F450 FreeRTOS Project — Makefile
# ============================================================================
# Build targets:
#   make          - Build firmware (ELF + HEX + BIN)
#   make flash    - Flash firmware via OpenOCD + J-Link
#   make erase    - Mass erase chip via OpenOCD
#   make debug    - Start OpenOCD server + GDB session
#   make clean    - Remove build artifacts
#   make test     - Build and run unit tests on host PC
#   make format   - Format source code with clang-format
# ============================================================================

# ── PHY Selection (RMII or MII) ───────────────────────────────────────────────
ETH_PHY_MODE ?= RMII

# ── Toolchain ────────────────────────────────────────────────────────────────
TRIPLET        = arm-none-eabi-
CC             = $(TRIPLET)gcc
CXX            = $(TRIPLET)g++
AS             = $(TRIPLET)gcc -x assembler-with-cpp
OBJCOPY        = $(TRIPLET)objcopy
OBJDUMP        = $(TRIPLET)objdump
SIZE           = $(TRIPLET)size
GDB            = $(TRIPLET)gdb
OPENOCD_DIR    = /d/tools/xpack-openocd-0.12.0-7
OPENOCD        = $(OPENOCD_DIR)/bin/openocd
OPENOCD_SCRIPTS = $(OPENOCD_DIR)/openocd/scripts

# ── Directories ─────────────────────────────────────────────────────────────
BUILD_DIR  = build
SRC_DIR    = src
CONFIG_DIR = config

# ── Output ───────────────────────────────────────────────────────────────────
TARGET     = $(BUILD_DIR)/firmware

# ── MCU Flags ────────────────────────────────────────────────────────────────
CPU        = -mcpu=cortex-m4
FPU        = -mfpu=fpv4-sp-d16 -mfloat-abi=hard
MCU        = $(CPU) -mthumb $(FPU)

# ── Compiler Flags ───────────────────────────────────────────────────────────
CFLAGS     = $(MCU)
CFLAGS    += -Wall -Wextra -Wshadow -Wundef
CFLAGS    += -Os -g3 -gdwarf-2
CFLAGS    += -ffunction-sections -fdata-sections
CFLAGS    += -fno-common -fmessage-length=0
CFLAGS    += -std=c11
CFLAGS    += -DGD32F450
CFLAGS    += -DUSE_STDPERIPH_DRIVER

# ── Include Paths ────────────────────────────────────────────────────────────
INC        = -I$(CONFIG_DIR)
INC       += -Isrc
INC       += -Isrc/bsp
INC       += -Isrc/bsp/gd32f4xx/CMSIS
INC       += -Isrc/bsp/gd32f4xx/StdPeriph
INC       += -Isrc/drivers/eth
INC       += -Isrc/drivers/uart
INC       += -Isrc/drivers/flash
INC       += -Isrc/drivers/timer
INC       += -Isrc/middleware/freertos
INC       += -Isrc/middleware/lwip/include
INC       += -Isrc/middleware/shell
INC       += -Isrc/utils
INC       += -Isrc/app/boot
INC       += -Isrc/app/ota
INC       += -Isrc/app/demo

# ── Linker Flags ─────────────────────────────────────────────────────────────
LDSCRIPT   = linker/gd32f450_flash.ld
LDFLAGS    = $(MCU) -specs=nosys.specs -T$(LDSCRIPT)
LDFLAGS   += -Wl,-Map=$(TARGET).map,--cref
LDFLAGS   += -Wl,--gc-sections
LDFLAGS   += -lc -lm -lnosys

# ── Source Files ─────────────────────────────────────────────────────────────
# CMSIS + Startup
C_SRCS     = $(SRC_DIR)/bsp/gd32f4xx/CMSIS/system_gd32f4xx.c
AS_SRCS    = $(SRC_DIR)/bsp/gd32f4xx/CMSIS/startup_gd32f450_470.S

# StdPeriph Drivers
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_rcu.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_gpio.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_enet.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_usart.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_dma.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_misc.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_syscfg.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_exti.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_fmc.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_timer.c
C_SRCS    += $(SRC_DIR)/bsp/gd32f4xx/StdPeriph/gd32f4xx_pmu.c

# BSP
C_SRCS    += $(SRC_DIR)/bsp/board.c
C_SRCS    += $(SRC_DIR)/bsp/bsp_led.c

# Drivers
C_SRCS    += $(SRC_DIR)/drivers/eth/eth_mac.c
ifeq ($(ETH_PHY_MODE),MII)
C_SRCS    += $(SRC_DIR)/drivers/eth/phy_dp83848.c
else
C_SRCS    += $(SRC_DIR)/drivers/eth/phy_lan8720.c
endif
C_SRCS    += $(SRC_DIR)/drivers/eth/ethernetif.c
C_SRCS    += $(SRC_DIR)/drivers/uart/uart_console.c
C_SRCS    += $(SRC_DIR)/drivers/flash/flash_if.c
C_SRCS    += $(SRC_DIR)/drivers/timer/systick_rtos.c

# FreeRTOS
C_SRCS    += $(SRC_DIR)/middleware/freertos/tasks.c
C_SRCS    += $(SRC_DIR)/middleware/freertos/queue.c
C_SRCS    += $(SRC_DIR)/middleware/freertos/list.c
C_SRCS    += $(SRC_DIR)/middleware/freertos/timers.c
C_SRCS    += $(SRC_DIR)/middleware/freertos/event_groups.c
C_SRCS    += $(SRC_DIR)/middleware/freertos/stream_buffer.c
C_SRCS    += $(SRC_DIR)/middleware/freertos/port.c
C_SRCS    += $(SRC_DIR)/middleware/freertos/heap_4.c

# LwIP (UDP-only subset)
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/init.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/def.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/mem.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/memp.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/netif.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/pbuf.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/sys.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/timeouts.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/udp.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/ipv4/ip4.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/ipv4/ip4_addr.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/ipv4/ip4_frag.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/ipv4/etharp.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/ipv4/icmp.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/ipv4/dhcp.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/core/inet_chksum.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/netif/ethernet.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/api/api_lib.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/api/api_msg.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/api/err.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/api/if_api.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/api/netbuf.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/api/netdb.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/api/netifapi.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/api/sockets.c
C_SRCS    += $(SRC_DIR)/middleware/lwip/api/tcpip.c

# Shell
C_SRCS    += $(SRC_DIR)/middleware/shell/cli.c
C_SRCS    += $(SRC_DIR)/middleware/shell/cli_cmds.c

# Utils
C_SRCS    += $(SRC_DIR)/utils/log.c
C_SRCS    += $(SRC_DIR)/utils/assert.c
C_SRCS    += $(SRC_DIR)/utils/crc32.c
C_SRCS    += $(SRC_DIR)/utils/ringbuf.c

# Application
C_SRCS    += $(SRC_DIR)/app/boot/boot.c
C_SRCS    += $(SRC_DIR)/app/ota/ota.c
C_SRCS    += $(SRC_DIR)/app/demo/udp_echo.c

# Main
C_SRCS    += $(SRC_DIR)/main.c

# ── Object Files ─────────────────────────────────────────────────────────────
C_OBJS     = $(C_SRCS:%.c=$(BUILD_DIR)/%.o)
AS_OBJS    = $(AS_SRCS:%.S=$(BUILD_DIR)/%.o)
OBJS       = $(C_OBJS) $(AS_OBJS)
OBJ_DIRS   = $(sort $(dir $(OBJS)))

# ── Colors ───────────────────────────────────────────────────────────────────
BOLD       = \033[1m
GREEN      = \033[32m
CYAN       = \033[36m
RED        = \033[31m
RESET      = \033[0m

# ── Default Target ───────────────────────────────────────────────────────────
.PHONY: all
all: $(TARGET).elf $(TARGET).hex $(TARGET).bin size

# ── Create build directories ─────────────────────────────────────────────────
$(OBJ_DIRS):
	@mkdir -p $@

# ── Compile C ────────────────────────────────────────────────────────────────
$(BUILD_DIR)/%.o: %.c | $(OBJ_DIRS)
	@echo "$(GREEN)[CC]$(RESET) $(notdir $<)"
	@$(CC) $(CFLAGS) $(INC) -MMD -MP -c $< -o $@

# ── Compile Assembly ─────────────────────────────────────────────────────────
$(BUILD_DIR)/%.o: %.S | $(OBJ_DIRS)
	@echo "$(CYAN)[AS]$(RESET) $(notdir $<)"
	@$(AS) $(MCU) $(INC) -c $< -o $@

# ── Link ─────────────────────────────────────────────────────────────────────
$(TARGET).elf: $(OBJS)
	@echo "$(BOLD)[LD]$(RESET) $@"
	@$(CC) $(LDFLAGS) $(OBJS) -o $@
	@$(SIZE) $@

# ── Binary Outputs ───────────────────────────────────────────────────────────
$(TARGET).hex: $(TARGET).elf
	@echo "$(CYAN)[HEX]$(RESET) $@"
	@$(OBJCOPY) -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	@echo "$(CYAN)[BIN]$(RESET) $@"
	@$(OBJCOPY) -O binary $< $@

# ── Size ─────────────────────────────────────────────────────────────────────
.PHONY: size
size: $(TARGET).elf
	@echo "$(BOLD)=== Memory Usage ===$(RESET)"
	@$(SIZE) --format=berkeley $(TARGET).elf

# ── Flash via OpenOCD + J-Link ───────────────────────────────────────────────
.PHONY: flash
flash: $(TARGET).hex
	@echo "$(BOLD)[FLASH]$(RESET) Programming firmware..."
	$(OPENOCD) -s $(OPENOCD_SCRIPTS) -f scripts/openocd/gd32f4x_jlink.cfg \
	           -c "init; reset halt; program $(TARGET).hex verify reset exit" \
	           -c "shutdown" 2>&1 || true

# ── Erase Chip ───────────────────────────────────────────────────────────────
.PHONY: erase
erase:
	@echo "$(RED)[ERASE]$(RESET) Mass erasing chip..."
	$(OPENOCD) -s $(OPENOCD_SCRIPTS) -f scripts/openocd/gd32f4x_jlink.cfg \
	           -c "init; reset halt; gd32f4x mass_erase; exit" \
	           -c "shutdown" 2>&1 || true

# ── Debug (Start OpenOCD + GDB) ──────────────────────────────────────────────
.PHONY: debug
debug: $(TARGET).elf
	@echo "$(BOLD)[DEBUG]$(RESET) Starting OpenOCD in background..."
	$(OPENOCD) -s $(OPENOCD_SCRIPTS) -f scripts/openocd/gd32f4x_jlink.cfg &
	@sleep 2
	@echo "$(BOLD)[GDB]$(RESET) Connecting..."
	$(GDB) -x scripts/gdb/gdbinit $(TARGET).elf

# ── Clean ────────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	@echo "$(RED)[CLEAN]$(RESET) Removing build artifacts..."
	@rm -rf $(BUILD_DIR)

# ── Format Code ──────────────────────────────────────────────────────────────
.PHONY: format
format:
	@echo "$(BOLD)[FORMAT]$(RESET) Formatting source code..."
	@find src -name "*.c" -o -name "*.h" | xargs clang-format -i
	@echo "Done."

# ── Unit Tests (Host PC) ─────────────────────────────────────────────────────
HOST_CC      = /d/tools/mingw64/mingw64/bin/gcc
HOST_CFLAGS  = -Wall -Wextra -g -std=c11
HOST_CFLAGS += -I$(CONFIG_DIR) -Isrc/utils -Itests/unity

TEST_DEPS    = tests/unity/unity.c src/utils/crc32.c src/utils/ringbuf.c

.PHONY: test
test:
	@echo "$(BOLD)[TEST]$(RESET) Building and running unit tests..."
	@mkdir -p $(BUILD_DIR)
	@failed=0; \
	for test_src in tests/unit/test_crc32.c tests/unit/test_ringbuf.c; do \
		name=$$(basename $$test_src .c); \
		echo "  [$$name]"; \
		$(HOST_CC) $(HOST_CFLAGS) $(TEST_DEPS) $$test_src -o $(BUILD_DIR)/$$name.exe; \
		if $(BUILD_DIR)/$$name.exe; then \
			echo "  [$$name] PASSED"; \
		else \
			echo "  [$$name] FAILED"; \
			failed=1; \
		fi; \
	done; \
	exit $$failed

# ── Include dependency files ─────────────────────────────────────────────────
-include $(C_OBJS:.o=.d)
