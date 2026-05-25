# GD32F450 FreeRTOS — 开发计划

> 基于 `docs/requirements.md`，按 5 个 Phase 迭代实施。每个 Phase 有独立的可验证输出。

---

## Phase 1: 骨架搭建（最小可运行系统）

**目标**：工具链全链路跑通 — 编译 → 链接 → 烧录 → LED 闪烁。

### 1.1 创建目录结构

一次性创建全部目录，即使前期为空目录，保证后续代码落位一致。

```
mkdir -p build cmake config docs linker scripts/openocd scripts/gdb
mkdir -p src/app/boot src/app/ota src/app/demo
mkdir -p src/bsp/gd32f4xx/CMSIS src/bsp/gd32f4xx/StdPeriph
mkdir -p src/drivers/eth src/drivers/uart src/drivers/flash src/drivers/timer
mkdir -p src/middleware/freertos src/middleware/lwip src/middleware/shell
mkdir -p src/utils
```

### 1.2 集成 GD32F4xx 标准外设库 + CMSIS

**来源**：从 GigaDevice 官网或 GD32F4xx SDK 包获取以下文件。

**CMSIS 层**（`src/bsp/gd32f4xx/CMSIS/`）：

| 文件 | 说明 |
|------|------|
| `core_cm4.h` | ARM Cortex-M4 内核头文件 |
| `core_cmFunc.h` | CMSIS 内联函数 |
| `core_cmInstr.h` | CMSIS 指令 |
| `core_cmSimd.h` | SIMD 指令 |
| `gd32f4xx.h` | GD32F450 寄存器定义 + 外设基址 |
| `system_gd32f4xx.c` | 系统初始化（时钟默认 200MHz） |
| `system_gd32f4xx.h` | SystemInit() 声明 |
| `startup_gd32f4xx.s` | 启动汇编（向量表 + Reset_Handler） |

**StdPeriph 层**（`src/bsp/gd32f4xx/StdPeriph/`）：

Phase 1 最小集合，后续按需添加：

| 文件 | 用途 |
|------|------|
| `gd32f4xx_rcu.c/.h` | 时钟控制（必须） |
| `gd32f4xx_gpio.c/.h` | GPIO 控制（必须） |

### 1.3 编写链接脚本

**文件**：`linker/gd32f450_flash.ld`

```ld
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
}

_stack_end = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text : { *(.text*) } > FLASH
    .rodata : { *(.rodata*) } > FLASH
    .data : { *(.data*) } > SRAM AT > FLASH
    .bss : { *(.bss*) } > SRAM
}
```

### 1.4 编写 Makefile

**文件**：`Makefile`

需要实现的目标：

| Target | 功能 |
|--------|------|
| `all` (default) | 编译 + 链接 → `build/firmware.elf` + `build/firmware.hex` + `build/firmware.bin` |
| `flash` | 通过 OpenOCD 烧录 hex 到 Flash |
| `erase` | 通过 OpenOCD 擦除芯片 |
| `debug` | 启动 OpenOCD 服务 + GDB 连接 |
| `clean` | 删除 `build/` |

关键编译标志：

```makefile
CC       = arm-none-eabi-gcc
CFLAGS   = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS  += -Os -Wall -Wextra -ffunction-sections -fdata-sections
CFLAGS  += -DSTM32F40_41xxx  # 兼容 STM32 生态库的宏
LDFLAGS  = -T linker/gd32f450_flash.ld -specs=nosys.specs
LDFLAGS += -Wl,--gc-sections -Wl,-Map=build/firmware.map
```

### 1.5 编写 CMakeLists.txt

**文件**：`CMakeLists.txt` + `cmake/toolchain.cmake`

功能与 Makefile 等价，额外支持 IDE（CLion/VSCode）的 IntelliSense。

### 1.6 编写 board.c / board.h

**文件**：`src/bsp/board.c`、`src/bsp/board.h`

```c
// board.h
#pragma once
#include "gd32f4xx.h"

void board_clock_init(void);  // 200MHz PLL from HSE 25MHz
void board_led_init(void);
void board_led_on(void);
void board_led_off(void);
void board_led_toggle(void);
void board_delay_ms(uint32_t ms);
```

```c
// board.c — 关键实现逻辑
void board_clock_init(void)
{
    // 1. HSE 25MHz enable
    // 2. PLL = HSE * N / P = 25 * 400 / 2 / 4 = 200MHz (PLLP=4 for 50MHz)
    //    Actually: PLL = HSE / M * N / P, for 200MHz:
    //    M=25, N=400, P=2 → VCO=400MHz, SYS=200MHz
    //    Wait: M=25 → 1MHz input, N=400 → 400MHz VCO, P=2 → 200MHz SYS
    //    Or: M=12.5 not possible... Let me recalculate.
    //    HSE=25MHz, PLL=HSE*N/(M*P), need SYS=200MHz
    //    M=25, N=400, P=2: PLL = 25*400/(25*2) = 200MHz ✓
    // 3. Flash latency = 5 wait states (for 200MHz)
    // 4. AHB=200MHz, APB1=50MHz, APB2=100MHz
}
```

### 1.7 编写 main.c

**文件**：`src/main.c`

```c
#include "board.h"

int main(void)
{
    board_clock_init();
    board_led_init();

    while (1) {
        board_led_toggle();
        board_delay_ms(500);
    }
}
```

### 1.8 编写 OpenOCD 脚本

**文件**：`scripts/openocd/gd32f4x_jlink.cfg`

```tcl
# J-Link + GD32F4xx via SWD
source [find interface/jlink.cfg]
transport select swd
source [find target/stm32f4x.cfg]  # GD32F4xx 与 STM32F4xx SWD 兼容
adapter speed 4000
```

### 1.9 .gitignore

```gitignore
build/
*.o
*.elf
*.hex
*.bin
*.map
```

### Phase 1 验证

```bash
make all && make flash
# 预期：LED 以 500ms 周期闪烁
```

---

## Phase 2: FreeRTOS 集成

**目标**：RTOS 内核跑起来，多任务调度正常。

### 2.1 移植 FreeRTOS 源码

**来源**：从 [FreeRTOS GitHub](https://github.com/FreeRTOS/FreeRTOS-Kernel) 获取 V10.6.2。

复制到 `src/middleware/freertos/`：

```
freertos/
├── tasks.c
├── queue.c
├── list.c
├── timers.c
├── event_groups.c
├── stream_buffer.c
├── heap_4.c
├── include/
│   ├── FreeRTOS.h
│   ├── task.h
│   ├── queue.h
│   ├── semphr.h
│   ├── timers.h
│   ├── event_groups.h
│   └── stream_buffer.h
└── portable/
    └── GCC/
        └── ARM_CM4F/
            ├── port.c
            ├── portmacro.h
            └── portasm.s  (if needed, or compiled from port.c)
```

### 2.2 编写 FreeRTOSConfig.h

**文件**：`config/FreeRTOSConfig.h`

关键配置：

```c
#define configCPU_CLOCK_HZ                  (200000000UL)
#define configTICK_RATE_HZ                  (1000)
#define configTOTAL_HEAP_SIZE               (64 * 1024)
#define configMINIMAL_STACK_SIZE            (128)
#define configMAX_PRIORITIES                (8)
#define configUSE_PREEMPTION                1
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0
#define configUSE_MUTEXES                   1
#define configUSE_COUNTING_SEMAPHORES       1
#define configUSE_TIMERS                    1
#define configTIMER_TASK_PRIORITY           (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH            10
#define configTIMER_TASK_STACK_DEPTH        256
#define configCHECK_FOR_STACK_OVERFLOW      2
```

### 2.3 SysTick 适配

**文件**：`src/drivers/timer/systick_rtos.c`、`systick_rtos.h`

```c
// 用 SysTick 提供 FreeRTOS 的 tick
// SysTick_Handler() 调用 xPortSysTickHandler()
// vPortSetupTimerInterrupt() 配置 SysTick 为 configTICK_RATE_HZ
```

### 2.4 更新 main.c 为 RTOS 版本

**文件**：`src/main.c`（覆盖 Phase 1 的裸机版本）

```c
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        board_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    board_clock_init();
    board_led_init();

    xTaskCreate(led_task, "led", 128, NULL, tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();

    // Should never reach here
    while (1);
}
```

### 2.5 更新 Makefile / CMakeLists.txt

- 添加 FreeRTOS 源文件和 include 路径
- 添加 `config/` 到 include 路径

### Phase 2 验证

```bash
make all && make flash
make debug
# GDB 中：break led_task, continue, info threads
# 预期：LED 闪烁，task 正常调度
```

---

## Phase 3: 以太网驱动

**目标**：MAC + PHY 驱动就绪，能收发原始以太网帧。

### 3.1 添加 StdPeriph 文件

新增 GD32 标准外设库文件到 `src/bsp/gd32f4xx/StdPeriph/`：

| 文件 | 用途 |
|------|------|
| `gd32f4xx_enet.c/.h` | 以太网 MAC 控制器 |
| `gd32f4xx_dma.c/.h` | DMA 控制器 |
| `gd32f4xx_exti.c/.h` | 外部中断（可选） |
| `gd32f4xx_syscfg.c/.h` | 系统配置 |
| `gd32f4xx_misc.c/.h` | NVIC 配置 |

### 3.2 编写 PHY 通用接口

**文件**：`src/drivers/eth/phy.h`

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PHY_MODE_RMII,
    PHY_MODE_MII,
} phy_mode_t;

typedef struct {
    bool     link_up;
    uint8_t  speed;     // 10 or 100
    uint8_t  duplex;    // 0=half, 1=full
} phy_status_t;

bool phy_init(phy_mode_t mode);
bool phy_get_status(phy_status_t *status);
void phy_reset(void);
```

### 3.3 编写 PHY 驱动实现

**文件**：`src/drivers/eth/phy_lan8720.c` (RMII)、`phy_dp83848.c` (MII)

每个实现文件完成：
1. PHY 复位（GPIO 引脚拉低/拉高）
2. MDIO 读写（通过 ENET MAC 的 MDIO 接口）
3. 读取 PHY 寄存器确认 ID
4. 使能自动协商
5. 等待 Link Up
6. 读取协商结果（速度/双工）

### 3.4 编写 ENET MAC 驱动

**文件**：`src/drivers/eth/eth_mac.c`、`eth_mac.h`

```c
#pragma once
#include "phy.h"

bool eth_mac_init(phy_mode_t mode);
bool eth_mac_send(const uint8_t *data, uint16_t len);
uint16_t eth_mac_recv(uint8_t *buf, uint16_t buf_size);
void eth_mac_isr(void);  // Called from ENET interrupt handler
```

核心实现点：
1. GPIO 引脚配置（RMII: 9 pins, MII: 17 pins）
2. RCU 时钟使能（ENET + GPIO + SYSCFG）
3. ENET MAC 配置（100Mbps full-duplex）
4. DMA 描述符链初始化（TX ring + RX ring，各 4 个描述符）
5. 中断配置（接收中断 + 错误中断）
6. 发送：填入 TX descriptor → 启动 DMA
7. 接收：检查 RX descriptor ownership → 拷贝数据 → 归还 descriptor

### 3.5 更新 board.c / board.h

添加 PHY 相关引脚定义（PHY_RESET_PIN、PHY_MDC_PIN、PHY_MDIO_PIN 等）。

### 3.6 更新 main.c — 以太网回环测试

```c
// Phase 3 test: raw Ethernet frame echo (no LwIP yet)
static void eth_test_task(void *pvParameters)
{
    eth_mac_init(ETH_PHY_RMII);

    uint8_t buf[1518];
    while (1) {
        uint16_t len = eth_mac_recv(buf, sizeof(buf));
        if (len > 0) {
            eth_mac_send(buf, len);  // Echo back
            board_led_toggle();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### Phase 3 验证

- 用 PC 发送原始以太网帧（Python + raw socket / Scapy）
- Wireshark 抓包确认帧被回传
- 验证 100Mbps full-duplex 协商成功

---

## Phase 4: LwIP + UDP Demo

**目标**：UDP Echo 服务运行，PC 端能 ping 通并能做 UDP 收发。

### 4.1 移植 LwIP 源码

**来源**：从 [LwIP 官网](https://savannah.nongnu.org/projects/lwip/) 获取 V2.1.2。

复制到 `src/middleware/lwip/`：

```
lwip/
├── core/
│   ├── init.c, def.c, dns.c, mem.c, memp.c, netif.c
│   ├── pbuf.c, raw.c, stats.c, sys.c, tcp.c, tcp_in.c
│   ├── tcp_out.c, timeouts.c, udp.c
│   ├── ipv4/
│   │   ├── ip4.c, ip4_addr.c, ip4_frag.c
│   │   ├── autoip.c, dhcp.c, etharp.c, icmp.c, igmp.c
│   └── ipv6/  (skip for Phase 4)
├── api/
│   ├── api_lib.c, api_msg.c, err.c, if_api.c
│   ├── netbuf.c, netdb.c, netifapi.c
│   ├── sockets.c, tcpip.c
├── netif/
│   └── ethernet.c  (Ethernet netif helpers)
└── include/
    └── lwip/  (all LwIP headers)
```

### 4.2 编写 lwipopts.h

**文件**：`config/lwipopts.h`

UDP-only 最小配置：

```c
#define NO_SYS                          0   // Use OS (FreeRTOS)
#define LWIP_SOCKET                     1
#define LWIP_NETCONN                    0
#define LWIP_RAW                        0

#define LWIP_UDP                        1
#define LWIP_TCP                        0
#define LWIP_ICMP                       1
#define LWIP_IGMP                       0
#define LWIP_DHCP                       0
#define LWIP_DNS                        0
#define LWIP_AUTOIP                     0

#define MEM_SIZE                        (16 * 1024)
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                8
#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               1536

#define LWIP_STATS                      0
#define LWIP_STATS_DISPLAY              0
#define LWIP_DEBUG                      0

#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_CHECK_UDP              1
```

### 4.3 编写 ethernetif.c（LwIP 移植层）

**文件**：`src/drivers/eth/ethernetif.c`

这是 LwIP 与 eth_mac 驱动之间的胶水层：

```c
err_t ethernetif_init(struct netif *netif);
void ethernetif_input(struct netif *netif);  // Called from eth_mac ISR or task
```

核心逻辑：
1. `low_level_init()` — 调用 `eth_mac_init()` + `phy_init()`
2. `low_level_output()` — 调用 `eth_mac_send()`
3. `low_level_input()` — 调用 `eth_mac_recv()` → 封装为 `pbuf`
4. 使用 FreeRTOS 信号量通知收包
5. 在专用 `lwip_thread` 中调用 `ethernetif_input()` 处理接收

### 4.4 编写 UDP Echo 任务

**文件**：`src/app/demo/udp_echo.c`、`udp_echo.h`

```c
void udp_echo_task(void *pvParameters)
{
    struct sockaddr_in addr, client_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = htonl(IPADDR_ANY);
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));

    uint8_t buf[1500];
    while (1) {
        socklen_t client_len = sizeof(client_addr);
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&client_addr, &client_len);
        if (len > 0) {
            sendto(sock, buf, len, 0,
                   (struct sockaddr *)&client_addr, client_len);
            board_led_toggle();
        }
    }
}
```

### 4.5 更新 main.c

```c
int main(void)
{
    board_clock_init();
    board_led_init();

    // Init Ethernet (MAC + PHY + LwIP + netif)
    network_init();  // New: encapsulate all ETH init

    // Create tasks
    xTaskCreate(led_task,         "led",     128, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(lwip_thread,      "lwip",    512, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(udp_echo_task,    "udp",     1024, NULL, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();
    while (1);
}
```

### Phase 4 验证

```bash
# PC 端验证
ping 192.168.1.100                          # ICMP 应通
echo "hello gd32" | nc -u 192.168.1.100 5000  # UDP echo 应收到回复
```

---

## Phase 5: Bootloader + Shell

**目标**：完整的 OTA 升级链路 + 调试命令行。

### 5.1 编写 boot.c / boot.h

**文件**：`src/app/boot/boot.c`、`boot.h`

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BOOT_MODE_APP = 0,
    BOOT_MODE_BOOTLOADER,
} boot_mode_t;

boot_mode_t boot_get_mode(void);
void boot_set_ota_flag(void);
void boot_clear_ota_flag(void);
bool boot_is_app_valid(void);
```

**启动模式检测优先级**：

1. RTC BKP 寄存器 `BOOT_FLAG_REG` == `BOOT_FLAG_MAGIC` → Bootloader 模式（最高优先，用于 OTA 后的首次启动）
2. GPIO 启动引脚（如 PA0）低电平 → Bootloader 模式（硬件强制）
3. App 区域 CRC32 校验失败 → Bootloader 模式（固件损坏保护）
4. 以上皆不满足 → App 模式

### 5.2 编写 ota.c / ota.h

**文件**：`src/app/ota/ota.c`、`ota.h`

```c
#pragma once

// OTA protocol constants
#define OTA_MAGIC_REQ   0x4F544151  // "OTAQ"
#define OTA_MAGIC_ACK   0x4F544141  // "OTAA"
#define OTA_MAGIC_DATA  0x4F544144  // "OTAD"
#define OTA_MAGIC_DONE  0x4F54414F  // "OTAO"
#define OTA_UDP_PORT    5001

void ota_bootloader_entry(void);
```

**OTA 流程**：
1. 初始化 ETH + PHY + LwIP（仅 UDP，无需 TCP）
2. 绑定 UDP 端口 5001
3. 5 秒超时窗口内等待 PC 端 OTA 工具连接
4. 收到升级请求 → 擦除 OTA Buffer 区（0x08080000，448KB）
5. 分片接收固件数据（每片最大 1024 bytes），写入 OTA Buffer
6. 接收完成 → 校验 CRC32
7. 擦除 App 区（0x08020000，384KB）
8. 从 OTA Buffer 复制到 App 区
9. 写入启动标志 → `NVIC_SystemReset()`

### 5.3 编写 flash_if.c / flash_if.h

**文件**：`src/drivers/flash/flash_if.c`、`flash_if.h`

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

bool flash_erase_sector(uint32_t addr);
bool flash_erase_range(uint32_t start_addr, uint32_t size);
bool flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
bool flash_read(uint32_t addr, uint8_t *buf, uint32_t len);
```

GD32F450 Flash 特性：
- 页大小：16KB/32KB/64KB/128KB（取决于扇区号，前 4 个 16KB，然后 1 个 64KB，其余 128KB）
- 写入粒度：32-bit word
- 必须按扇区擦除
- 编程前需解锁 Flash 控制寄存器

### 5.4 编写 Shell 命令行

**文件**：`src/middleware/shell/cli.c`、`cli.h`

```c
#pragma once

typedef struct {
    const char *name;
    const char *help;
    void (*handler)(int argc, char *argv[]);
} cli_cmd_t;

void cli_init(void);
void cli_register_cmd(const cli_cmd_t *cmd);
void cli_process(void);  // Called periodically; reads UART, dispatches
```

**文件**：`src/middleware/shell/cli_cmds.c`

内置命令：

| 命令 | 功能 |
|------|------|
| `help` | 列出所有命令 |
| `info` | 系统信息：MCU 型号/主频/RAM 使用/Flash 使用/任务列表 |
| `eth` | 以太网状态：PHY 型号/Link 状态/速率/双工/IP 地址 |
| `ota` | 手动进入 OTA 模式 |
| `reset` | 软件复位系统 |
| `ping <ip>` | Ping 指定 IP 地址（使用 LwIP raw API） |

### 5.5 编写 UART Console 驱动

**文件**：`src/drivers/uart/uart_console.c`、`uart_console.h`

```c
void uart_console_init(uint32_t baudrate);  // USART0, 115200, 8N1
void uart_console_send(const uint8_t *data, uint32_t len);
uint32_t uart_console_recv(uint8_t *buf, uint32_t buf_size);  // Non-blocking
```

### 5.6 更新 main.c — 最终版本

```c
#include "board.h"
#include "boot.h"
#include "ota.h"
#include "udp_echo.h"
#include "cli.h"

int main(void)
{
    board_clock_init();
    board_led_init();
    uart_console_init(115200);

    // Boot mode decision
    boot_mode_t mode = boot_get_mode();

    if (mode == BOOT_MODE_BOOTLOADER) {
        ota_bootloader_entry();
        // ota_bootloader_entry() should call NVIC_SystemReset() on success,
        // or fall through if timeout
    }

    // App mode
    network_init();
    cli_init();

    xTaskCreate(cli_task,        "cli",     512, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(lwip_thread,     "lwip",    512, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(udp_echo_task,   "udp",    1024, NULL, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();
    while (1);
}
```

### 5.7 OTA PC 端工具

Python 脚本 `scripts/ota_tool.py`：

```python
# Usage: python ota_tool.py firmware.bin 192.168.1.100
# Sends firmware via OTA protocol over UDP to GD32
```

### Phase 5 验证

1. Shell 命令测试：UART 连接 → `help` → `info` → `eth` → `ping`
2. OTA 测试：运行 `ota_tool.py firmware_v2.bin 192.168.1.100` → 设备自动复位 → 确认新固件运行
3. 异常保护测试：OTA 中途断电 → 重新上电 → 能回退到 App 模式（因 CRC32 校验失败，自动保持旧固件）

---

## 依赖关系总览

```
Phase 1 (骨架) ──→ Phase 2 (FreeRTOS) ──→ Phase 3 (ETH驱动)
                                               │
                    ┌──────────────────────────┘
                    ↓
              Phase 4 (LwIP + UDP)
                    │
          ┌─────────┴─────────┐
          ↓                   ↓
    Phase 5 (Shell)    Phase 5 (Bootloader)
          └─────────┬─────────┘
                    ↓
          Phase 5 (OTA 联调)
```

## 文件创建清单（按执行顺序）

```
Phase 1 (12 files):
  linker/gd32f450_flash.ld
  Makefile
  .gitignore
  cmake/toolchain.cmake
  CMakeLists.txt
  scripts/openocd/gd32f4x_jlink.cfg
  scripts/openocd/gd32f4x_daplink.cfg
  scripts/gdb/gdbinit
  src/bsp/board.c + board.h
  src/main.c
  config/app_config.h

Phase 2 (4 files):
  config/FreeRTOSConfig.h
  src/drivers/timer/systick_rtos.c + .h
  src/main.c (overwrite)

Phase 3 (5 files):
  src/drivers/eth/phy.h
  src/drivers/eth/phy_lan8720.c
  src/drivers/eth/phy_dp83848.c
  src/drivers/eth/eth_mac.c + .h

Phase 4 (3 files):
  config/lwipopts.h
  src/drivers/eth/ethernetif.c
  src/app/demo/udp_echo.c + .h

Phase 5 (12 files):
  src/app/boot/boot.c + .h
  src/app/ota/ota.c + .h
  src/drivers/flash/flash_if.c + .h
  src/drivers/uart/uart_console.c + .h
  src/middleware/shell/cli.c + .h
  src/middleware/shell/cli_cmds.c
  scripts/ota_tool.py
  src/main.c (overwrite, final)

Utils (throughout):
  src/utils/log.c + .h (Phase 2+)
  src/utils/assert.c + .h (Phase 2+)
  src/utils/crc32.c + .h (Phase 5)
  src/utils/ringbuf.c + .h (Phase 5)
```
