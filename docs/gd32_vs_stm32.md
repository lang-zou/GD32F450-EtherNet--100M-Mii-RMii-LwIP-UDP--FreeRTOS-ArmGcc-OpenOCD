# GD32 与 STM32 差异对照

从 STM32 移植到 GD32（或直接在 GD32 上开发）时，几乎所有外设 API 和宏命名都有差异。本文档记录了在实际项目中遇到的所有差异和解决经验。

---

## 1. 芯片级差异

### 1.1 核心参数

| 参数 | STM32F407 | GD32F450 |
|------|-----------|----------|
| 内核 | Cortex-M4F | Cortex-M4F |
| 最大频率 | 168 MHz | **200 MHz** |
| Flash | up to 1MB | up to 3072KB (3MB) |
| SRAM | 192 KB | **512 KB** (448+64 TCM) |
| Flash 等待周期 | 5 WS @ 168MHz | **5 WS** @ 200MHz (GD32 Flash 更快) |

### 1.2 硬件兼容性

- **引脚兼容**：GD32F450 与 STM32F407 在 LQFP176 封装上引脚兼容
- **SWD 兼容**：完全兼容，J-Link/ST-Link 均可使用
- **外设寄存器偏移**：大部分兼容，但位域名称不同
- **Flash 控制器**：GD32 的 FMC 与 STM32 的 FLASH 控制器 API 完全不可互换

---

## 2. 外设命名对照

### 2.1 系统与时钟 (RCU vs RCC)

| STM32 宏 | GD32 宏 | 说明 |
|----------|---------|------|
| `RCC` | `RCU` | 复位与时钟单元 (Reset and Clock Unit) |
| `RCC_HSE` | `RCU_HXTAL` | 高速外部晶振 (High-speed XTAL) |
| `RCC_HSE_ON` | `RCU_HXTAL_ON` | — |
| `RCC_PLLSOURCE_HSE` | `RCU_PLLSRC_HXTAL` | PLL 时钟源 |
| `RCC_SYSCLKSOURCE_PLLCLK` | `RCU_CKSYSSRC_PLLP` | 系统时钟源选择 |
| `RCC_APB1PeriphClockCmd` | `rcu_periph_clock_enable` | 外设时钟使能 |
| `RCC_AHB1PeriphClockCmd` | `rcu_periph_clock_enable` | **GD32 不分 AHB/APB，统一 API** |

### 2.2 以太网 (ENET)

这是差异最大的外设，几乎每个宏名都不同：

**DMA 描述符位域：**

| STM32 宏 | GD32 宏 | 位域含义 |
|----------|---------|---------|
| `ETH_DMATXDESC_OWN` | `ENET_TDES0_DAV` | **D**escriptor **Av**ailable (DMA 占用) |
| `ETH_DMATXDESC_IC` | `ENET_TDES0_INTC` | **Int**errupt **C**ompletion |
| `ETH_DMATXDESC_FS` | `ENET_TDES0_FSG` | **F**irst **S**egment |
| `ETH_DMATXDESC_LS` | `ENET_TDES0_LSG` | **L**ast **S**egment |
| `ETH_DMATXDESC_TCH` | `ENET_TDES0_TCHM` | **T**ransmit **C**hain |
| `ETH_DMARXDESC_OWN` | `ENET_RDES0_DAV` | 同上，RX 方向 |
| `ETH_DMARXDESC_FL` | `GET_RDES0_FRML(status)` | 帧长度 (需用宏提取，用位 16-29) |
| `ETH_DMARXDESC_ES` | `ENET_RDES0_ERRS` | **Err**or **S**ummary |
| `ETH_DMARXDESC_DE` | `ENET_RDES0_DERR` | **D**escriptor **Err**or |
| `ETH_DMARXDESC_RE` | `ENET_RDES0_RERR` | **R**eceive **Err**or |
| `ETH_DMARXDESC_RCH` | `ENET_RDES1_RCHM` | **R**eceive **Ch**ain |

**DMA 特性配置：**

| STM32 宏 | GD32 宏 | 说明 |
|----------|---------|------|
| `ETH_DMA_IT_FBE` | `ENET_DMA_CTL_DAFRF` | Flush RX FIFO |
| `ETH_DMA_IT_OSE` | `ENET_DMA_CTL_OSF` | Operate on Second Frame |
| `ETH_DMA_IT_NISE` | `ENET_DMA_CTL_NISR_EN` | Normal Interrupt Summary Enable |
| `ETH_DMA_IT_RIE` | `ENET_DMA_CTL_RIE` | Receive Interrupt Enable |

**MAC 配置：**

| STM32 函数/宏 | GD32 函数/宏 | 说明 |
|--------------|-------------|------|
| `ETH_MAC_IT_FRAME_SENT` | — | GD32 用 DMA 中断，不单独区分 |
| `ETH_MACAddress_Set` | `enet_mac_address_set` | MAC 地址设置 |
| `ETH_Start` | `enet_mac_enable` | MAC 使能 |
| `ETH_DMATxDescChainInit` | 手动配置 `eth_tx_desc[i]` | GD32 无封装函数 |
| `ETH_DMARxDescChainInit` | 手动配置 `eth_rx_desc[i]` | GD32 无封装函数 |

**SMI (MDC/MDIO)：**

| STM32 函数 | GD32 函数 | 说明 |
|-----------|----------|------|
| `ETH_ReadPHYRegister()` | `enet_phy_write_read(ENET_PHY_READ, ...)` | 读写同一函数，方向由第一个参数决定 |
| `ETH_WritePHYRegister()` | `enet_phy_write_read(ENET_PHY_WRITE, ...)` | — |

**初始化参数结构体：**

| STM32 方法 | GD32 方法 | 说明 |
|-----------|----------|------|
| `ETH_StructInit(&init)` | 手动设置 `enet_initpara_config()` | GD32 无结构体方式 |
| `ETH_Init(&init, point)` | `enet_initpara_config()` + `enet_mac_config()` | 分两步 |

### 2.3 Flash 控制器 (FMC vs FLASH)

| STM32 | GD32 | 说明 |
|-------|------|------|
| `FLASH_EraseSector()` | `fmc_sector_erase(sector)` | GD32 只需 1 个参数 |
| `FLASH_ProgramWord()` | `fmc_word_program(addr, data)` | GD32 只需 2 个参数 |
| `FLASH_FLAG_PGERR` | `FMC_FLAG_PGMERR` | 编程错误标志 |
| `FLASH_FLAG_EOP` | `FMC_FLAG_END` | 操作完成标志 |

### 2.4 GPIO

| STM32 宏 | GD32 宏 | 说明 |
|----------|---------|------|
| `GPIO_Speed_100MHz` | `GPIO_OSPEED_50MHZ` | GD32 最大速率为 50/100/200MHz，宏名不同 |
| `GPIO_Mode_AF` | `GPIO_MODE_AF` | 功能相同，命名约定不同 |

### 2.5 电源管理 (PMU vs PWR)

| STM32 | GD32 | 说明 |
|-------|------|------|
| `PWR_BackupAccessCmd(ENABLE)` | `pmu_backup_write_enable()` | 备份域写使能 |
| `RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE)` | `rcu_periph_clock_enable(RCU_PMU)` | 使能 PMU 时钟 |

---

## 3. API 风格差异

### 3.1 外设时钟使能

STM32 按总线分类使能：
```c
// STM32
RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
```

GD32 统一 API，不论挂在哪个总线：
```c
// GD32
rcu_periph_clock_enable(RCU_GPIOA);
rcu_periph_clock_enable(RCU_USART0);
```

### 3.2 GPIO 配置

STM32 用结构体：
```c
// STM32
GPIO_InitTypeDef gpio;
gpio.GPIO_Pin   = GPIO_Pin_0;
gpio.GPIO_Mode  = GPIO_Mode_OUT;
gpio.GPIO_Speed = GPIO_Speed_100MHz;
GPIO_Init(GPIOA, &gpio);
```

GD32 用函数调用：
```c
// GD32
gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, GPIO_PIN_0);
```

### 3.3 PHY SMI 通信

STM32 分别调用读写函数：
```c
// STM32
ETH_ReadPHYRegister(phy_addr, reg_addr, &value);
ETH_WritePHYRegister(phy_addr, reg_addr, value);
```

GD32 读写归一到同一函数，内部处理 SMI 忙等待：
```c
// GD32 — 读
enet_phy_write_read(ENET_PHY_READ, phy_addr, reg_addr, &value);

// GD32 — 写
uint16_t val = new_value;
enet_phy_write_read(ENET_PHY_WRITE, phy_addr, reg_addr, &val);
```

**关键**：`enet_phy_write_read` 内部已经处理了 SMI 忙等待，不需要在调用前手动等待。

---

## 4. 移植常见陷阱

### 4.1 DAV 位语义 ≠ OWN 位？

**陷阱**：看到 DAV 名与 STM32 OWN 不同，担心语义不同。

**事实**：DAV = Descriptor Available，逻辑完全一致：
- `DAV = 1`：DMA 拥有描述符（CPU 不可操作）
- `DAV = 0`：CPU 拥有描述符（DMA 已处理完）

与 STM32 OWN 位逻辑相同，直接替换即可。

### 4.2 RX 帧长度提取

**陷阱**：STM32 的 `FL` 位是 RDES0 的位 16-29，可以直接位与提取。GD32 的 `GET_RDES0_FRML()` 是一个宏，**必须用宏提取而不能手动移位**。

```c
// 正确 (GD32)
uint32_t frame_len = GET_RDES0_FRML(desc->status);

// 错误 — 宏名不同
uint32_t frame_len = (desc->status & ETH_DMARXDESC_FL) >> 16;  // STM32 方式
```

### 4.3 时钟配置 PLL 通道

GD32 PLL 输出分三路（P/Q/R），主系统时钟走 P 通道：

```c
// GD32: 明确指定使用 PLLP 作为系统时钟
rcu_system_clock_source_config(RCU_CKSYSSRC_PLLP);

// STM32 等效
RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
```

### 4.4 backup 寄存器访问

**陷阱**：GD32 备份寄存器需要先使能 PMU 时钟。

```c
// GD32: 访问 BKP 前必须使能 PMU
rcu_periph_clock_enable(RCU_PMU);
pmu_backup_write_enable();
// 然后才能读写备份寄存器
```

### 4.5 中断优先级

GD32F4xx 使用**4 位优先级**（16 个优先等级），而非 STM32F4 的可配置优先级分组：

```c
// FreeRTOSConfig.h 中正确配置
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   0x0F   // 最低优先级 = 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  4  // 可安全调用 API 的最高优先级

// 转换到 NVIC 寄存器用的值（高 4 位）
#define configKERNEL_INTERRUPT_PRIORITY           (15 << 4)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY      (4 << 4)
```

### 4.6 Flash 等待周期

GD32F450 @ 200MHz 只需 **5 WS**，比 STM32F407 @ 168MHz 的 5 WS 更优（频率更高但 WS 相同）。

---

## 5. StdPeriph 库文件注意

### 5.1 库配置文件

GD32 StdPeriph 库需要 `gd32f4xx_libopt.h` 文件。这个文件在官方 SDK 中有，但 GCC 移植版可能不包含。如果缺失会报 `No such file or directory` 错误。

从模板目录复制：
```bash
cp gd32f4xx_firmware_library_gcc_makefile/Template/gd32f4xx_libopt.h \
   src/bsp/gd32f4xx/StdPeriph/
```

### 5.2 编译警告

`gd32f4xx_fmc.c` 中有两个未使用变量的警告（`wp0_state`、`wp1_state`），是官方库代码的问题，不影响功能。

`gd32f4xx_enet.c` 中的 `icmp_input` 宏重定义警告（`PBUF_POOL_BUFSIZE_ALIGNED`），是 GD32 库与 LwIP 库的宏命名冲突，在 `-Os` 优化下无害。

---

## 6. 快速移植清单

从 STM32 项目移植到 GD32，按顺序处理：

1. [ ] `#include` 路径：`stm32f4xx.h` → `gd32f4xx.h`
2. [ ] 全局搜索替换外设宏：`RCC` → `RCU`, `FLASH` → `FMC`, `PWR` → `PMU`
3. [ ] 时钟初始化：`RCC_HSE` → `RCU_HXTAL`, `RCC_PLLSource_HSE` → `RCU_PLLSRC_HXTAL`, `RCC_SYSCLKSource_PLLCLK` → `RCU_CKSYSSRC_PLLP`
4. [ ] ENET 驱动全重写：所有 `ETH_DMATXDESC_*` / `ETH_DMARXDESC_*` 宏替换
5. [ ] PHY SMI 驱动：用 `enet_phy_write_read()` 替代 STM32 的读写分开 API
6. [ ] Flash 操作：参数数不同，`fmc_sector_erase(sector)`, `fmc_word_program(addr, data)`
7. [ ] GPIO 速度：`GPIO_Speed_100MHz` → `GPIO_OSPEED_200MHZ`（或 `GPIO_OSPEED_50MHZ`）
8. [ ] 中断优先级：确认 4 位优先级配置
9. [ ] `libopt.h` 文件：确认存在
10. [ ] 编译验证：逐个文件编译，逐个错误修复

**预计工时**：简单外设 (GPIO/UART/Timer) 半天，ENET 以太网 1-2 天。
