# 工具链安装指南

从零搭建 GD32F450 交叉编译 + 调试环境（Windows / Git Bash）。

**目标**：安装完本指南后，`make` 能编译出固件，`make flash` 能烧录，`make test` 能跑单元测试。

---

## 1. 环境前提

- Windows 10/11 x64
- Git for Windows（提供 Git Bash 终端, 内含 MSYS2/MINGW64）
- 网络可访问 GitHub（已配置梯子）

---

## 2. ARM GNU Toolchain（交叉编译器）

### 2.1 下载

从 ARM Developer 官网下载 Windows 版：

| 项目 | 值 |
|------|-----|
| 版本 | **Arm GNU Toolchain 13.3.Rel1** (13.3.1 20240614) |
| 文件 | `arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi.zip` (285 MB) |
| 地址 | https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads |

### 2.2 安装

```bash
# 解压到 D:\tools
mkdir -p /d/tools
unzip arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi.zip -d /d/tools/
```

### 2.3 环境变量

```bash
echo 'export PATH="/d/tools/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 2.4 验证

```bash
arm-none-eabi-gcc --version
# arm-none-eabi-gcc.exe (Arm GNU Toolchain 13.3.Rel1) 13.3.1 20240614
```

---

## 3. GNU Make

### 3.1 安装

Git for Windows 不自带 make，需从 ezwinports 下载：

| 项目 | 值 |
|------|-----|
| 版本 | **GNU Make 4.4.1** |
| 文件 | `make-4.4.1-without-guile-w32-bin.zip` (384 KB) |
| 地址 | https://sourceforge.net/projects/ezwinports/ |

```bash
# 解压 make.exe 到 Git Bash 的 /usr/bin/
unzip -j make-4.4.1-without-guile-w32-bin.zip -d /usr/bin/
```

### 3.2 验证

```bash
make --version
# GNU Make 4.4.1
```

---

## 4. OpenOCD（调试烧录）

### 4.1 下载

使用 xPack 预编译版（比官方 Release 更新更全）：

| 项目 | 值 |
|------|-----|
| 版本 | **xPack OpenOCD v0.12.0-7** |
| 文件 | `xpack-openocd-0.12.0-7-win32-x64.zip` (~3 MB) |
| 地址 | https://github.com/xpack-dev-tools/openocd-xpack/releases |

### 4.2 安装

```bash
unzip xpack-openocd-0.12.0-7-win32-x64.zip -d /d/tools/
```

### 4.3 环境变量

```bash
echo 'export PATH="/d/tools/xpack-openocd-0.12.0-7/bin:$PATH"' >> ~/.bashrc
```

### 4.4 验证

```bash
openocd --version
# xPack Open On-Chip Debugger 0.12.0+dev
```

### 4.5 配置说明

xPack OpenOCD 的脚本文件在 `openocd/scripts/` 而非 `share/openocd/scripts/`。Makefile 中通过 `-s` 参数指定：

```makefile
OPENOCD_SCRIPTS = /d/tools/xpack-openocd-0.12.0-7/openocd/scripts
$(OPENOCD) -s $(OPENOCD_SCRIPTS) -f scripts/openocd/gd32f4x_jlink.cfg
```

GD32F4xx 没有专用 target 脚本，项目使用 STM32F4x 脚本站替（SWD 兼容）。

---

## 5. 主机 GCC（单元测试用）

### 5.1 下载

用 WinLibs 独立 GCC（无需 MSYS2，解压即用）：

| 项目 | 值 |
|------|-----|
| 版本 | **WinLibs GCC 16.1.0** (MinGW-W64 x86_64-ucrt-posix-seh, r2) |
| 文件 | `winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r2.zip` (262 MB) |
| 地址 | https://github.com/brechtsanders/winlibs_mingw/releases |

### 5.2 安装

```bash
unzip winlibs-x86_64-*-gcc-16.1.0-*.zip -d /d/tools/mingw64/
```

> **注意**：WinLibs 的 zip 内含 `mingw64/` 顶层目录，解压后路径为 `/d/tools/mingw64/mingw64/bin/gcc.exe`。

### 5.3 环境变量

```bash
echo 'export PATH="/d/tools/mingw64/mingw64/bin:$PATH"' >> ~/.bashrc
```

### 5.4 验证

```bash
gcc --version
# gcc.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r2) 16.1.0
```

---

## 6. 最终环境验证

```bash
# 关闭并重新打开 Git Bash，然后：
source ~/.bashrc

echo "=== ARM GCC ===" && arm-none-eabi-gcc --version | head -1
echo "=== GNU Make ===" && make --version | head -1
echo "=== OpenOCD ===" && openocd --version | head -1
echo "=== Host GCC ===" && gcc --version | head -1
```

预期输出四个工具的版本信息，无报错。

---

## 7. 编译项目

```bash
cd /e/zoulang/code_proj/embedded_proj
make clean
make -j8
```

预期输出：

```
[CC] system_gd32f4xx.c
[CC] gd32f4xx_rcu.c
...
[AS] startup_gd32f450_470.S
[LD] build/firmware.elf
   text	   data	    bss	    dec	    hex
  53484	   1780	 129384	 184648	  2d148
[HEX] build/firmware.hex
[BIN] build/firmware.bin
=== Memory Usage ===
```

---

## 8. 烧录固件

```bash
make flash
```

前提：
- J-Link 调试器已通过 USB 连接
- J-Link 驱动已安装（从 https://www.segger.com/downloads/jlink/ 下载）
- GD32F450 开发板已上电

---

## 9. 调试

```bash
make debug
```

这会：
1. 后台启动 OpenOCD（连接 J-Link → GD32F450）
2. 启动 GDB，自动连接 OpenOCD
3. 加载固件到 Flash
4. 停在 `main()` 入口

常用 GDB 命令：

```
break main          # 在 main 断点
continue            # 继续运行
info registers      # 查看寄存器
monitor reset halt  # 复位并暂停
```

---

## 10. 常见问题

### Q: `arm-none-eabi-gcc: command not found`

检查 PATH：`echo $PATH | tr ':' '\n' | grep arm`，确认路径正确后 `source ~/.bashrc`。

### Q: `make: command not found`

Git Bash 默认不带 make。按步骤 3 安装 ezwinports make。

### Q: OpenOCD 报 `Error: unable to find a matching CMSIS-DAP device`

检查 J-Link 驱动是否正确安装，USB 线是否连接，开发板是否上电。

### Q: GCC 下载失败/太慢（GitHub 超时）

WinLibs 也托管在 SourceForge：
https://sourceforge.net/projects/winlibs-mingw/files/

GitHub 下载慢可尝试在浏览器中下载后放到 `download/` 目录。

### Q: 链接警告 `_write/_fstat/_isatty not implemented`

**正常**，不影响功能。裸机环境无文件系统，C 标准库的 I/O stub 函数没有实现。项目不使用 `printf`/文件操作。
