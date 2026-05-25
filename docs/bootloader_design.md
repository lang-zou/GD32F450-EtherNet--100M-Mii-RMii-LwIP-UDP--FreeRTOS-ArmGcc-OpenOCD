# Bootloader 设计

## 1. 设计理念：统一固件架构

### 1.1 传统方案的问题

传统 bootloader 方案维护两套独立固件：

```
bootloader.hex  (0x08000000, 独立项目)
    └─ 检查标志 → 跳转到 app 或 进入升级模式

app.hex  (0x08020000, 独立项目)
    └─ VTOR 重映射到 0x08020000
```

**痛点**：
- 两套 startup、链接脚本、HAL 驱动需分别维护
- VTOR 偏移重映射逻辑复杂，容易出错
- bootloader 升级自身时必须额外处理
- 调试需切换两个 ELF 文件
- 共享代码（CRC32、Flash 操作）需在两处编译

### 1.2 本项目方案：单 main.c 统一入口

```
同一份 firmware.elf (0x08000000 → 连续)
  main()
    ├─ boot_get_mode() → 决定走哪个分支
    ├─ [Branch A] ota_bootloader_entry()  → OTA 升级
    └─ [Branch B] app_entry()             → 正常业务
```

**优势**：
- 只需维护一套代码、一套链接脚本
- bootloader 和 app 天然共享所有 HAL/驱动/工具代码
- 不需要 VTOR 重映射（向量表始终在 0x08000000）
- 调试统一，一个 ELF 覆盖全部代码路径
- Flash 简洁，在链接脚本层面就是一段连续镜像

**劣势**：
- 固件整体较大（但 GD32F450 有 3MB Flash，实际只用 ~54KB）
- bootloader 也在 app 的 .text 段中，无法独立升级 bootloader

---

## 2. 启动模式检测

### 2.1 优先级链

```c
boot_mode_t boot_get_mode(void)
{
    // Priority 1: RTC 备份寄存器 (OTA 触发)
    if (bkp_read(0x50) == BOOT_FLAG_MAGIC)  // "OTAB"
        return BOOT_MODE_BOOTLOADER;

    // Priority 2: GPIO 硬件按键
    if (BOOT_BUTTON_PRESSED())
        return BOOT_MODE_BOOTLOADER;

    // Priority 3: App 有效性校验
    if (!boot_is_app_valid())
        return BOOT_MODE_BOOTLOADER;

    // Default: 正常启动
    return BOOT_MODE_APP;
}
```

### 2.2 检测方式说明

| 优先级 | 触发方式 | 检测内容 | 场景 |
|--------|---------|---------|------|
| 1 (最高) | RTC BKP 寄存器 | 读取 `BKP_DATA_0` 是否等于 "OTAB" (0x4F544142) | OTA 完成后置位，复位后强制进入 bootloader |
| 2 | GPIO 按键 | PA0 是否为低电平 | 开发/调试时手动进入 |
| 3 | App 有效性 | 检查 Flash App 区前 256 字节是否全 0xFF，检查 SP/PC 合法性 | App 被意外擦除后自动恢复 |

### 2.3 App 有效性校验逻辑

```c
bool boot_is_app_valid(void)
{
    // 1. 读取 App 区头部 256 字节
    flash_read_buf(FLASH_APP_BASE, buf, 256);

    // 2. 检查是否有内容 (非全 0xFF)
    // 3. 检查栈指针在 SRAM 范围内 (0x20000000 - 0x20020000)
    // 4. 检查复位向量在 App Flash 范围内，且 bit[0] = 1 (Thumb)
    //    bit[0] = 1 表示 Thumb 模式，Cortex-M 只能执行 Thumb 指令
}
```

---

## 3. OTA 固件升级协议

### 3.1 通信参数

| 参数 | 值 |
|------|-----|
| 传输层 | UDP |
| 端口 | 5001 |
| 分块大小 | 1024 bytes |
| 接收超时 | 5 秒（总超时） |
| 块间超时 | 2 秒 |
| 校验算法 | CRC32 |

### 3.2 数据包格式 (TLV)

所有多字节字段为**大端序** (Network Byte Order)。

**REQ — 发起升级请求 (PC → Device, 9 bytes)**

```
Offset  0    1    2    3    4    5    6    7    8
       "O"  "T"  "A"  "Q"   total_size (4B, big-endian)
```

**ACK — 确认响应 (Device → PC, 9 bytes)**

```
Offset  0    1    2    3    4    5    6    7    8
       "O"  "T"  "A"  "A"   seq (4B)    status (1B)
                                              0x00 = OK
                                              0x01 = NAK (重传)
                                              0x02 = Abort
```

**DATA — 固件数据块 (PC → Device, 10 + N bytes)**

```
Offset  0    1    2    3    4    5    6    7    8    9    10 ..
       "O"  "T"  "A"  "D"   seq (4B)    chunk_size (2B)   payload (N bytes)
                                              (max 1024)
```

**DONE — 传输完成 (PC → Device, 12 bytes)**

```
Offset  0    1    2    3    4    5    6    7    8    9    10   11
       "O"  "T"  "A"  "O"   total_size (4B)       crc32 (4B)
```

### 3.3 升级流程

```
1. Device 进入 Bootloader 模式
2. Device 初始化 ETH + UDP 端口 5001
3. Device 等待 REQ (超时 5 秒)
4. PC 发送 REQ (固件总大小)
5. Device 擦除 OTA Buffer 区域，回复 ACK (seq=0, OK)
6. PC 逐块发送 DATA (seq 递增)
7. Device 每收到一块 → 写入 OTA Buffer → 回复 ACK
8. PC 发送 DONE (总大小 + CRC32)
9. Device 全量 CRC32 校验 OTA Buffer 内容
10. CRC 通过 → 从 OTA Buffer 复制到 App 区
11. 设置 BKP 引导标志 → NVIC_SystemReset()
12. 芯片复位 → boot_get_mode() 检测到 BKP 标志 → 再次进入 Bootloader
    → 校验 App 有效性 → 正常启动
```

### 3.4 错误处理

| 错误场景 | 处理 |
|---------|------|
| 块序号不连续 | 回复 ACK (NAK), 丢弃当前块 |
| 块大小超出范围 (>1024) | 回复 ACK (Abort), 退出升级 |
| 写入 Flash 失败 | 回复 ACK (Abort), 退出升级 |
| CRC32 校验失败 | 回复 ACK (Abort), 保持原 App |
| 总超时 (5 秒无 REQ) | 退出 Bootloader, 走正常 App 路径 |

---

## 4. Flash 分区

### 4.1 物理布局

```
GD32F450IK6 Flash (1024KB used / 3072KB total)

0x08000000 ┌──────────────┐  ← 中断向量表
           │ .isr_vector  │
           ├──────────────┤
           │ .text        │  ← 全部代码 (bootloader + app + 库)
           │              │     当前 ~54KB, 远小于 128KB 预算
           │ .rodata      │
           │              │
0x08020000 ├─ ─ ─ ─ ─ ─ ─ ┤  ← 逻辑 App 起始 (同一 .text 段内)
           │              │
           │  (剩余 .text) │
           │              │
0x08080000 ├─ ─ ─ ─ ─ ─ ─ ┤  ← OTA Buffer (独立的 Flash 区域)
           │              │     OTA 升级时暂存新固件
           │ OTA Buffer   │     448KB, 可容纳完整固件
           │              │
0x080F0000 ├──────────────┤  ← Config 区
           │ Config/Data  │     64KB, 持久化配置参数
0x08100000 └──────────────┘  ← Flash 末尾 (1024KB)
```

### 4.2 配置常量 (app_config.h)

```c
#define FLASH_BOOT_BASE       0x08000000UL   // Boot + Common
#define FLASH_APP_BASE        0x08020000UL   // Application
#define FLASH_OTA_BUFFER_BASE 0x08080000UL   // OTA 固件暂存
#define FLASH_CONFIG_BASE     0x080F0000UL   // 持久化配置

#define FLASH_BOOT_SIZE       0x20000UL      // 128 KB
#define FLASH_APP_SIZE        0x60000UL      // 384 KB
#define FLASH_OTA_BUFFER_SIZE 0x70000UL      // 448 KB
#define FLASH_CONFIG_SIZE     0x10000UL      // 64 KB
```

---

## 5. 关键实现细节

### 5.1 备份寄存器

GD32F450 有 32 个 16 位备份寄存器 (BKP_DATA_0 ~ BKP_DATA_31)，位于备份域 (0x40002800 + 偏移)。掉电后由 VBAT 供电保持。

```c
#define BKP_BASE     0x40002800UL
#define BOOT_BKP_REG  BKP_DATA_0  // 0x50 偏移

// 访问前必须使能 PMU 时钟
rcu_periph_clock_enable(RCU_PMU);
pmu_backup_write_enable();
```

### 5.2 Flash 操作

OTA 写入涉及擦除和编程，全部使用 GD32 官方 FMC API：

```c
// 擦除一个扇区 (sector 号, 非地址)
fmc_sector_erase((uint32_t)sector);  // 返回 fmc_state_enum

// 按 32-bit word 编程
fmc_word_program(addr, data);  // 返回 fmc_state_enum

// 读取 (直接内存访问)
uint8_t *src = (uint8_t *)flash_addr;
```

### 5.3 CRC32 校验

```c
uint32_t crc = crc32_init();
crc = crc32_update(crc, buf, len);  // 可按块累加
crc = crc32_finalize(crc);
```

OTA 时边接收边累加 CRC32，收到 DONE 包后与 PC 发送的 CRC32 比较。

---

## 6. 启动时序

```
上电
 │
 ▼
Reset_Handler
 ├─ 复制 .data, 清零 .bss
 ├─ SystemInit() → FPU 使能, SystemCoreClock = 200MHz
 └─ main()
      │
      ├─ board_clock_init() → HXTAL 25MHz → PLL → 200MHz
      │
      ├─ boot_get_mode()
      │    │
      │    ├── BKP 标志 == "OTAB" ?
      │    │    YES → BOOT_MODE_BOOTLOADER
      │    │
      │    ├── BOOT_BUTTON 按下 ?
      │    │    YES → BOOT_MODE_BOOTLOADER
      │    │
      │    ├── App 区无有效固件 ?
      │    │    YES → BOOT_MODE_BOOTLOADER
      │    │
      │    └── NO  → BOOT_MODE_APP
      │
      ├─ [Bootloader]
      │    ├─ 最小外设初始化
      │    ├─ ETH + PHY + LwIP 初始化
      │    ├─ 等待 OTA (UDP 5001, 超时 5s)
      │    ├─ 超时 → fall through
      │    └─ 升级完成 → Reset
      │
      └─ [App]
           ├─ 全外设初始化
           ├─ 网络初始化
           ├─ 创建 FreeRTOS 任务
           └─ vTaskStartScheduler()
```

---

## 7. 安全注意事项

1. **OTA Buffer 擦除**：接收前先整区擦除，防止旧数据残留
2. **CRC32 全量校验**：不是逐块校验，而是验证整个固件镜像
3. **写保护**：Boot 区 (0x08000000-0x0801FFFF) 不应在 OTA 时被擦除
4. **回退保护**：CRC 失败时保持旧固件不变，不部分写入
5. **看门狗**：OTA 过程中未喂狗（ENABLE_WATCHDOG=0 默认关闭），长固件传输可能触发复位

---

## 8. 从 Shell 触发 OTA

```bash
# 通过 UART 控制台
gd32> ota
Entering OTA mode...
[OTA] Waiting for firmware update on UDP port 5001...
```

`cmd_ota` handler 调用 `ota_enter_from_cli()` 主动进入升级模式。
