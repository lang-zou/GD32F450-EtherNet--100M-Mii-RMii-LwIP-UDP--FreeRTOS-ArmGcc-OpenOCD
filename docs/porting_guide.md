# 新项目移植指南

如何基于本项目快速创建新的嵌入式项目（换 MCU / 改功能 / 做新产品）。

---

## 1. 项目复用价值

本项目可作为模板复用的部分：

| 可复用 | 不可直接复用（需修改） |
|--------|----------------------|
| 四层分层架构 + 调用规则 | MCU 特定的 BSP/HAL 文件 |
| Makefile 构建系统框架 | 引脚映射 (board.h) |
| FreeRTOS 集成方式 + FreeRTOSConfig.h 模板 | 时钟配置 (board.c) |
| LwIP 集成方式 + lwipopts.h 模板 | 链接脚本 (gd32f450_flash.ld) |
| Bootloader 统一固件设计 | 外设驱动 (非 GD32 系列) |
| UDP/OTA 协议实现 | 启动文件 (startup_*.S) |
| CLI Shell 框架 | CMSIS 核心文件 |
| 日志系统、CRC32、RingBuf | |
| Unity 单元测试框架 | |
| 配置集中管理模式 | |
| 文档体系结构 | |

---

## 2. 场景一：同一 MCU (GD32F450)，不同产品

**改动量**：小。主要改应用层。

### 2.1 保留不变

- 所有 `src/bsp/gd32f4xx/` 文件
- 所有 `src/drivers/` 文件
- 所有 `src/middleware/` 文件
- 所有 `src/utils/` 文件
- `linker/gd32f450_flash.ld`
- 链接脚本、Makefile 工具链配置

### 2.2 需要修改

| 文件 | 修改内容 |
|------|---------|
| `config/app_config.h` | 功能开关：关闭不需要的 demo，打开新功能 |
| `config/FreeRTOSConfig.h` | 按新产品的任务数/优先级调整 |
| `board.h` | 按新板子实际连线调整引脚映射 |
| `src/bsp/board.c` | 添加新外设初始化 |
| `src/main.c` | 创建新业务 task，删除旧 demo task |
| `src/app/` | 删除 demo 目录，新建业务模块目录 |
| `Makefile` | 更新源文件列表 |

### 2.3 步骤清单

```bash
# 1. 复制项目
cp -r embedded_proj new_product/

# 2. 修改板级配置
vim new_product/config/app_config.h   # 功能开关
vim new_product/src/bsp/board.h       # 引脚映射

# 3. 清理 demo 代码
rm -rf new_product/src/app/demo/

# 4. 创建新业务模块
mkdir new_product/src/app/my_module/

# 5. 修改 main.c
vim new_product/src/main.c  # 替换 task 创建逻辑

# 6. 更新 Makefile 源文件列表

# 7. 编译验证
cd new_product && make
```

---

## 3. 场景二：换用 STM32F4xx MCU

**改动量**：中等。BSP/HAL 层和驱动层需适配。

### 3.1 需要替换

| 原文件 | 替换为 | 说明 |
|--------|--------|------|
| `src/bsp/gd32f4xx/CMSIS/` | STM32F4 CMSIS 文件 | `stm32f407xx.h`, `system_stm32f4xx.c`, `startup_stm32f407xx.s` |
| `src/bsp/gd32f4xx/StdPeriph/` | STM32 HAL 或 StdPeriph | 如果用 HAL，风格差异较大；建议用 STM32 StdPeriph 库保持风格一致 |
| `linker/gd32f450_flash.ld` | `stm32f407_flash.ld` | Flash/RAM 地址可能不同 |

### 3.2 需要适配

| 文件 | 改动 |
|------|------|
| `board.c` | 所有 API 从 GD32 名改为 STM32 名 (参见 [gd32_vs_stm32.md](gd32_vs_stm32.md)) |
| `board.h` | `#include "gd32f4xx.h"` → `#include "stm32f4xx.h"` |
| `eth_mac.c` | **重写**。所有 ENET DMA 描述符宏名、初始化 API 完全不同 |
| `phy_lan8720.c` | SMI 读写函数从 `enet_phy_write_read()` 改为 `ETH_ReadPHYRegister()` / `ETH_WritePHYRegister()` |
| `flash_if.c` | Flash API 从 `fmc_*` 改为 `FLASH_*` |
| `boot.c` | 备份寄存器访问方式可能不同 |
| `systick_rtos.c` | SysTick 寄存器可能不同（但 CMSIS 层面通常一致） |
| `config/FreeRTOSConfig.h` | CPU 频率、中断优先级 |
| `Makefile` | MCU flags: `-mcpu=cortex-m4`, 设备宏从 `-DGD32F450` 改为 `-DSTM32F407xx` |

### 3.3 核心修改示例：eth_mac.c 关键差异点

```c
// GD32 (当前)
desc->status = ENET_TDES0_TCHM | ENET_TDES0_FSG
             | ENET_TDES0_LSG | ENET_TDES0_INTC
             | ENET_TDES0_DAV;

// STM32 (移植后)
desc->status = ETH_DMATXDESC_TCH | ETH_DMATXDESC_FS
             | ETH_DMATXDESC_LS | ETH_DMATXDESC_IC
             | ETH_DMATXDESC_OWN;
```

> 完整差异清单见 [docs/gd32_vs_stm32.md](docs/gd32_vs_stm32.md)

### 3.4 步骤清单

1. [ ] 复制项目到新目录
2. [ ] 替换 CMSIS 文件（core 头文件 + 设备头文件 + system + startup）
3. [ ] 替换 StdPeriph 库文件
4. [ ] 更新 `board.h` — `#include` 和时钟宏定义
5. [ ] 重写 `board.c` — 时钟和外设初始化 API
6. [ ] 重写 `eth_mac.c` — ENET 宏名和 API
7. [ ] 适配 `phy_*.c` — SMI 读写 API
8. [ ] 适配 `flash_if.c` — Flash API
9. [ ] 适配 `boot.c` — 备份寄存器访问
10. [ ] 更新 `linker/*.ld` — Flash/RAM 地址
11. [ ] 更新 `FreeRTOSConfig.h` — CPU 频率
12. [ ] 更新 Makefile — MCU flags 和源文件列表
13. [ ] **逐个文件编译，逐个错误修复**
14. [ ] 烧录验证

---

## 4. 场景三：换用其他 Cortex-M 系列 MCU

**改动量**：大。按场景二的步骤，额外关注：

- 确认 FPU 配置（Cortex-M3 无 FPU，需去掉 `-mfpu=fpv4-sp-d16 -mfloat-abi=hard`）
- 确认中断优先级位数（Cortex-M3/M4 通常为 4 位，Cortex-M7 可能 8 位）
- TCM 内存（Cortex-M7 有 ITCM/DTCM，需调整链接脚本）
- 启动文件根据具体内核选择

---

## 5. 场景四：添加 FreeRTOS 到已有裸机项目

**改动量**：小到中。可参考本项目的集成方式。

### 5.1 核心步骤

1. 复制 FreeRTOS 内核源码到项目中
2. 创建 `FreeRTOSConfig.h`（直接从本项目复用，修改 CPU_HZ）
3. 将 SysTick 替换为 FreeRTOS tick（本项目的 `systick_rtos.c` 可直接复用）
4. 将裸机 `while(1)` 转换为 FreeRTOS task
5. 在 Makefile 中添加 FreeRTOS 源文件
6. 在链接脚本中确保 heap/stack 配置充足

### 5.2 FreeRTOSConfig.h 关键参数

```c
#define configCPU_CLOCK_HZ          (200000000UL)  // 改为你的 MCU 频率
#define configTICK_RATE_HZ          (1000)          // 通常 1KHz 即可
#define configTOTAL_HEAP_SIZE       (64 * 1024)     // 按需调整
#define configMAX_PRIORITIES        (8)             // 按需调整
#define configMINIMAL_STACK_SIZE    (128)           // 单位是 words, 非 bytes
```

---

## 6. 添加新外设驱动（以本项目架构为准）

以添加 I2C 驱动为例：

### 6.1 文件创建

```
src/drivers/i2c/i2c_if.c
src/drivers/i2c/i2c_if.h
```

### 6.2 接口设计

```c
// i2c_if.h
typedef enum { I2C_SPEED_STANDARD = 100000, I2C_SPEED_FAST = 400000 } i2c_speed_t;

bool i2c_init(uint8_t instance, i2c_speed_t speed);
bool i2c_write(uint8_t instance, uint8_t addr, const uint8_t *data, uint16_t len);
bool i2c_read(uint8_t instance, uint8_t addr, uint8_t *data, uint16_t len);
```

### 6.3 集成步骤

1. 编写 `i2c_if.c`：调用 GD32 StdPeriph `gd32f4xx_i2c.c` 的 API
2. 在 `board.h` 中定义 I2C 引脚映射
3. 在 `board.c` 中添加 I2C GPIO/时钟初始化
4. 在 Makefile 中添加 `gd32f4xx_i2c.c` 和 `i2c_if.c`
5. 应用层 `#include "i2c_if.h"` 即可直接使用

---

## 7. Makefile 移植注意事项

### 7.1 MCU 编译标志

```makefile
# Cortex-M4F (有 FPU)
CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# Cortex-M3 (无 FPU)
CPU = -mcpu=cortex-m3
FPU =                                           # 留空

# Cortex-M0+
CPU = -mcpu=cortex-m0plus
FPU =                                           # 留空
```

### 7.2 设备宏

```makefile
# GD32F450
CFLAGS += -DGD32F450

# STM32F407
CFLAGS += -DSTM32F407xx

# STM32F103
CFLAGS += -DSTM32F103xB
```

### 7.3 链接脚本 Flash/RAM 地址

```ld
/* GD32F450IK6 */
FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 448K

/* STM32F407VG */
FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K

/* STM32F103C8 */
FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
```

---

## 8. 快速检查清单

拿到新 MCU/新板子时，按此清单确保基本链路正常：

**Phase 1 — 工具链验证：**
- [ ] `arm-none-eabi-gcc --version` 正常
- [ ] 空项目能编译链接（不做任何初始化，只验证链接脚本）

**Phase 2 — 最小系统：**
- [ ] `Reset_Handler` → `SystemInit()` → `main()` 能进入
- [ ] 时钟配置正确（用示波器测 MCO 输出）
- [ ] GPIO 输出正常（最小=LED 闪烁）

**Phase 3 — 串口：**
- [ ] UART TX 能输出字符（最基础的 printf 调试）

**Phase 4 — RTOS：**
- [ ] SysTick 中断产生
- [ ] FreeRTOS vTaskStartScheduler() 不崩溃
- [ ] 两个简单 task 能切换

**Phase 5 — 网络（如有）：**
- [ ] PHY 能通过 SMI 读取寄存器
- [ ] 链路能 Up
- [ ] ARP 能解析
- [ ] UDP 能收发

---

## 9. 文档体系复用

本项目 `docs/` 目录可直接复制到新项目中，按需修改：

- `architecture.md` → 更新分层图、任务清单、Flash 分区
- `build_config.md` → 更新编译配置、源文件清单
- `toolchain_setup.md` → 工具链版本升级时更新
- `porting_guide.md` → 本文件，记录移植到新 MCU 的过程
- `gd32_vs_stm32.md` → 换 MCU 平台时重置
