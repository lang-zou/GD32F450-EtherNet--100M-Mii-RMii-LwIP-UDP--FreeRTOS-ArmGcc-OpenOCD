# 编译 & 烧录 & 调试 操作指南

日常开发中最常用的命令和工作流。

---

## 1. 编译

### 1.1 基础编译

```bash
cd /e/zoulang/code_proj/embedded_proj
make
```

编译所有源文件，生成 ELF + HEX + BIN，并打印内存占用。

**输出解读：**

```
[CC] system_gd32f4xx.c        ← 绿色：编译 C 文件
[AS] startup_gd32f450_470.S   ← 青色：汇编
[LD] build/firmware.elf       ← 粗体：链接
   text	   data	    bss	    dec	    hex
  53484	   1780	 129384	 184648	  2d148   ← 内存占用
[HEX] build/firmware.hex      ← HEX 生成
[BIN] build/firmware.bin      ← BIN 生成
=== Memory Usage ===
```

| 段 | 含义 |
|----|------|
| `text` | 代码 + 只读数据 (Flash) |
| `data` | 已初始化全局变量 (Flash → RAM 复制) |
| `bss` | 零初始化全局变量 (RAM) |
| `dec` | 总 RAM 占用 = data + bss |

### 1.2 并行编译（加速）

```bash
make -j8    # 8 个并行编译进程
```

### 1.3 切换 PHY 模式

```bash
make ETH_PHY_MODE=MII    # DP83848 MII 模式
make ETH_PHY_MODE=RMII   # LAN8720A RMII 模式（默认）
```

### 1.4 仅查看内存占用（不重新编译）

```bash
make size
```

### 1.5 清理

```bash
make clean      # 删除 build/ 目录
make clean && make -j8   # 全量重编
```

---

## 2. 烧录（Flash）

### 2.1 前提条件

- J-Link 调试器通过 USB 连接 PC
- J-Link 驱动已安装：[Segger 官网](https://www.segger.com/downloads/jlink/)
- GD32F450 开发板已上电
- SWD 接口已连接（VCC, GND, SWDIO, SWCLK）

### 2.2 烧录命令

```bash
make flash
```

**执行流程：**

```
1. 检查 build/firmware.hex 存在
2. 启动 OpenOCD, 加载 scripts/openocd/gd32f4x_jlink.cfg
3. init → reset halt → program → verify → reset → exit
4. 关闭 OpenOCD
```

### 2.3 烧录失败排查

| 错误信息 | 可能原因 | 解决 |
|---------|---------|------|
| `Error: unable to find a matching CMSIS-DAP device` | J-Link 未被识别 | 检查 USB 连接，重装驱动 |
| `Error: init mode failed` | 目标 MCU 无响应 | 检查 SWD 连线，开发板上电 |
| `Error: Flash programming failed` | Flash 写保护 | 先执行 `make erase` |
| `Error: verification failed` | 烧录数据错误 | 重新烧录，检查 SWD 速率 |
| `openocd: command not found` | PATH 未配置 | `source ~/.bashrc` |

### 2.4 整片擦除

```bash
make erase
```

擦除整个 Flash（含 App 区 + OTA Buffer），烧录前先擦除可避免残留数据干扰。

### 2.5 使用其他调试器

项目提供了两套 OpenOCD 配置：

```bash
# J-Link (默认)
# Makefile 中已配置 scripts/openocd/gd32f4x_jlink.cfg

# CMSIS-DAP / DAP-Link
# 修改 Makefile 中 flash/erase/debug 目标的 -f 参数：
#   -f scripts/openocd/gd32f4x_daplink.cfg
```

---

## 3. 调试（GDB）

### 3.1 启动调试

```bash
make debug
```

**执行流程：**

```
1. 后台启动 OpenOCD（端口 3333 等待 GDB 连接）
2. 等 2 秒确保 OpenOCD 就绪
3. 启动 arm-none-eabi-gdb, 自动执行 scripts/gdb/gdbinit:
   - target extended-remote localhost:3333  → 连接 OpenOCD
   - monitor reset halt                    → 复位并暂停
   - monitor flash write_image erase ...   → 烧录固件
   - monitor reset halt                    → 复位停在入口
```

### 3.2 GDB 常用命令

```gdb
# 断点
break main                      # 在 main() 设断点
break eth_mac.c:150             # 在文件行号设断点
info breakpoints                # 列出所有断点
delete 1                        # 删除 1 号断点

# 运行控制
continue        (c)             # 继续运行
step            (s)             # 单步进入
next            (n)             # 单步跳过
finish          (fin)           # 运行到当前函数返回

# 查看状态
info registers                  # 查看所有寄存器
print/x $sp                     # 十六进制打印 SP
print/x variable_name           # 打印变量值
backtrace       (bt)            # 调用栈
info threads                    # FreeRTOS 任务列表

# 内存查看
x/16x 0x20000000                # 查看 16 个字的内存 (hex)
x/8xw &g_netif                  # 查看 netif 结构体

# MCU 控制
monitor reset halt              # 复位并暂停
monitor reset run               # 复位并运行
monitor reg pc sp lr            # 查看 PC/SP/LR

# 退出
quit            (q)             # 退出 GDB
```

### 3.3 调试 FreeRTOS 任务

```gdb
# 查看当前任务
print/x pxCurrentTCB
print (char*)pxCurrentTCB->pcTaskName

# 遍历任务列表
print uxTaskGetNumberOfTasks()

# 查看任务栈
print/x *(pxCurrentTCB)
```

### 3.4 手动 GDB（不通过 make）

```bash
# 终端 1：启动 OpenOCD
openocd -s /d/tools/xpack-openocd-0.12.0-7/openocd/scripts \
        -f scripts/openocd/gd32f4x_jlink.cfg

# 终端 2：连接 GDB
arm-none-eabi-gdb -x scripts/gdb/gdbinit build/firmware.elf
```

---

## 4. 单元测试

### 4.1 运行测试

```bash
make test
```

**输出示例：**

```
[TEST] Building and running unit tests...
  [test_crc32]
tests/unit/test_crc32.c:131:test_crc32_empty:PASS
tests/unit/test_crc32.c:132:test_crc32_known_vector:PASS
...
-----------------------
8 Tests 0 Failures 0 Ignored
OK
  [test_crc32] PASSED
  [test_ringbuf]
...
9 Tests 0 Failures 0 Ignored
OK
  [test_ringbuf] PASSED
```

### 4.2 测试失败排查

```
tests/unit/test_crc32.c:133:test_crc32_single_char:FAIL: Expected 0x1234 Was 0x5678
```

- `Expected` — 测试用例预期的正确值
- `Was` — 实际计算得到的值
- 检查对应函数实现逻辑

### 4.3 添加新测试

1. 在 `tests/unit/` 下创建 `test_xxx.c`
2. 编写测试（参考 `test_crc32.c` / `test_ringbuf.c` 格式）
3. 在 Makefile 的 `test` 目标循环中添加
4. `make test` 运行

**测试文件模板：**

```c
#include "unity.h"
#include "your_module.h"

void setUp(void)    { /* 每个测试前执行 */ }
void tearDown(void) { /* 每个测试后执行 */ }

void test_your_feature_normal_case(void)
{
    int result = your_func(1, 2);
    TEST_ASSERT_EQUAL(3, result);
}

void test_your_feature_edge_case(void)
{
    int result = your_func(0, 0);
    TEST_ASSERT_EQUAL(0, result);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_your_feature_normal_case);
    RUN_TEST(test_your_feature_edge_case);
    return UNITY_END();
}
```

---

## 5. 代码格式化

```bash
make format
```

需要安装 `clang-format`。使用 Google C Style，配置文件为项目根目录的 `.clang-format`。

---

## 6. Makefile 目标速查

| 命令 | 功能 |
|------|------|
| `make` | 编译固件 (ELF + HEX + BIN)，打印内存占用 |
| `make flash` | 通过 OpenOCD + J-Link 烧录 HEX |
| `make erase` | 整片擦除 Flash |
| `make debug` | 启动 OpenOCD + GDB 调试会话 |
| `make test` | 编译运行主机端单元测试 |
| `make clean` | 删除所有编译产物 |
| `make format` | clang-format 格式化源码 |
| `make size` | 查看内存占用（不重新编译） |
| `make ETH_PHY_MODE=MII` | 编译 MII 模式固件 |
| `make -j8` | 8 并行编译 |

---

## 7. 典型开发工作流

### 7.1 日常改代码 → 测试

```bash
# 1. 改代码
vim src/app/demo/udp_echo.c

# 2. 编译
make -j8

# 3. 烧录
make flash

# 4. 打开串口终端观察日志
# 波特率 115200-8-N-1, 设备 USART0 (PA9/PA10)
```

### 7.2 改驱动 → 调试

```bash
# 1. 改驱动代码
vim src/drivers/eth/eth_mac.c

# 2. 编译
make -j8

# 3. 进入调试
make debug
# GDB 中：
(gdb) break eth_mac_send
(gdb) continue
# ... 触发网络通信 ...
(gdb) print/x *desc
(gdb) step
```

### 7.3 改工具函数 → 跑单元测试

```bash
# 1. 改工具函数
vim src/utils/crc32.c

# 2. 直接跑单元测试（无需交叉编译）
make test
# 17 Tests 0 Failures → 通过，继续

# 3. 不通过则修代码，再跑 make test，循环
```

### 7.4 全量重编（换 PHY 模式后）

```bash
make clean && make ETH_PHY_MODE=MII -j8 && make flash
```

---

## 8. 串口终端

开发时需要串口终端查看日志和 Shell 交互。

### 8.1 参数

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验位 | None |
| 流控 | None |
| TX/RX | PA9 (TX) / PA10 (RX) |

### 8.2 终端工具推荐

- **Windows**: PuTTY, Tera Term, MobaXterm
- **Git Bash**: `screen /dev/ttyS3 115200`（注意 COM 端口映射）
- **跨平台**: `minicom`, `picocom`

---

## 9. 常见警告说明

### 9.1 libc 系统调用警告（可忽略）

```
warning: _write is not implemented and will always fail
warning: _fstat is not implemented and will always fail
warning: _isatty is not implemented and will always fail
warning: _getpid/_kill/_read/_lseek/_close ...
```

**原因**：ARM GCC 的 newlib C 库期望底层 OS 提供这些系统调用（open/read/write/...），裸机环境没有实现。链接时用了 `-specs=nosys.specs` 提供空 stub。项目不使用 `printf`/文件 I/O，**完全不影响功能**。

### 9.2 RWX 段权限警告（可忽略）

```
warning: build/firmware.elf has a LOAD segment with RWX permissions
```

**原因**：嵌入式链接脚本将 `.data` 段的 LMA 放在 Flash（只读域）但 VMA 在 RAM（读写域），GNU ld 将整段标记为 RWX（一个段跨了两个存储区域）。**标准做法，不影响运行**。

### 9.3 GD32 库警告（可忽略）

```
src/bsp/gd32f4xx/StdPeriph/gd32f4xx_fmc.c:602: unused variable 'wp1_state'
src/bsp/gd32f4xx/StdPeriph/gd32f4xx_fmc.c:601: unused variable 'wp0_state'
```

GD32 官方库代码的问题，不影响功能。
