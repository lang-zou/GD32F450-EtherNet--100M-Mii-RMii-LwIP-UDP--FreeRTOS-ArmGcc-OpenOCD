# GD32F450 FreeRTOS 嵌入式项目

基于 GD32F450IK6 (Cortex-M4F) 的嵌入式软件开发框架，集成 FreeRTOS + LwIP 百兆以太网 + OTA 固件升级 + UART 命令行 Shell。

**可作为新项目模板直接复用，也可作为 GD32/ARM GCC/FreeRTOS/LwIP 技术参考。**

---

## 快速开始

```bash
make                  # 编译固件 (ELF + HEX + BIN)
make flash            # 烧录 (J-Link + OpenOCD)
make debug            # 调试 (OpenOCD + GDB)
make test             # 单元测试 (主机 PC)
make clean            # 清理

# PHY 模式切换
make ETH_PHY_MODE=MII    # MII 模式 (DP83848)
make ETH_PHY_MODE=RMII   # RMII 模式 (LAN8720A, 默认)
```

---

## 项目特性

| 特性 | 说明 |
|------|------|
| **MCU** | GD32F450IK6, Cortex-M4F, 200MHz, 512KB SRAM |
| **RTOS** | FreeRTOS V10.6.2, 1KHz tick, heap_4 内存管理 |
| **以太网** | 10/100Mbps, RMII (LAN8720A) + MII (DP83848) 双模 |
| **协议栈** | LwIP, UDP Echo demo (端口 5000) |
| **OTA 升级** | UDP 自定义协议, CRC32 校验, 统一固件架构 |
| **调试** | UART Shell (115200-8-N-1), help/info/eth/ota/reset 命令 |
| **工具链** | ARM GCC 13.3 + OpenOCD 0.12 + J-Link, 完全去 Keil |
| **测试** | Unity 测试框架, `make test` 一键运行 |
| **代码标准** | C11, 四层分层架构, 配置集中管理, 英文注释 |

---

## 软件架构

```
┌──────────────────────────────────────────────┐
│          Application Layer (app/)            │
│  boot.c/ota.c    udp_echo.c    cli_cmds.c   │
├──────────────────────────────────────────────┤
│          Middleware Layer (middleware/)       │
│  FreeRTOS      LwIP          Shell (cli)     │
├──────────────────────────────────────────────┤
│          Driver Layer (drivers/)             │
│  eth_mac  │  phy_lan8720   │  uart_console  │
│  phy_dp83848 │  flash_if   │  systick_rtos  │
├──────────────────────────────────────────────┤
│          BSP/HAL Layer (bsp/)                │
│  GD32F4xx StdPeriph  │  CMSIS  │  board     │
├──────────────────────────────────────────────┤
│          Hardware                            │
│  GD32F450IK6  │  LAN8720A/DP83848  │  J-Link │
└──────────────────────────────────────────────┘
```

**调用规则**：上层可调用下层 API，下层不依赖上层。跨层调用必须经过紧邻下层转发。

---

## 启动流程

```
上电 → Reset_Handler (startup.S)
     → SystemInit() (system_gd32f4xx.c)
     → main()
          ├─ board_clock_init()         → 配置 200MHz PLL
          ├─ boot_get_mode()            → 检测启动模式
          │    ├─ RTC BKP 寄存器        → 最高优先: OTA 强制进入
          │    ├─ GPIO 按键             → 硬件触发 bootloader
          │    └─ App 向量表/SP 校验    → 自动恢复出厂
          │
          ├─ [Bootloader 路径]
          │    └─ ota_bootloader_entry()
          │         ├─ 最小外设初始化 (ETH + UART)
          │         ├─ 绑定 UDP 端口 5001
          │         ├─ 接收固件 → CRC32 校验 → 写入 Flash
          │         └─ NVIC_SystemReset()
          │
          └─ [App 路径]
               └─ app_entry()
                    ├─ board_periph_init() → 全外设
                    ├─ network_init() → LwIP + 静态 IP
                    ├─ xTaskCreate() × N
                    └─ vTaskStartScheduler() → 永不返回
```

---

## 目录结构

```
embedded_proj/
├── config/                     # 集中配置
│   ├── app_config.h            # 功能开关 + 网络参数 + Flash 分区
│   ├── FreeRTOSConfig.h        # FreeRTOS 内核参数
│   └── lwipopts.h              # LwIP 协议栈参数
├── linker/
│   └── gd32f450_flash.ld       # 链接脚本 (Flash/RAM/TCM 分区)
├── scripts/
│   ├── openocd/                # OpenOCD J-Link/DAP-Link 配置
│   └── gdb/                    # GDB 初始化脚本
├── src/
│   ├── main.c                  # ★ 唯一入口
│   ├── app/                    # 应用层
│   │   ├── boot/boot.c         # 启动模式检测
│   │   ├── ota/ota.c           # OTA 固件升级
│   │   └── demo/udp_echo.c     # UDP Echo demo
│   ├── bsp/                    # 板级支持包
│   │   ├── board.c/.h          # 时钟 + 引脚映射
│   │   ├── bsp_led.c/.h        # LED 抽象
│   │   └── gd32f4xx/           # GD32 官方外设库
│   │       ├── CMSIS/          # core_cm4.h + system_gd32f4xx + startup
│   │       └── StdPeriph/      # 11 个外设驱动文件
│   ├── drivers/                # 硬件驱动层
│   │   ├── eth/                # ENET MAC + 双 PHY + LwIP 移植层
│   │   ├── uart/               # 调试串口控制台
│   │   ├── flash/              # 内部 Flash 读/写/擦除
│   │   └── timer/              # SysTick (FreeRTOS tick 源)
│   ├── middleware/              # 第三方中间件
│   │   ├── freertos/           # FreeRTOS 内核源码
│   │   ├── lwip/               # LwIP TCP/IP 协议栈
│   │   └── shell/              # UART 命令行 Shell
│   └── utils/                  # 通用工具
│       ├── log.c/.h            # 分级日志 (ERROR/WARN/INFO/DEBUG)
│       ├── crc32.c/.h          # CRC32 (固件校验)
│       └── ringbuf.c/.h        # 环形缓冲区 (UART 接收)
├── tests/
│   ├── unity/                  # Unity 测试框架
│   └── unit/                   # 单元测试用例 (CRC32 × 8, RingBuf × 9)
├── download/                   # 工具链 & 固件库 (688 MB, 仅存档)
├── docs/                       # 技术文档
├── Makefile                    # 构建入口
└── README.md                   # 本文件
```

---

## 配置管理

所有可调参数集中在 `config/` 目录，不分散在代码中：

| 文件 | 管理内容 | 关键配置项 |
|------|---------|-----------|
| `app_config.h` | 功能开关、网络参数、Flash 分区 | `ETH_PHY_MODE`, `ENABLE_SHELL`, `LOG_LEVEL`, IP/端口 |
| `FreeRTOSConfig.h` | RTOS 内核参数 | `TICK_RATE_HZ=1000`, `TOTAL_HEAP_SIZE=64KB`, `MAX_PRIORITIES=8` |
| `lwipopts.h` | LwIP 协议栈参数 | `NO_SYS=0`, `MEM_SIZE`, `LWIP_UDP=1`, `LWIP_TCP=0` |
| `board.h` | 硬件引脚映射 | LED PC6, Button PA0, UART PA9/PA10, RMII/MII 完整引脚 |

---

## 编译产物

当前内存占用 (编译优化 `-Os`):

```
   text     data     bss      dec     hex
  53,484    1,780  129,384  184,648  2d148
```

GD32F450IK6 可用 **Flash 1024KB / SRAM 448KB**，余量充足。

| 产物 | 路径 | 用途 |
|------|------|------|
| `firmware.elf` | `build/` | ELF, 含调试符号, GDB 用 |
| `firmware.hex` | `build/` | Intel HEX, `make flash` 烧录用 |
| `firmware.bin` | `build/` | Raw binary, OTA 升级用 |
| `firmware.map` | `build/` | 链接 Map, 符号交叉引用 |

---

## 开发环境

| 工具 | 版本 | 路径 |
|------|------|------|
| ARM GCC | 13.3.1 | `D:\tools\arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi\` |
| GNU Make | 4.4.1 | `/usr/bin/make.exe` |
| OpenOCD | xPack 0.12.0-7 | `D:\tools\xpack-openocd-0.12.0-7\` |
| Host GCC | WinLibs 16.1.0 | `D:\tools\mingw64\mingw64\` |

环境变量已写入 `~/.bashrc`。

---

## 第三方库

| 库 | 来源 | 版本/Commit | 本地路径 |
|----|------|-----------|---------|
| GD32F4xx Firmware Library | `github.com/cjacker/gd32f4xx_firmware_library_gcc_makefile` | `3b5d2f2` | `src/bsp/gd32f4xx/` |
| FreeRTOS Kernel | `github.com/FreeRTOS/FreeRTOS-Kernel` | `d3b074a` | `src/middleware/freertos/` |
| LwIP | `github.com/lwip-tcpip/lwip` | `0c957ec` | `src/middleware/lwip/` |
| Unity Test | `github.com/ThrowTheSwitch/Unity` | `4fe7d60` | `tests/unity/` |

原始仓库备份于 `download/`。

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [docs/build_flash_guide.md](docs/build_flash_guide.md) | **编译 & 烧录 & 调试 操作手册** — make/flash/debug/test 日常命令和 GDB 速查 |
| [docs/toolchain_setup.md](docs/toolchain_setup.md) | 工具链安装指南 — 从零搭建 ARM GCC + OpenOCD + 主机 GCC |
| [docs/architecture.md](docs/architecture.md) | 软件架构设计 — 分层设计、模块职责、数据流、内存布局 |
| [docs/build_config.md](docs/build_config.md) | 编译配置参考 — 所有编译选项、源文件清单、链接脚本详解 |
| [docs/gd32_vs_stm32.md](docs/gd32_vs_stm32.md) | GD32/STM32 差异对照 — 外设命名、API 差异、移植陷阱 |
| [docs/bootloader_design.md](docs/bootloader_design.md) | Bootloader 设计 — 统一固件架构 + OTA 协议 + Flash 分区 |
| [docs/ethernet_guide.md](docs/ethernet_guide.md) | 以太网驱动指南 — ENET MAC/PHY/LwIP 移植全流程 |
| [docs/porting_guide.md](docs/porting_guide.md) | 新项目移植指南 — 改什么、不改什么、检查清单 |
| [docs/requirements.md](docs/requirements.md) | 需求文档 — 项目起源和核心决策 |
| [docs/development_plan.md](docs/development_plan.md) | 开发计划 — 5 个 Phase 迭代记录和验证方案 |
