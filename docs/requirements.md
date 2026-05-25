# GD32F450 FreeRTOS 嵌入式项目 — 优化需求文档

## 1. 项目概述

| 维度 | 决策 |
|------|------|
| **MCU** | GD32F450IK6 (Cortex-M4F, 200MHz, 512KB SRAM, up to 3072KB Flash) |
| **OS** | FreeRTOS V10.6.2 (LTS) |
| **语言** | C11 |
| **工具链** | ARM GNU Toolchain (arm-none-eabi-gcc 12.3+) + OpenOCD v0.12+ |
| **调试器** | J-Link (SWD) |
| **构建系统** | Makefile + CMake 双套 |
| **以太网** | 10/100Mbps, RMII + MII 双接口, LwIP 协议栈, UDP 通信 |
| **PHY 芯片** | RMII: LAN8720A, MII: DP83848 (主流稳定型号，驱动均实现) |
| **入口设计** | 单 main.c 入口，bootloader 与 app 同一代码库，编译为同一固件镜像 |

---

## 2. 核心架构决策

### 2.1 统一固件架构（Bootloader + App 单 main.c）

**设计思路**：bootloader 和 app 不是两套独立固件，而是同一固件镜像中的两条代码路径。

```
Reset → startup.s → SystemInit() → main()
                                      ├─ boot_get_mode()
                                      │    ├─ RTC BKP 寄存器检测（最高优先）
                                      │    ├─ GPIO 引脚检测（硬件按键触发）
                                      │    └─ App 有效性检测（CRC/签名校验）
                                      │
                                      ├─ [Bootloader 路径]
                                      │    └─ bootloader_entry()
                                      │         ├─ 初始化最小外设（ETH + UART）
                                      │         ├─ 等待 UDP OTA 升级命令（超时 5s）
                                      │         ├─ 接收+校验+写入新固件
                                      │         ├─ 置位标志 → NVIC_SystemReset()
                                      │
                                      └─ [App 路径]
                                           └─ app_entry()
                                                ├─ 全外设初始化
                                                ├─ FreeRTOS 任务创建
                                                └─ vTaskStartScheduler()
```

**优势**：
- 无需维护两套 startup、链接脚本、HAL 驱动
- Flash 分区简单，无 VTOR 重映射复杂度
- OTA 时 bootloader 逻辑天然能使用所有 HAL/驱动层能力
- 调试统一，一套断点覆盖全部代码路径

**Flash 分区**（总 3MB，按 1MB 预算）：

| 区域 | 地址范围 | 大小 | 用途 |
|------|---------|------|------|
| Boot + Common | 0x08000000 - 0x0801FFFF | 128KB | 中断向量表 + 公共代码 + bootloader |
| Application | 0x08020000 - 0x0807FFFF | 384KB | 应用逻辑 |
| OTA Buffer | 0x08080000 - 0x080EFFFF | 448KB | OTA 固件暂存区 |
| Config/Data | 0x080F0000 - 0x080FFFFF | 64KB | 持久化配置 + 参数存储 |

### 2.2 分层软件架构

```
┌──────────────────────────────────────────────┐
│              Application Layer               │
│  udp_demo_task  │  ota_task  │  monitor_task │
├──────────────────────────────────────────────┤
│              Middleware Layer                 │
│  FreeRTOS  │  LwIP 2.1  │  Shell  │  OTA Lib │
├──────────────────────────────────────────────┤
│              Driver Layer                    │
│  ETH MAC  │  PHY (LAN8720/DP83848)          │
│  UART     │  Flash      │  GPIO  │  Timer   │
├──────────────────────────────────────────────┤
│              BSP / HAL Layer                 │
│  GD32F4xx StdPeriphLib  │  CMSIS (Core+M4)  │
├──────────────────────────────────────────────┤
│              Hardware Layer                  │
│  GD32F450IK6  │  PHY Chip  │  J-Link (SWD)  │
└──────────────────────────────────────────────┘
```

**层级职责**：
- **BSP/HAL**：仅封装寄存器操作，不包含业务逻辑。使用 GD32 官方标准外设库。
- **Driver**：面向功能的外设驱动（eth_init、flash_erase、uart_send），统一接口，交换 PHY 芯片仅需换实现文件。
- **Middleware**：第三方组件，通过配置文件裁剪。LwIP 通过 `lwipopts.h` 配置。
- **Application**：FreeRTOS Task 级别的业务逻辑，不同 demo/产品通过替换此层实现。

### 2.3 去 Keil 化工具链方案

| 环节 | 工具 | 说明 |
|------|------|------|
| 编译 | arm-none-eabi-gcc | GNU ARM 工具链, 从 [ARM 官网](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain) 获取 |
| 构建 | Makefile / CMake 各一套 | 两套独立维护，功能等价 |
| 链接 | gd32f450.ld | 自定义链接脚本，按 Flash 分区定义 |
| 烧录 | OpenOCD + J-Link | `openocd -f interface/jlink.cfg -f target/gd32f4x.cfg -c "program firmware.hex verify reset"` |
| 调试 | OpenOCD + GDB | `arm-none-eabi-gdb -x scripts/gdb/gdbinit` |
| 格式化 | clang-format | Google C style, 通过 `.clang-format` 配置 |
| 静态分析 | cppcheck | CI 可集成 |

---

## 3. 目录结构

```
gd32f450_freertos/
├── build/                          # Build output (gitignore)
├── cmake/
│   └── toolchain.cmake             # ARM GCC toolchain definition
├── config/
│   ├── FreeRTOSConfig.h            # FreeRTOS kernel configuration
│   ├── lwipopts.h                  # LwIP stack configuration
│   ├── app_config.h                # Application-level feature switches
│   └── clang-format                # Code style config (Google C)
├── docs/
│   └── requirements.md             # This document
├── linker/
│   └── gd32f450_flash.ld           # Flash linker script
├── scripts/
│   ├── openocd/
│   │   ├── gd32f4x_jlink.cfg       # OpenOCD: J-Link + GD32F4x
│   │   ├── gd32f4x_daplink.cfg     # OpenOCD: CMSIS-DAP 备选
│   │   ├── flash_program.tcl       # Flash programming script
│   │   └── flash_erase.tcl         # Mass erase script
│   └── gdb/
│       └── gdbinit                 # GDB init: connect to OpenOCD
├── src/
│   ├── main.c                      # ★ 唯一入口 ★
│   ├── app/                        # Application Layer
│   │   ├── boot/
│   │   │   ├── boot.c              # Boot mode detection + bootloader flow
│   │   │   └── boot.h
│   │   ├── ota/
│   │   │   ├── ota.c               # OTA firmware update logic
│   │   │   └── ota.h
│   │   └── demo/
│   │       ├── udp_echo.c          # UDP Echo demo task
│   │       └── udp_echo.h
│   ├── bsp/                        # Board Support Package
│   │   ├── board.c                 # Clock tree, GPIO pinout, board-level init
│   │   ├── board.h
│   │   ├── bsp_led.c/.h            # On-board LED abstraction
│   │   ├── bsp_button.c/.h         # On-board button abstraction
│   │   └── gd32f4xx/               # GD32 standard peripheral library
│   │       ├── CMSIS/              # CMSIS Core + Device headers
│   │       └── StdPeriph/          # GD32F4xx_StdPeriph_Driver
│   ├── drivers/                    # Hardware Drivers
│   │   ├── eth/
│   │   │   ├── eth_mac.c/.h        # GD32 ENET MAC driver
│   │   │   ├── phy.h               # PHY common interface
│   │   │   ├── phy_lan8720.c       # LAN8720 RMII implementation
│   │   │   └── phy_dp83848.c       # DP83848 MII implementation
│   │   ├── uart/
│   │   │   └── uart_console.c/.h   # Debug console (USART0)
│   │   ├── flash/
│   │   │   └── flash_if.c/.h       # Internal flash read/write/erase
│   │   └── timer/
│   │       └── systick_rtos.c/.h   # SysTick for FreeRTOS tick
│   ├── middleware/                  # Third-party Middleware
│   │   ├── freertos/               # FreeRTOS source (kernel + portable)
│   │   ├── lwip/                   # LwIP source (core + netif + api)
│   │   └── shell/                  # Lightweight UART command shell
│   │       ├── cli.c/.h
│   │       └── cli_cmds.c          # Built-in commands (help, info, reset)
│   └── utils/                      # Common Utilities
│       ├── log.c/.h                # Logging (UART output, level-filtered)
│       ├── assert.c/.h             # Custom assert (logs + halts)
│       ├── ringbuf.c/.h            # Lock-free ring buffer
│       └── crc32.c/.h              # CRC32 for firmware verification
├── CMakeLists.txt                  # CMake build entry
├── Makefile                        # Make build entry
├── .gitignore
└── README.md
```

---

## 4. 关键模块设计要点

### 4.1 以太网驱动（RMII + MII 双模）

```
phy.h → 统一 PHY 操作接口
  ├── phy_lan8720.c  (RMII 模式, 地址 0x00, 自动协商)
  └── phy_dp83848.c  (MII 模式,  地址 0x01, 自动协商)

eth_mac.c → GD32 ENET MAC 驱动
  ├── eth_mac_init(mode)      → 初始化 MAC + DMA 描述符
  ├── eth_mac_send(buf, len)  → 发送以太网帧
  └── eth_mac_recv(buf, len)  → 接收以太网帧（中断驱动）
```

- 通过 `app_config.h` 中的 `#define ETH_PHY_MODE ETH_PHY_RMII` 编译时选择模式
- PHY 自动协商 100Mbps 全双工
- 使用 DMA 描述符链实现零拷贝收发
- 中断通知 LwIP 收包，FreeRTOS 信号量同步

### 4.2 LwIP 集成

- **移植层**：实现 `ethernetif.c`（参考 LwIP contrib），对接 eth_mac 驱动
- **网络参数**：
  - 静态 IP: 192.168.1.100（可通过 CLI 修改）
  - 子网掩码: 255.255.255.0
  - 网关: 192.168.1.1
- **线程模型**：LwIP 在专用 `lwip_thread` 中运行（优先级高于 app task）
- **内存池**：根据 UDP demo 需求调整 `lwipopts.h` 中的 MEM_SIZE、PBUF_POOL_SIZE

### 4.3 UDP Echo Demo

```
FreeRTOS Task: udp_echo_task (prio: tskIDLE_PRIORITY+2, stack: 1024)
  └─ socket(AF_INET, SOCK_DGRAM, 0)
  └─ bind(port: 5000)
  └─ loop:
       ├─ recvfrom(buf, 1500, ...)  // block with 200ms timeout
       ├─ On RX:
       │   ├─ log("UDP rx: %d bytes from %s:%d", len, ip, port)
       │   ├─ sendto(same buf, same addr)
       │   └─ bsp_led_toggle(LED_GREEN)
       └─ On timeout:
           └─ feed watchdog
```

### 4.4 Bootloader 流程

```
bootloader_entry():
  1. Init ETH + PHY (minimal)
  2. Init LwIP (UDP only, no TCP)
  3. Log: "Bootloader mode, waiting for OTA..."
  4. Enter receive window (5 seconds)
     ├─ Exchange protocol (simple TLV):
     │   REQ:  "OTAQ" | cmd(1B) | seq(4B) | size(4B)
     │   ACK:  "OTAA" | seq(4B) | status(1B)
     │   DATA: "OTAD" | seq(4B) | size(2B) | payload(N)
     │   DONE: "OTAO" | total_size(4B) | crc32(4B)
     ├─ Erase app area + OTA buffer
     ├─ Write received chunks to OTA buffer
     ├─ Verify CRC32
     ├─ Copy from OTA buffer to app area
     └─ BKP_REG_WRITE(BOOT_OK_FLAG) → NVIC_SystemReset()
  5. On timeout → fall through to app (if valid)
```

### 4.5 调试命令行 Shell

通过 UART0 (115200-8-N-1) 提供交互式命令行：

```
gd32> help
  info    - System information
  eth     - Ethernet status
  ota     - Enter OTA mode
  reset   - System reset
  ping    - Ping host

gd32> info
  MCU: GD32F450IK6 @ 200MHz
  RAM: 256KB used / 512KB total
  Flash: 128KB used / 1024KB total
  FreeRTOS: 5 tasks
  ETH: 100Mbps Full-Duplex

gd32> ota
  Entering OTA mode...
  [OTA] Waiting for firmware update on UDP port 5001...
```

---

## 5. 实现路线图（迭代顺序）

### Phase 1: 骨架搭建（最小可运行）
1. 创建目录结构
2. 集成 GD32F4xx 标准外设库 + CMSIS
3. 编写 `gd32f450_flash.ld` 链接脚本
4. 编写 Makefile + CMakeLists.txt（编译 + OpenOCD 烧录 + GDB 调试 目标）
5. `board.c` — 时钟配置（200MHz PLL）+ GPIO LED 初始化
6. `main.c` — LED 闪烁 → 验证工具链全链路

### Phase 2: FreeRTOS 集成
1. 移植 FreeRTOS（内核 + Cortex-M4F port + SysTick）
2. `FreeRTOSConfig.h` 配置
3. 创建 LED blink task → 验证 RTOS 调度

### Phase 3: 以太网驱动
1. `eth_mac.c` — ENET MAC 驱动（DMA 描述符 + 中断）
2. `phy_lan8720.c` — RMII PHY 驱动
3. `phy_dp83848.c` — MII PHY 驱动
4. 以太网帧收发 loopback 测试

### Phase 4: LwIP + UDP Demo
1. 移植 LwIP + `ethernetif.c`
2. `lwipopts.h` 配置
3. UDP echo 任务 → PC 端用 `nc -u` 或 Python 脚本验证

### Phase 5: Bootloader + Shell
1. `boot.c` — 启动模式检测逻辑
2. `ota.c` — OTA 协议实现
3. `shell/` — UART 命令行
4. 端到端 OTA 流程验证

---

## 6. 验证方案

| 阶段 | 验证方式 | 通过标准 |
|------|---------|---------|
| Phase 1 | `make flash` 烧录 → LED 按 500ms 间隔闪烁 | LED 正常闪烁 |
| Phase 2 | GDB 连接 → 断点观察 task 切换 → `vTaskList()` 输出 | 5 个 task 正常运行 |
| Phase 3 | Wireshark 抓包 → 验证以太网帧格式 | 抓包正确 |
| Phase 4 | PC `echo "hello" \| nc -u 192.168.1.100 5000` → 收到回复 | 双向通信 OK |
| Phase 5 | 触发 OTA → 烧录新固件 → reset → 新固件运行 | OTA 成功 |

---

## 7. 配置管理

关键配置项集中定义，避免魔法数字分散：

| 配置文件 | 管理内容 |
|---------|---------|
| `app_config.h` | 功能开关（ETH_PHY_MODE、ENABLE_SHELL、LOG_LEVEL、OTA_TIMEOUT_MS） |
| `board.h` | 硬件 pinout（LED_PIN、BUTTON_PIN、PHY_RESET_PIN、BOOT_PIN） |
| `FreeRTOSConfig.h` | RTOS 参数（TICK_RATE_HZ、MINIMAL_STACK_SIZE、TOTAL_HEAP_SIZE） |
| `lwipopts.h` | LwIP 参数（MEM_SIZE、PBUF_POOL_SIZE、TCP/UDP/ICMP 开关） |
| `gd32f450_flash.ld` | 内存映射（FLASH_ORIGIN、SRAM_ORIGIN、OTA_BUFFER_ORIGIN） |
