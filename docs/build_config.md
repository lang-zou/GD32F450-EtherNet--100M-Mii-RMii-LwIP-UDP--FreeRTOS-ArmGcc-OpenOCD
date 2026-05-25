# GD32F450 FreeRTOS 项目 — 编译配置文档

## 1. 工具链与依赖

### 1.1 ARM GCC 工具链

| 项目 | 值 |
|------|-----|
| 版本 | **Arm GNU Toolchain 13.3.Rel1** (13.3.1 20240614) |
| 下载来源 | [ARM Developer](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) |
| 安装包 | `arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi.zip` (285 MB) |
| 安装路径 | `D:\tools\arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi\` |
| PATH 设置 | `export PATH="/c/tools/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi/bin:$PATH"` |
| 持久化 | 已写入 `~/.bashrc` |

**包含组件：**

| 工具 | 版本 | 用途 |
|------|------|------|
| arm-none-eabi-gcc | 13.3.1 | C/C++ 编译器 |
| arm-none-eabi-g++ | 13.3.1 | C++ 编译器 |
| arm-none-eabi-as | 2.42.0 | 汇编器 |
| arm-none-eabi-ld | 2.42.0 | 链接器 |
| arm-none-eabi-objcopy | 2.42.0 | 格式转换 (ELF→HEX/BIN) |
| arm-none-eabi-objdump | 2.42.0 | 反汇编 |
| arm-none-eabi-size | 2.42.0 | 段大小统计 |
| arm-none-eabi-gdb | 14.2.90 | 调试器 |

### 1.2 GNU Make

| 项目 | 值 |
|------|-----|
| 版本 | **GNU Make 4.4.1** |
| 下载来源 | [ezwinports](https://sourceforge.net/projects/ezwinports/) |
| 安装包 | `make-4.4.1-without-guile-w32-bin.zip` (383 KB) |
| 安装路径 | `/usr/bin/make.exe` |

### 1.3 OpenOCD

| 项目 | 值 |
|------|-----|
| 版本 | **xPack OpenOCD v0.12.0-7** (0.12.0+dev-02228-ge5888bda3-dirty, 2025-10-04) |
| 下载来源 | [xPack OpenOCD Releases](https://github.com/xpack-dev-tools/openocd-xpack/releases) |
| 安装包 | `xpack-openocd-0.12.0-7-win32-x64.zip` |
| 安装路径 | `D:\tools\xpack-openocd-0.12.0-7\` |
| 脚本路径 | `D:\tools\xpack-openocd-0.12.0-7\openocd\scripts\` (在 Makefile 中通过 `-s` 指定) |
| 用途 | `make flash` / `make debug` / `make erase` |

### 1.4 主机 GCC（单元测试用）

| 项目 | 值 |
|------|-----|
| 版本 | **MinGW-W64 GCC 16.1.0** (WinLibs build, x86_64-ucrt-posix-seh, r2) |
| 下载来源 | [WinLibs GitHub Releases](https://github.com/brechtsanders/winlibs_mingw/releases) |
| 安装包 | `winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r2.zip` (262 MB) |
| 安装路径 | `D:\tools\mingw64\mingw64\` |
| PATH 设置 | `export PATH="/c/tools/mingw64/mingw64/bin:$PATH"`（已写入 `~/.bashrc`） |
| 用途 | `make test` 在主机 PC 上编译运行单元测试 |

---

## 2. 第三方固件库

### 2.1 库清单

| 库名称 | 来源仓库 | 提交 hash | 本地路径 | 大小 |
|--------|---------|-----------|----------|------|
| GD32F4xx Firmware Library | `github.com/cjacker/gd32f4xx_firmware_library_gcc_makefile` | `3b5d2f2` | `src/bsp/gd32f4xx/` | 103 MB (完整库) |
| FreeRTOS Kernel | `github.com/FreeRTOS/FreeRTOS-Kernel` | `d3b074a` | `src/middleware/freertos/` | 23 MB (完整库) |
| LwIP | `github.com/lwip-tcpip/lwip` | `0c957ec` | `src/middleware/lwip/` | 12 MB (完整库) |
| Unity Test | `github.com/ThrowTheSwitch/Unity` | `4fe7d60` | `tests/unity/` | 2.1 MB (完整库) |

### 2.2 GD32F4xx Firmware Library 详细说明

GD32 官方库的社区 GCC 移植版，已适配 GCC 编译环境。

**使用的 CMSIS 文件：**

| 文件 | 用途 |
|------|------|
| `core_cm4.h` | Cortex-M4 CMSIS Core |
| `gd32f4xx.h` | GD32F4xx 设备头文件（含外设基址、中断号、ErrStatus 枚举等） |
| `system_gd32f4xx.c/.h` | 系统初始化（SystemInit, SystemCoreClock 更新） |
| `startup_gd32f450_470.S` | 启动文件（中断向量表 + Reset_Handler） |

**使用的 StdPeriph 驱动文件：**

| 文件 | 用途 | 代码行数 |
|------|------|---------|
| `gd32f4xx_rcu.c/.h` | 复位与时钟控制（PLL, HXTAL, 外设时钟） | ~2400 行 |
| `gd32f4xx_gpio.c/.h` | GPIO 输入/输出/复用配置 | ~600 行 |
| `gd32f4xx_enet.c/.h` | 以太网 MAC 控制器（DMA 描述符, SMI, 中断） | ~2200 行 |
| `gd32f4xx_usart.c/.h` | USART 串口驱动 | ~1000 行 |
| `gd32f4xx_dma.c/.h` | DMA 控制器 | ~900 行 |
| `gd32f4xx_misc.c/.h` | NVIC 中断优先级配置 | ~200 行 |
| `gd32f4xx_syscfg.c/.h` | 系统配置（GPIO AF, ENET PHY 接口） | ~300 行 |
| `gd32f4xx_exti.c/.h` | 外部中断 | ~300 行 |
| `gd32f4xx_fmc.c/.h` | Flash 存储器控制器 | ~1600 行 |
| `gd32f4xx_timer.c/.h` | 定时器 | ~3500 行 |
| `gd32f4xx_pmu.c/.h` | 电源管理（备份域访问） | ~300 行 |
| `gd32f4xx_libopt.h` | 库配置头文件（选择性包含所有外设头文件） | — |

部分文件编译时有未使用变量警告（`gd32f4xx_fmc.c` 中 `wp0_state`/`wp1_state`），不影响功能。

### 2.3 FreeRTOS Kernel 详细说明

| 文件 | 用途 |
|------|------|
| `tasks.c` | 任务管理（创建、删除、延时、调度） |
| `queue.c` | 队列 |
| `list.c` | 双向链表（调度器核心数据结构） |
| `timers.c` | 软件定时器 |
| `event_groups.c` | 事件组 |
| `stream_buffer.c` | 流缓冲区 |
| `croutine.c` | 协程（未使用） |
| `port.c` | Cortex-M4F 移植层（PendSV, SysTick, SVC） |
| `heap_4.c` | 内存管理（heap_4: 最佳匹配 + 合并空闲块） |
| `FreeRTOS.h` | 内核 API 头文件 |

### 2.4 LwIP 详细说明

| 类别 | 文件 | 用途 |
|------|------|------|
| Core | `init.c` | LwIP 初始化 |
| Core | `def.c` | 通用定义 |
| Core | `mem.c` | 堆内存管理 |
| Core | `memp.c` | 内存池管理 |
| Core | `netif.c` | 网络接口抽象 |
| Core | `pbuf.c` | 数据包缓冲区 |
| Core | `sys.c` | OS 抽象层 |
| Core | `timeouts.c` | 定时器管理 |
| Core | `udp.c` | UDP 协议 |
| IPv4 | `ip4.c` | IP 层 |
| IPv4 | `ip4_addr.c` | IP 地址处理 |
| IPv4 | `ip4_frag.c` | IP 分片 |
| IPv4 | `etharp.c` | ARP 协议 |
| IPv4 | `icmp.c` | ICMP (Ping) |
| IPv4 | `dhcp.c` | DHCP (未启用) |
| Netif | `ethernet.c` | 以太网帧 |
| API | `api_lib.c`, `api_msg.c`, `err.c` 等 | Socket API (未启用) |
| Arch | `arch/cc.h` | 编译器抽象（GCC packed struct, 平台断言） |
| Arch | `arch/cpu.h` | 端序定义（Little Endian） |
| Arch | `arch/sys_arch.h` | OS 抽象层类型定义 |

### 2.5 Unity 测试框架

| 文件 | 用途 |
|------|------|
| `unity.c` | 测试框架核心 |
| `unity.h` | 公共 API |
| `unity_internals.h` | 内部实现 |

---

## 3. 编译配置

### 3.1 MCU 目标

```
MCU:      GD32F450IK6
Core:     Cortex-M4F (ARMv7E-M)
FPU:      fpv4-sp-d16 (单精度硬件浮点)
最大频率: 200 MHz
Flash:    1024 KB (实际可用约 1MB)
SRAM:     512 KB (448 KB + 64 KB TCM)
```

### 3.2 编译选项

```makefile
# CPU/FPU
CPU        = -mcpu=cortex-m4
FPU        = -mfpu=fpv4-sp-d16 -mfloat-abi=hard
MCU        = $(CPU) -mthumb $(FPU)

# C 编译器标志
CFLAGS     = $(MCU)
CFLAGS    += -Wall -Wextra -Wshadow -Wundef        # 警告
CFLAGS    += -Os -g3 -gdwarf-2                      # 优化 + 调试信息
CFLAGS    += -ffunction-sections -fdata-sections     # 允许 gc-sections
CFLAGS    += -fno-common -fmessage-length=0
CFLAGS    += -std=c11                                 # C11 标准
CFLAGS    += -DGD32F450                                # 设备宏
CFLAGS    += -DUSE_STDPERIPH_DRIVER                    # 使用标准外设库
```

### 3.3 链接选项

```makefile
LDSCRIPT   = linker/gd32f450_flash.ld
LDFLAGS    = $(MCU) -specs=nosys.specs -T$(LDSCRIPT)
LDFLAGS   += -Wl,-Map=$(TARGET).map,--cref
LDFLAGS   += -Wl,--gc-sections             # 移除未使用段
LDFLAGS   += -lc -lm -lnosys               # 链接 C 库/数学库/nosys
```

### 3.4 链接脚本内存分区

```
FLASH (rx)  ORIGIN = 0x08000000, LENGTH = 1024K
RAM   (rwx) ORIGIN = 0x20000000, LENGTH = 448K
TCM   (rwx) ORIGIN = 0x10000000, LENGTH = 64K

段布局:
  .isr_vector → Flash 起始
  .text       → Flash
  .rodata     → Flash
  .data       → Flash (LMA) → RAM (VMA)
  .bss        → RAM
  .tcm_data   → TCM (0x10000000)
  .heap       → RAM 末尾 (32 KB)
  .stack      → RAM 末尾
```

### 3.5 Include 路径

```
-Isrc
-Isrc/bsp
-Isrc/bsp/gd32f4xx/CMSIS
-Isrc/bsp/gd32f4xx/StdPeriph
-Isrc/drivers/eth
-Isrc/drivers/uart
-Isrc/drivers/flash
-Isrc/drivers/timer
-Isrc/middleware/freertos
-Isrc/middleware/lwip/include
-Isrc/middleware/shell
-Isrc/utils
-Isrc/app/boot
-Isrc/app/ota
-Isrc/app/demo
-Iconfig
```

### 3.6 PHY 模式选择

默认 RMII，可通过命令行切换：

```bash
# RMII (LAN8720A) — 默认
make

# MII (DP83848)
make ETH_PHY_MODE=MII
```

Makefile 中的实现：

```makefile
ETH_PHY_MODE ?= RMII

ifeq ($(ETH_PHY_MODE),MII)
C_SRCS += $(SRC_DIR)/drivers/eth/phy_dp83848.c
else
C_SRCS += $(SRC_DIR)/drivers/eth/phy_lan8720.c
endif
```

### 3.7 编译源文件清单

共 **60+** 个源文件：

| 类别 | 文件数 | 说明 |
|------|--------|------|
| CMSIS + 启动 | 2 | system_gd32f4xx.c + startup_gd32f450_470.S |
| StdPeriph 驱动 | 11 | RCU, GPIO, ENET, USART, DMA, MISC, SYSCFG, EXTI, FMC, TIMER, PMU |
| BSP | 2 | board.c, bsp_led.c |
| 驱动层 | 7 | eth_mac.c, phy_lan8720.c, ethernetif.c, uart_console.c, flash_if.c, systick_rtos.c |
| FreeRTOS | 9 | tasks.c, queue.c, list.c, timers.c, event_groups.c, stream_buffer.c, croutine.c, port.c, heap_4.c |
| LwIP | 24 | 核心 + IPv4 + netif + API (src/middleware/lwip) |
| Shell | 2 | cli.c, cli_cmds.c |
| 工具类 | 4 | log.c, assert.c, crc32.c, ringbuf.c |
| 应用层 | 4 | boot.c, ota.c, udp_echo.c, main.c |

---

## 4. 构建产物

| 产物 | 路径 | 说明 |
|------|------|------|
| `firmware.elf` | `build/firmware.elf` | ELF 可执行文件（含调试符号） |
| `firmware.hex` | `build/firmware.hex` | Intel HEX 格式（用于 OpenOCD 烧录） |
| `firmware.bin` | `build/firmware.bin` | Raw binary 格式（用于 OTA 升级） |
| `firmware.map` | `build/firmware.map` | 链接 Map 文件（符号交叉引用） |

### 编译输出示例

```
[CC] system_gd32f4xx.c
[CC] gd32f4xx_rcu.c
...
[AS] startup_gd32f450_470.S
[LD] build/firmware.elf
[HEX] build/firmware.hex
[BIN] build/firmware.bin
=== Memory Usage ===
   text    data     bss     dec     hex  filename
  53420    1780  129384  184584   2d108  build/firmware.elf
```

---

## 5. 链接警告说明

### 5.1 newlib 系统调用未实现（可忽略）

```
warning: _write is not implemented and will always fail
warning: _fstat is not implemented and will always fail
warning: _isatty is not implemented and will always fail
warning: _getpid is not implemented and will always fail
warning: _kill is not implemented and will always fail
warning: _read is not implemented and will always fail
```

这些是 newlib C 标准库的小型 stub 函数。链接时使用了 `-specs=nosys.specs` 和 `-lnosys`，但这些 stub 在某些函数调用链中仍会触发。项目不依赖标准 I/O 或文件系统操作，**完全不影响实际功能**。

### 5.2 RWX 段权限警告（可忽略）

```
warning: build/firmware.elf has a LOAD segment with RWX permissions
```

嵌入式链接脚本将 `.data` 段的 LMA 放在 Flash 中、VMA 放在 RAM 中，GNU ld 将其报告为 RWX 段。这是标准做法，**不影响运行**。

---

## 6. 编译命令参考

```bash
# 编译（默认 RMII 模式）
make

# 编译 MII 模式
make ETH_PHY_MODE=MII

# 烧录固件（需要 OpenOCD + J-Link）
make flash

# 整片擦除（需要 OpenOCD）
make erase

# 调试（需要 OpenOCD + GDB）
make debug

# 清理构建文件
make clean

# 代码格式化（需要 clang-format）
make format

# 单元测试（需要主机 GCC）
make test
```

---

## 7. 下载资源清单

| # | 资源名称 | 来源 | 版本/提交 | 大小 | 存放位置 |
|---|---------|------|-----------|------|---------|
| 1 | ARM GNU Toolchain | ARM Developer | 13.3.Rel1 | 285 MB (ZIP) | `download/` |
| 2 | GD32F4xx Firmware Library (GCC) | github.com/cjacker/gd32f4xx_firmware_library_gcc_makefile | `3b5d2f2` | 103 MB | `download/`（源码集成到 `src/bsp/gd32f4xx/`） |
| 3 | FreeRTOS Kernel | github.com/FreeRTOS/FreeRTOS-Kernel | `d3b074a` | 23 MB | `download/`（源码集成到 `src/middleware/freertos/`） |
| 4 | LwIP | github.com/lwip-tcpip/lwip | `0c957ec` | 12 MB | `download/`（源码集成到 `src/middleware/lwip/`） |
| 5 | Unity Test | github.com/ThrowTheSwitch/Unity | `4fe7d60` | 2.1 MB | `download/`（源码集成到 `tests/unity/`） |
| 6 | xPack OpenOCD | github.com/xpack-dev-tools/openocd-xpack | v0.12.0-7 | — | `download/`（安装到 `D:\tools\`） |
| 7 | MinGW-W64 GCC (WinLibs) | github.com/brechtsanders/winlibs_mingw | 16.1.0 | 262 MB (ZIP) | `download/`（安装到 `D:\tools\`） |
| 8 | GNU Make | ezwinports (SourceForge) | 4.4.1 | 384 KB (ZIP) | `download/`（安装到 `/usr/bin/`） |

> **总计下载量：~687 MB**，全部归置于 `download/` 目录
