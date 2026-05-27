# 代码修复记录

**项目**: GD32F450 FreeRTOS 嵌入式项目  
**日期**: 2026-05-27  
**修复范围**: P0 核心功能 + P1 健壮性 + P2 单元测试

---

## 修改文件清单

| # | 文件 | 操作 | 说明 |
|---|------|------|------|
| 1 | `src/app/ota/ota.c` | 重写 | 完整 OTA UDP 收包逻辑 |
| 2 | `src/app/ota/ota.h` | 重写 | 导出协议常量 + 新增 ota_network_init |
| 3 | `src/app/boot/boot.c` | 修改 | 提取 boot_validate_header() 纯函数 |
| 4 | `src/app/boot/boot.h` | 修改 | 新增 boot_validate_header 声明 |
| 5 | `src/main.c` | 修改 | Bootloader 路径加 ETH/LwIP 初始化 |
| 6 | `src/bsp/board.c` | 修改 | DWT 移到 init + 删除重复 SYSCFG |
| 7 | `src/drivers/uart/uart_console.c` | 修改 | TX 超时保护 + send_str 去重 |
| 8 | `tests/unit/test_boot.c` | 新建 | 9 条用例 |
| 9 | `tests/unit/test_ota.c` | 新建 | 10 条用例 |
| 10 | `scripts/ota_tool.py` | 修正 | 字节序 小端→大端 |
| 11 | `Makefile` | 修改 | 新增测试文件 + include 路径 |

---

## 详细修复

### P0-1: 实现 OTA UDP 收包逻辑

**文件**: `src/app/ota/ota.c` + `src/app/ota/ota.h`

**问题**: `ota_bootloader_entry()` 为空壳，仅打印日志后 `board_delay_ms()` 空等超时。

**修复**:
- 使用 LwIP raw API 实现完整 UDP 协议栈：`udp_new → udp_bind → udp_recv` 回调
- 三阶段状态机：`PHASE_REQ` → `PHASE_DATA` → `PHASE_DONE`
- 包队列机制：ISR 回调入队，主循环轮询出队处理
- 新增 `ota_network_init()`：配置 LwIP netif + 静态 IP
- CRC32 逐块运行校验，6 种 ACK 状态码
- 校验通过后 `flash_erase_range` + 逐块复制到 app 区 → `NVIC_SystemReset()`

### P0-2: Bootloader 路径加 ETH 初始化

**文件**: `src/main.c`

**问题**: `board_periph_init()` 只开 GPIO 时钟，未初始化以太网，OTA 无法通信。

**修复**: 在 `ota_bootloader_entry()` 前调用 `eth_mac_init()` + `ota_network_init()`。

### P1-1: UART TX 超时保护 + 去重

**文件**: `src/drivers/uart/uart_console.c`

**问题**: TX 忙等无限循环，USART 异常时死锁；`send_str` 与 `send` 逻辑重复。

**修复**:
- `UART_TX_TIMEOUT = 0xFFFFFF`，忙等循环加入递减超时检测
- `send_str()` 改为 `uart_console_send((const uint8_t *)str, strlen(str))`

### P1-2: DWT 初始化提到 init

**文件**: `src/bsp/board.c`

**问题**: `board_delay_us()` 每次调用重新配置 DWT 计数器。

**修复**: DWT 配置移到 `board_clock_init()` 末尾一次执行，`delay_us` 只读计数器。

### P1-3: board_periph_init 删重复 SYSCFG

**文件**: `src/bsp/board.c`

**问题**: `rcu_periph_clock_enable(RCU_SYSCFG)` 在第 89 行和第 93 行重复。

**修复**: 删除第 93 行重复调用。

### P2: 增加单元测试

**`tests/unit/test_boot.c`** — 9 条用例：

| 用例 | 场景 |
|------|------|
| test_boot_validate_empty | 全 0xFF → 拒绝 |
| test_boot_validate_valid | 有效 SP+PC（Thumb位）→ 通过 |
| test_boot_validate_sp_low | SP < 0x20000000 → 拒绝 |
| test_boot_validate_sp_high | SP > 0x20020000 → 拒绝 |
| test_boot_validate_sp_boundary | SP = 0x20020000（边界）→ 通过 |
| test_boot_validate_pc_no_thumb | PC Thumb位=0 → 拒绝 |
| test_boot_validate_pc_out_of_range_low | PC 在 boot 区 → 拒绝 |
| test_boot_validate_pc_out_of_range_high | PC 超出 app 区 → 拒绝 |
| test_boot_validate_partial_ff | 仅 SP 有效其余 0xFF → 拒绝 |

**`tests/unit/test_ota.c`** — 10 条用例：

| 用例 | 场景 |
|------|------|
| test_ota_packet_sizes | 包大小常量验证 |
| test_ota_magic_numbers | Magic ASCII 值 |
| test_ota_req_magic_offset | REQ magic 字节位置 |
| test_ota_req_size_field | REQ size 大端编码 |
| test_ota_ack_status_offset | ACK status 字节位置 |
| test_ota_ack_status_codes | 状态码唯一性 |
| test_ota_data_payload_size | DATA payload 大小字段 |
| test_ota_done_crc_field | DONE CRC 大端编码 |
| test_ota_max_payload | 最大 payload/包大小 |
| test_ota_full_roundtrip_constants | 全流程常量一致性 |

### 附带: Python OTA 工具字节序修正

**文件**: `scripts/ota_tool.py`

**问题**: `struct.pack("<I")` 小端，C 端用大端（网络序），双方不兼容。

**修复**: 全部改为 `>I`/`>H`/`>B`，ACK 状态码常量化匹配 `ota.h`。

---

## 对比

| 指标 | 修改前 | 修改后 |
|------|--------|--------|
| OTA 功能 | 空壳 | 完整 UDP 协议实现 |
| Bootloader ETH | 未初始化 | eth_mac_init + netif |
| UART TX 安全 | 无限循环 | 超时保护 |
| DWT 效率 | 每次重配 | 一次初始化 |
| send_str | 逻辑重复 | 复用 send() |
| 单元测试 | 19 条 (2 文件) | 38 条 (4 文件) |
| Python 工具 | 小端（不兼容） | 大端（兼容） |

---

## 运行测试

```bash
make test    # 运行所有 38 条单元测试
```
