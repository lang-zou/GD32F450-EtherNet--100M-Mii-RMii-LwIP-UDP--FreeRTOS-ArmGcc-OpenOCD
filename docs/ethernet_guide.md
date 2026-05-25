# 以太网驱动指南

从寄存器到 Socket 的全链路设计。覆盖 GD32F450 ENET MAC 驱动、双 PHY 模式、LwIP 协议栈移植。

---

## 1. 架构概览

```
┌─────────────────────────────────────────────┐
│              LwIP Socket API                │
│  udp_new() → udp_bind() → udp_recv()       │
├─────────────────────────────────────────────┤
│              LwIP Core                      │
│  udp_input() → ip4_output() → etharp_*()   │
├─────────────────────────────────────────────┤
│              LwIP Port (ethernetif.c)       │
│  low_level_output() / low_level_input()     │
├─────────────────────────────────────────────┤
│              ENET MAC Driver (eth_mac.c)    │
│  DMA Descriptor × 10  │  eth_mac_send/recv  │
├─────────────────────────────────────────────┤
│              PHY Driver (phy_lan8720 / phy_ │
│  SMI (MDC/MDIO)  │  Auto-Negotiation       │
├─────────────────────────────────────────────┤
│              GD32F450 ENET Peripheral       │
│  RMII (9 pins) / MII (17 pins)             │
├─────────────────────────────────────────────┤
│              PHY Chip                       │
│  LAN8720A (RMII)  /  DP83848 (MII)         │
└─────────────────────────────────────────────┘
```

---

## 2. PHY 驱动（phy.h 统一接口）

### 2.1 接口设计

```c
// phy.h — 所有 PHY 芯片必须实现的统一接口
bool phy_init(void);                             // 初始化 (复位 + 自协商)
bool phy_get_status(phy_status_t *status);       // 读取链路状态
void phy_hardware_reset(void);                   // GPIO 硬件复位
bool phy_read_reg(uint8_t addr, uint8_t reg, uint16_t *val);   // SMI 读
bool phy_write_reg(uint8_t addr, uint8_t reg, uint16_t val);   // SMI 写
```

### 2.2 SMI 通信（关键）

GD32 使用 `enet_phy_write_read()` 统一 API 处理 PHY 寄存器读写：

```c
bool phy_read_reg(uint8_t phy_addr, uint8_t reg_addr, uint16_t *value)
{
    if (value == NULL) return false;
    // ENET_PHY_READ 方向, phy_addr 地址, reg_addr 寄存器号, value 输出
    return (SUCCESS == enet_phy_write_read(ENET_PHY_READ, phy_addr, reg_addr, value));
}

bool phy_write_reg(uint8_t phy_addr, uint8_t reg_addr, uint16_t value)
{
    uint16_t val = value;
    // ENET_PHY_WRITE 方向
    return (SUCCESS == enet_phy_write_read(ENET_PHY_WRITE, phy_addr, reg_addr, &val));
}
```

**要点**：
- 读写使用同一函数，方向由 `ENET_PHY_READ` / `ENET_PHY_WRITE` 区分
- 函数内部已处理 SMI 忙等待，无需手动等待
- 写操作需要传指向 `uint16_t` 的指针（API 设计如此）

### 2.3 PHY 初始化流程

```
phy_init()
  ├─ phy_hardware_reset()     → GPIO 拉低/拉高 PHY RESET 引脚
  ├─ phy_write_reg(BCR, RESET) → 软件复位
  ├─ 等待复位完成 (poll BCR)
  ├─ phy_write_reg(ADVERTISE)  → 宣告能力 (100Mbps Full-Duplex)
  ├─ phy_write_reg(BCR, AN_EN) → 启动自协商
  └─ 等待自协商完成 (poll BSR)
```

### 2.4 双 PHY 芯片对比

| 特性 | LAN8720A (RMII) | DP83848 (MII) |
|------|-----------------|---------------|
| 接口 | RMII (9 pins) | MII (17 pins) |
| PHY 地址 | 0x00 | 0x01 |
| 特殊寄存器 | 无 | 无 |
| 自协商 | 标准 IEEE 802.3 | 标准 IEEE 802.3 |
| 时钟 | 50MHz (REF_CLK) | 25MHz (TX_CLK/RX_CLK) |

---

## 3. ENET MAC 驱动（eth_mac.c）

### 3.1 DMA 描述符链

GD32F450 ENET 使用链式 DMA 描述符管理收发缓冲：

```
TX Descriptor Chain (5 descriptors):
┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐
│ TX[0]│──→│ TX[1]│──→│ TX[2]│──→│ TX[3]│──→│ TX[4]│─→ back to TX[0]
└──────┘   └──────┘   └──────┘   └──────┘   └──────┘

RX Descriptor Chain (5 descriptors):
┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐
│ RX[0]│──→│ RX[1]│──→│ RX[2]│──→│ RX[3]│──→│ RX[4]│─→ back to RX[0]
└──────┘   └──────┘   └──────┘   └──────┘   └──────┘
```

每个描述符包含：
- `status` (TDES0/RDES0): 控制和状态位
- `control_buffer_size` (TDES1/RDES1): 缓冲区大小/控制
- `buffer1_addr`: 数据缓冲区指针
- `buffer2_next_desc_addr`: 下一个描述符指针（链式模式）

### 3.2 描述符所有权（DAV 位）

```
DAV = 1 → DMA 引擎拥有，CPU 不可操作
DAV = 0 → CPU 拥有，DMA 已处理完

发送流程:
  1. CPU 填充 TX 缓冲区
  2. CPU 设置 TDES0 = TCHM | FSG | LSG | INTC | DAV
  3. ENET DMA 检测到 DAV=1 → 发送帧
  4. 发送完成 → DMA 清除 DAV 位
  5. CPU 轮询 DAV=0 确认发送完成

接收流程:
  1. 初始化时 CPU 设置 RDES0 = DAV (DMA 拥有)
  2. 收到帧 → DMA 填充缓冲区 → 清除 DAV 位
  3. CPU 轮询 DAV=0 发现有帧
  4. CPU 读取数据后重新设置 DAV=1
```

### 3.3 发送实现

```c
bool eth_mac_send(const uint8_t *data, uint16_t len)
{
    // 1. 等待当前 TX 描述符可用
    while ((desc->status & ENET_TDES0_DAV) && --timeout);

    // 2. 复制数据到 TX 缓冲区
    memcpy(tx_buf, data, len);

    // 3. 设置描述符: 链模式 + 首帧 + 末帧 + 完成中断 + DMA 占用
    desc->status = ENET_TDES0_TCHM | ENET_TDES0_FSG
                 | ENET_TDES0_LSG | ENET_TDES0_INTC
                 | ENET_TDES0_DAV;

    // 4. 通知 DMA 有数据待发 (如果 DMA 暂停)
    enet_dma_transmission_resume();

    return true;
}
```

### 3.4 接收实现

```c
uint16_t eth_mac_recv(uint8_t *buf, uint16_t buf_size)
{
    // 1. 检查 RX 描述符是否被 CPU 拥有 (DAV=0 表示有帧)
    if (desc->status & ENET_RDES0_DAV)
        return 0;  // 无帧

    // 2. 获取帧长度 (使用 GD32 专用宏)
    uint32_t frame_len = GET_RDES0_FRML(desc->status);

    // 3. 错误检查
    if (desc->status & (ENET_RDES0_ERRS | ENET_RDES0_DERR | ENET_RDES0_RERR))
        goto release;

    // 4. 复制数据
    memcpy(buf, rx_buf, frame_len);

release:
    // 5. 归还描述符给 DMA
    desc->status = ENET_RDES0_DAV;

    return frame_len;
}
```

### 3.5 MAC 初始化关键步骤

```c
bool eth_mac_init(eth_mac_mode_t mode)
{
    // 1. 使能 ENET 时钟
    rcu_periph_clock_enable(RCU_ENET);

    // 2. GPIO 配置 (RMII 9 pins / MII 17 pins)
    //    gpio_mode_set() + gpio_output_options_set() + gpio_af_set()

    // 3. SMI 配置 (MDC/MDIO)
    enet_mdc_clk_range_config(ENET_MDC_RANGE_200MHZ);

    // 4. 选择 PHY 接口模式
    if (mode == ETH_MAC_MODE_RMII)
        enet_rmii_mode_enable();
    else
        enet_mii_mode_enable();

    // 5. MAC 配置
    enet_initpara_config(DMA_OPTION, ENABLE);
    enet_initpara_config(STORE_OPTION, 1); // Store-and-Forward

    // 6. DMA 配置
    enet_dma_feature_enable(ENET_DMA_CTL_DAFRF | ENET_DMA_CTL_OSF);

    // 7. 初始化 DMA 描述符链
    //    设置 TX/RX buffer 地址和描述符指针

    // 8. 启动 MAC + DMA
    enet_mac_enable();
    enet_dma_enable();

    // 9. 初始化 PHY
    phy_init();

    return true;
}
```

### 3.6 GD32 ENET 特殊函数说明

| 函数 | 用途 |
|------|------|
| `enet_initpara_config(DMA_OPTION, ENABLE)` | 使能 DMA 功能 |
| `enet_initpara_config(STORE_OPTION, 1)` | Store-and-Forward 模式（收完整帧再转发） |
| `enet_dma_feature_enable(ENET_DMA_CTL_DAFRF \| ENET_DMA_CTL_OSF)` | Flush RX FIFO + 处理多帧 |
| `enet_mdc_clk_range_config(ENET_MDC_RANGE_200MHZ)` | MDC 时钟 = HCLK/82 (适合 200MHz) |
| `enet_rmii_mode_enable()` / `enet_mii_mode_enable()` | PHY 接口模式选择 |
| `enet_dma_transmission_resume()` | 通知 DMA 有数据，开始轮询 TX 描述符 |

---

## 4. LwIP 移植层（ethernetif.c）

### 4.1 初始化

```c
err_t ethernetif_init(struct netif *netif)
{
    // 1. 分配 ethernetif_data (MAC 地址 + 链路状态)
    eth_data = (struct ethernetif_data *)mem_malloc(sizeof(...));

    // 2. 设置 netif 字段
    netif->name[0]    = 'e';
    netif->name[1]    = 'n';
    netif->output     = etharp_output;       // IPv4 输出
    netif->linkoutput = low_level_output;    // 链路层输出
    netif->mtu        = 1500;
    netif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    // 3. 初始化底层硬件 (MAC + PHY)
    low_level_init(netif);
}
```

### 4.2 数据收发

**发送** (`low_level_output`):
```
LwIP 调用 netif->linkoutput(pbuf)
  → 遍历 pbuf 链表，逐段拷贝到线性 TX buffer
  → eth_mac_send(tx_buf, total_len)
```

**接收** (`low_level_input`):
```
ethernetif_input() 被周期性调用 (在 udp_echo_task 中每 10ms 一次)
  → eth_mac_recv(rx_buf)  → 轮询 RX 描述符
  → pbuf_alloc(PBUF_RAW, len, PBUF_POOL)
  → pbuf_take(p, rx_buf, len)
  → netif->input(p, netif)  → 投喂 LwIP 协议栈
```

**链路检测** (`ethernetif_link_check`):
```
每 1000 次 ethernetif_input() 调用执行一次
  → eth_mac_link_status(&speed, &duplex)
  → 更新 netif->flags (NETIF_FLAG_LINK_UP)
```

### 4.3 sys_now() 实现

LwIP 需要毫秒级时间基准：

```c
u32_t sys_now(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}
```

### 4.4 端口文件

LwIP 需要以下 arch 文件（位于 `src/middleware/lwip/include/arch/`）：

| 文件 | 内容 |
|------|------|
| `cc.h` | 编译器抽象：`PACK_STRUCT_*` 宏（GCC `__attribute__((packed))`），数据类型映射 |
| `cpu.h` | 端序：`BYTE_ORDER == LITTLE_ENDIAN` |
| `sys_arch.h` | OS 抽象层：信号量/邮箱类型定义（NO_SYS 模式下可简化为空实现） |

---

## 5. LwIP 配置（lwipopts.h）

```c
// 操作系统集成 (使用 FreeRTOS)
#define NO_SYS                  0   // 启用 OS 支持

// 协议开关
#define LWIP_UDP                1   // 启用 UDP (必须)
#define LWIP_TCP                0   // 禁用 TCP (节省 ~40KB)
#define LWIP_ICMP               1   // 启用 ICMP (ping 支持)
#define LWIP_DHCP               0   // 禁用 DHCP (使用静态 IP)

// 内存配置
#define MEM_SIZE                (16 * 1024)  // 堆内存池
#define MEMP_NUM_PBUF           16            // pbuf 数量
#define MEMP_NUM_UDP_PCB        8             // UDP PCB 数量

// pbuf 配置
#define PBUF_POOL_SIZE          16            // pbuf 池大小
#define PBUF_POOL_BUFSIZE       1528          // 单 pbuf 缓冲区 (MTU + 头)

// 无 OS 保护 (NO_SYS=0 时不适用)
#define SYS_LIGHTWEIGHT_PROT    0

// 调试级别 (release 时全关)
#define LWIP_DEBUG              0
```

---

## 6. 网络初始化完整流程

```c
bool network_init(void)
{
    // 1. 初始化 LwIP 内核
    lwip_init();  // 初始化 mem/memp/pbuf/netif/sys 等子系统

    // 2. 配置静态 IP (192.168.1.100 / 255.255.255.0 / 网关 192.168.1.1)
    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 192, 168, 1, 100);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 1, 1);

    // 3. 注册网络接口
    //    ethernetif_init → 底层硬件初始化
    //    ethernetif_input → 收包处理函数
    netif_add(&g_netif, &ipaddr, &netmask, &gw,
              NULL, ethernetif_init, ethernetif_input);

    // 4. 设为默认接口并启用
    netif_set_default(&g_netif);
    netif_set_up(&g_netif);
}
```

---

## 7. UDP Echo 数据流

```
外部 PC: echo "hello" | nc -u 192.168.1.100 5000

  ↓ 以太网帧到达 PHY

PHY (LAN8720A/DP83848)
  → RMII/MII 接口
  → ENET MAC 接收
  → DMA RX 描述符 (DAV 被硬件清除)

udp_echo_task (每 10ms):
  ethernetif_input(&g_netif)
    → low_level_input()
      → eth_mac_recv(rx_buf, 1524) → 152 字节
      → pbuf_alloc() + pbuf_take()
      → netif->input(p, netif)
        → LwIP 协议栈
          → ip4_input()
            → udp_input()
              → udp_echo_callback()  ← 我们的回调函数
                → LOG_I("UDP rx: 152 bytes from 192.168.1.X:XXXXX")
                → udp_sendto(pcb, p, addr, port)  ← 原样回发
                → bsp_led_toggle()

  udp_sendto()
    → udp_send()
      → ip4_output_if()
        → etharp_output()
          → netif->linkoutput()
            → low_level_output()
              → 遍历 pbuf → 线性 TX buffer
              → eth_mac_send(tx_buf, 152)
                → 设置 DMA TX 描述符 (DAV=1)
                → enet_dma_transmission_resume()

  ↓ 帧从 ENET MAC → PHY → 网线 → PC

PC 收到: "hello" (原样回显)
```

---

## 8. 常见问题

### Q: PHY 自协商失败/链路灯不亮

1. 检查 PHY RESET 引脚初始化 (PD3)
2. 确认 RMII/MII 模式选择与 PHY 芯片匹配
3. 检查 50MHz/25MHz 参考时钟是否正常
4. 用 `phy_get_status()` 读取 PHY BSR 寄存器

### Q: 能 ping 通但不能 UDP 通信

1. 检查防火墙
2. 确认 IP 在同一子网
3. 用 Wireshark 抓包确认 ARP 解析是否正常

### Q: UDP 丢包严重

1. 增大 `PBUF_POOL_SIZE` (lwipopts.h)
2. 增加 RX 描述符数量 (eth_mac.c, 默认 5 个)
3. 缩短 `ethernetif_input()` 轮询间隔 (当前 10ms)
4. 改为中断驱动收包（当前为轮询模式）

### Q: 编译错误 `ENET_TDES0_DAV` 等宏未定义

这些是 GD32 专用宏，不是 STM32 的。确保 `#include "gd32f4xx_enet.h"` 而非 STM32 的 `stm32f4xx_eth.h`。参考 [gd32_vs_stm32.md](gd32_vs_stm32.md)。
