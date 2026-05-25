# 软件架构设计

## 1. 设计哲学

### 1.1 核心原则

- **分层隔离**：每层只做自己该做的事，层间通过固定接口交互
- **依赖方向**：上层调用下层，下层绝不依赖上层
- **配置集中**：所有可调参数在 `config/` 目录，代码中不含魔法数字
- **官方库优先**：底层全部使用 GD32 官方 StdPeriph 库，不自己写寄存器操作
- **编译时决策**：PHY 模式、功能开关等通过宏在编译时确定，零运行时开销

### 1.2 为什么不用 CubeMX/Keil

| 问题 | 替代方案 |
|------|---------|
| Keil 收费、闭源、仅 Windows | ARM GCC（免费、开源、跨平台） |
| CubeMX 生成代码臃肿、耦合 | 手工分层架构，清晰可控 |
| HAL 库抽象过度、性能损失 | GD32 StdPeriph 库，接近寄存器、性能好 |
| MDK-ARM 项目管理复杂 | 一个 Makefile 搞定，支持并行编译 |

---

## 2. 分层设计

```
┌─────────────────────────────────────────────────────────┐
│ Layer 4: Application                                    │
│ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐  │
│ │ boot.c   │ │ ota.c    │ │udp_echo.c│ │ cli_cmds.c │  │
│ │ 启动检测 │ │OTA 升级  │ │UDP Demo  │ │ Shell 命令 │  │
│ └──────────┘ └──────────┘ └──────────┘ └────────────┘  │
├─────────────────────────────────────────────────────────┤
│ Layer 3: Middleware                                     │
│ ┌──────────┐ ┌──────────┐ ┌──────────┐                 │
│ │FreeRTOS  │ │LwIP      │ │Shell(cli)│                 │
│ │任务调度  │ │TCP/IP栈  │ │命令解析  │                 │
│ └──────────┘ └──────────┘ └──────────┘                 │
├─────────────────────────────────────────────────────────┤
│ Layer 2: Driver                                         │
│ ┌──────────┐ ┌──────────┐ ┌──────────┐                 │
│ │eth_mac   │ │phy_*     │ │uart_cons │                 │
│ │ENET MAC  │ │PHY 驱动  │ │串口控制台│                 │
│ ├──────────┤ ├──────────┤ ├──────────┤                 │
│ │flash_if  │ │systick_rt│ │ethernetif│                 │
│ │Flash操作 │ │系统时钟  │ │LwIP 移植 │                 │
│ └──────────┘ └──────────┘ └──────────┘                 │
├─────────────────────────────────────────────────────────┤
│ Layer 1: BSP/HAL                                        │
│ ┌──────────┐ ┌──────────┐ ┌──────────────┐             │
│ │board     │ │CMSIS     │ │StdPeriph     │             │
│ │板级初始化│ │Cortex-M4 │ │外设 HAL 驱动 │             │
│ └──────────┘ └──────────┘ └──────────────┘             │
├─────────────────────────────────────────────────────────┤
│ Layer 0: Hardware                                       │
│ GD32F450IK6 │ LAN8720A/DP83848 │ J-Link SWD            │
└─────────────────────────────────────────────────────────┘
```

### 2.1 Layer 1 — BSP/HAL 层

**职责**：封装寄存器操作，提供硬件抽象。全部使用 GD32 官方库代码，项目只做集成不做修改。

**文件清单**：

| 文件 | 说明 |
|------|------|
| `system_gd32f4xx.c` | 系统初始化：`SystemInit()` 设置 FPU、更新 `SystemCoreClock` |
| `startup_gd32f450_470.S` | 启动汇编：中断向量表、`Reset_Handler`、默认中断处理 |
| `core_cm4.h` | ARM Cortex-M4 CMSIS 核心定义 |
| `gd32f4xx.h` | 设备头文件：外设基址、中断号、寄存器结构体 |
| `gd32f4xx_rcu.c` | 复位与时钟控制（PLL、HXTAL、外设时钟使能） |
| `gd32f4xx_gpio.c` | GPIO 输入/输出/复用配置 |
| `gd32f4xx_enet.c` | 以太网 MAC 控制器（含 DMA 描述符、SMI、中断） |
| `gd32f4xx_usart.c` | 串口驱动 |
| `gd32f4xx_dma.c` | DMA 控制器 |
| `gd32f4xx_misc.c` | NVIC 中断优先级配置 |
| `gd32f4xx_syscfg.c` | 系统配置（GPIO AF 复用、ENET PHY 接口选择） |
| `gd32f4xx_exti.c` | 外部中断 |
| `gd32f4xx_fmc.c` | Flash 存储器控制器（页擦除/字编程） |
| `gd32f4xx_timer.c` | 通用定时器 |
| `gd32f4xx_pmu.c` | 电源管理（备份域写使能） |
| `board.c` | 项目级板级初始化：200MHz 时钟配置、外设 GPIO 初始化 |

**board.h 引脚映射设计**：

所有硬件引脚集中在 `board.h` 中，通过宏定义管理。换板子只需改这个文件：

```c
#define LED_PORT           GPIOC
#define LED_PIN            GPIO_PIN_6
#define BOOT_BUTTON_PORT   GPIOA
#define BOOT_BUTTON_PIN    GPIO_PIN_0
// ... 完整 RMII/MII 引脚映射
```

### 2.2 Layer 2 — Driver 层

**职责**：面向功能的外设驱动，提供统一 API。上层不需要知道寄存器细节。

#### 以太网子系统

```
phy.h (统一接口)
  ├── phy_lan8720.c  → RMII, PHY 地址 0x00
  └── phy_dp83848.c  → MII,  PHY 地址 0x01

eth_mac.c  → GD32 ENET MAC 驱动
  ├── DMA 描述符链: 5 TX + 5 RX 描述符
  ├── eth_mac_init(mode) → MAC + DMA + PHY 初始化
  ├── eth_mac_send(buf, len) → 发送帧（轮询 DAV 位）
  ├── eth_mac_recv(buf, len) → 接收帧（轮询 DAV 位）
  └── eth_mac_isr() → ENET 中断处理

ethernetif.c  → LwIP 移植层
  ├── ethernetif_init() → netif_add 回调
  ├── ethernetif_input() → 周期性收包 → 投喂 LwIP
  ├── low_level_output() → pbuf → 线性缓冲 → eth_mac_send()
  └── low_level_input() → eth_mac_recv() → pbuf
```

**数据流（接收方向）**：

```
PHY → RMII/MII → ENET MAC → DMA RX Descriptor → eth_mac_recv()
  → ethernetif_input() → pbuf → netif->input() → LwIP 协议栈
  → udp_echo_callback() → udp_sendto() → pbuf
  → low_level_output() → eth_mac_send() → DMA TX Descriptor → ENET MAC → PHY
```

#### 其他驱动

| 驱动 | API | 说明 |
|------|-----|------|
| `uart_console` | `uart_console_init(baud)`, `uart_console_putc/getc` | USART0 调试输出 |
| `flash_if` | `flash_read_buf/erase_sector/write_word` | 内部 Flash 操作 |
| `systick_rtos` | `systick_rtos_init()` | SysTick 中断 → FreeRTOS tick |

### 2.3 Layer 3 — Middleware 层

**职责**：第三方代码集成，不改动源码，只通过配置文件裁剪。

| 中间件 | 配置文件 | 裁剪要点 |
|--------|---------|---------|
| FreeRTOS | `config/FreeRTOSConfig.h` | 1KHz tick, 8 优先级, 64KB heap, heap_4, NO_STATIC_ALLOC |
| LwIP | `config/lwipopts.h` | UDP only, NO_SYS=0, 禁用 TCP/DHCP, MEM_SIZE=16KB |
| Shell (cli) | 代码级 | 命令注册表在 `cli_cmds.c` |

**FreeRTOS 任务设计**：

| 任务 | 优先级 | 栈大小 | 周期 | 功能 |
|------|--------|--------|------|------|
| `led_task` | idle+1 | 128 words | 500ms | LED 闪烁 |
| `cli_task` | idle+1 | 512 words | 10ms | 处理 UART 命令 |
| `udp_echo_task` | idle+2 | 1024 words | 10ms | 收包→回显 |
| Timer Task | max-1 | 256 words | 自动 | 软件定时器 |
| Idle Task | idle | 128 words | — | CPU 空闲 |

### 2.4 Layer 4 — Application 层

**职责**：FreeRTOS Task 级别的业务逻辑。

| 模块 | 入口 | 说明 |
|------|------|------|
| `boot.c` | `boot_get_mode()` | 启动模式检测：BKP 寄存器→GPIO→App 校验 |
| `ota.c` | `ota_bootloader_entry()` | OTA 固件升级：UDP 收包→CRC32→写 Flash |
| `udp_echo.c` | `udp_echo_task()` | UDP Echo demo: 收包→回显→闪灯 |
| `cli_cmds.c` | `cli_register_builtin_cmds()` | 5 个内置命令：help/info/eth/ota/reset |

---

## 3. 启动流程（完整链路）

```
硬件上电
  │
  ▼
0x08000000: 中断向量表
  │ .isr_vector[0] = _estack (栈顶: 0x20070000)
  │ .isr_vector[1] = Reset_Handler
  │
  ▼
Reset_Handler (startup_gd32f450_470.S)
  ├─ 将 .data 段从 Flash 复制到 RAM
  ├─ 清零 .bss 段
  ├─ 调用 SystemInit() (system_gd32f4xx.c)
  │    ├─ 使能 FPU (SCB->CPACR)
  │    ├─ 配置 Flash 等待周期
  │    └─ 设置 SystemCoreClock = 200000000
  └─ 调用 main()
  │
  ▼
main() (main.c)
  ├─ board_clock_init()  — HXTAL 25MHz → PLL → 200MHz SYSCLK
  │
  ├─ boot_get_mode()  → 启动模式判定
  │    ├─ BKP_DATA_0 == "OTAB" → BOOT_MODE_BOOTLOADER
  │    ├─ BOOT_BUTTON_PRESSED() → BOOT_MODE_BOOTLOADER
  │    ├─ !boot_is_app_valid()  → BOOT_MODE_BOOTLOADER
  │    └─ else                  → BOOT_MODE_APP
  │
  ├─ [Branch A: BOOT_MODE_BOOTLOADER]
  │    ├─ board_periph_init()  — 最小外设 (GPIO, UART)
  │    ├─ uart_console_init(115200)
  │    ├─ ota_bootloader_entry()
  │    │    ├─ ETH MAC + PHY + LwIP 初始化
  │    │    ├─ 绑定 UDP 端口 5001
  │    │    ├─ 等待 OTA 命令 (超时 5s)
  │    │    ├─ 接收固件 → CRC32 校验
  │    │    ├─ Flash 写入 OTA Buffer
  │    │    └─ boot_set_ota_flag() → NVIC_SystemReset()
  │    └─ 超时 → fall through 到 App 路径
  │
  └─ [Branch B: BOOT_MODE_APP]
       └─ app_entry()
            ├─ board_periph_init()  — 全外设初始化
            ├─ bsp_led_init()
            ├─ uart_console_init(115200)
            ├─ network_init()       — LwIP + netif + 静态 IP 192.168.1.100
            ├─ xTaskCreate(led_task)   → "led", prio=1, stack=128
            ├─ xTaskCreate(cli_task)   → "cli", prio=1, stack=512
            ├─ xTaskCreate(udp_echo)   → "udp_echo", prio=2, stack=1024
            └─ vTaskStartScheduler()   → 永不返回
```

---

## 4. 内存布局

### 4.1 物理内存

```
GD32F450IK6 内存映射:
┌─────────────────┐ 0x20070000 (SRAM 结束, 448KB)
│  .stack         │ ← 向下增长
│  .heap          │ ← 向上增长 (32KB 最小)
│  FreeRTOS heap  │ ← heap_4 管理区 (64KB)
│  .bss           │ ← 零初始化全局变量 (129KB)
│  .data          │ ← 已初始化全局变量 (2KB)
├─────────────────┤ 0x20000000 (SRAM 起始)
│     ...         │
├─────────────────┤ 0x10010000 (TCM 结束, 64KB)
│  .tcm_data      │ ← 关键数据 (可放 FreeRTOS 内核对象)
├─────────────────┤ 0x10000000 (TCM 起始)
│     ...         │
├─────────────────┤ 0x08100000 (Flash 结束, 1024KB)
│  Config         │ 0x080F0000 (64KB)
│  OTA Buffer     │ 0x08080000 (448KB)
│  Application    │ 0x08020000 (384KB)
│  Boot + Common  │ 0x08000000 (128KB) ← .text + .rodata + .isr_vector
└─────────────────┘
```

### 4.2 Flash 分区（逻辑分区，同一物理镜像）

| 区域 | 起始地址 | 大小 | 实际用途 |
|------|---------|------|---------|
| Boot + Common | 0x08000000 | 128KB | 中断向量表 + 公共代码 (.text/.rodata) |
| Application | 0x08020000 | 384KB | 应用逻辑（同一 .text 段内） |
| OTA Buffer | 0x08080000 | 448KB | OTA 升级时固件暂存区 |
| Config | 0x080F0000 | 64KB | 持久化配置参数（预留） |

**注意**：当前设计中 Boot + App 编译为同一 `.text` 段，物理上从 `0x08000000` 开始连续放置。Flash 分区地址在 `app_config.h` 中定义，OTA 模块使用这些地址进行擦除和写入。

---

## 5. OTA 升级流程

```
PC Tool                           GD32F450 (Bootloader)
  │                                     │
  │──── OTA REQ (总大小) ─────────────→ │ 收到请求
  │                                     │ 擦除 OTA Buffer
  │←─── OTA ACK (seq=0, OK) ────────── │
  │                                     │
  │──── OTA DATA (seq=1, 1024 bytes) ─→ │ 写入 OTA Buffer
  │←─── OTA ACK (seq=1, OK) ────────── │
  │                                     │
  │──── OTA DATA (seq=2, 1024 bytes) ─→ │
  │←─── OTA ACK (seq=2, OK) ────────── │
  │         ... (重复 N 次) ...          │
  │                                     │
  │──── OTA DONE (总大小, CRC32) ──────→ │
  │                                     │ CRC32 校验
  │                                     │ 从 OTA Buffer 复制到 App 区
  │                                     │ 设置 BKP 标志
  │                                     │ NVIC_SystemReset()
  │                                     │
  │←─── OTA ACK (完成) ─────────────── │
```

**协议格式（TLV over UDP, 端口 5001）**：

| 类型 | Magic (4B) | cmd (1B) | seq (4B) | size (2-4B) | payload |
|------|-----------|----------|----------|-------------|---------|
| REQ | "OTAQ" | — | — | total_size (4B) | — |
| ACK | "OTAA" | — | seq | status (1B) | — |
| DATA | "OTAD" | — | seq | chunk_size (2B) | firmware_chunk |
| DONE | "OTAO" | — | — | total_size (4B) | crc32 (4B) |

---

## 6. 配置系统设计

### 6.1 配置分层

```
config/
├── app_config.h       ← 应用级：功能开关、网络、Flash 分区
├── FreeRTOSConfig.h   ← 内核级：调度、内存、中断
├── lwipopts.h         ← 协议栈级：协议开关、缓冲池
└── board.h (src/bsp)  ← 硬件级：引脚映射、时钟频率
```

### 6.2 编译时决策

所有功能开关在 `app_config.h` 中用 `#define` 控制：

```c
#define ENABLE_SHELL     1   // → 编译 cli.c + cli_cmds.c, 创建 cli_task
#define ENABLE_UDP_DEMO  1   // → 编译 udp_echo.c, 创建 udp_echo_task
#define ENABLE_OTA       1   // → 编译 ota.c, 注册 ota 命令
#define ENABLE_LED_DEMO  1   // → 创建 led_task
#define LOG_LEVEL        LOG_LEVEL_INFO  // → 编译时过滤日志
```

`main.c` 中用 `#if` 条件编译，未启用的功能零代码、零 CPU 开销。

### 6.3 PHY 双模选择

`app_config.h` 中定义模式枚举：
```c
#define ETH_PHY_RMII  1
#define ETH_PHY_MII   2
#define ETH_PHY_MODE  ETH_PHY_RMII  // 默认
```

Makefile 中对应选择编译文件：
```makefile
ifeq ($(ETH_PHY_MODE),MII)
C_SRCS += $(SRC_DIR)/drivers/eth/phy_dp83848.c
else
C_SRCS += $(SRC_DIR)/drivers/eth/phy_lan8720.c
endif
```

命令行覆盖：`make ETH_PHY_MODE=MII`

---

## 7. 日志系统

四级日志，编译时可裁剪：

```c
#define LOG_LEVEL_NONE  0  // 关闭所有日志
#define LOG_LEVEL_ERROR 1  // 仅错误
#define LOG_LEVEL_WARN  2  // 错误 + 警告
#define LOG_LEVEL_INFO  3  // 错误 + 警告 + 信息
#define LOG_LEVEL_DEBUG 4  // 全部

// 使用
LOG_E("Error: %d", err);    // ERROR 级别
LOG_W("Warning message");   // WARN 级别
LOG_I("Info message");      // INFO 级别
LOG_D("Debug: 0x%08X", v);  // DEBUG 级别
```

输出走 `uart_console` 的 putc，最终从 USART0 TX (PA9) 发出。

---

## 8. 关键技术决策记录

| 决策 | 选择 | 理由 |
|------|------|------|
| RTOS | FreeRTOS (非 Zephyr) | 轻量 (<10KB ROM)、成熟稳定、GD32 官方支持 |
| 协议栈 | LwIP (非 uIP) | 功能完整、BSD socket API、社区活跃 |
| 构建 | Makefile (非 CMake only) | Make 零依赖、GCC 原生支持、GDB 集成好 |
| 入口 | 单 main.c 统一固件 | 免维护两套代码和链接脚本 |
| PHY API | 官方 `enet_phy_write_read()` | 不走自写 SMI 操作，用官方封装 |
| Flash API | 官方 `fmc_sector_erase()`/`fmc_word_program()` | 不自己写 Flash 时序 |
| 代码风格 | C11 + 英文注释 | 避免中文编码问题，国际化友好 |

---

## 9. 扩展指南

### 添加新任务

1. 在 `config/app_config.h` 中加 `#define ENABLE_YOUR_FEATURE 1`
2. 在 `src/app/` 下新建模块
3. 在 `main.c` 的 `app_entry()` 中用 `#if ENABLE_YOUR_FEATURE` 创建任务
4. 在 Makefile 中添加源文件

### 添加新 CLI 命令

1. 在 `cli_cmds.c` 中写 handler 函数
2. 在 `builtin_cmds[]` 数组中添加 `{ "name", "help", handler }` 条目

### 换 PHY 芯片

1. 在 `src/drivers/eth/` 下新建 `phy_xxx.c`，实现 `phy.h` 接口
2. 在 Makefile 中添加编译条件
3. 在 `app_config.h` 中定义新模式枚举值

### 移植到新 MCU

参考 [docs/porting_guide.md](docs/porting_guide.md)
