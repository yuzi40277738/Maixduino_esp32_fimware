# Windows 编译与烧录指南（Maixduino / ESP32）

本文档记录在本机（Windows + ESP-IDF 5.5 + EIM 安装）上编译 nina-fw 并烧录至 ESP32 协处理器的步骤与工具调用方法。

## 环境路径（本机）

| 项目 | 路径 |
|------|------|
| ESP-IDF | `D:\IDF\.espressif\v5.5\esp-idf` |
| 工具链根目录 | `C:\Espressif\tools` |
| Python 虚拟环境 | `C:\Espressif\tools\python\v5.5\venv` |
| 环境激活脚本 | `C:\Espressif\tools\Microsoft.v5.5.PowerShell_profile.ps1` |
| 项目目录 | `D:\K210_Test_App\nina-fw` |

> **注意：** 不要直接使用 `D:\IDF\.espressif\v5.5\esp-idf\export.bat`。本机该脚本会查找不存在的 Python 环境（`C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env`），导致 `idf.py` 无法运行。应使用 EIM 生成的 PowerShell 激活脚本。

## 1. 激活 ESP-IDF 环境

在 **PowerShell** 中执行（每次新开终端都需要）：

```powershell
. C:\Espressif\tools\Microsoft.v5.5.PowerShell_profile.ps1
cd D:\K210_Test_App\nina-fw
```

激活成功后终端会显示 `IDF PowerShell Environment`，并可用以下命令：

| 命令 | 用途 |
|------|------|
| `idf.py` | 编译、烧录、监视串口 |
| `esptool.py` | 直接烧录 / 擦除 Flash |
| `espefuse.py` | 读写 eFuse |
| `espsecure.py` | 签名 / 加密固件 |

## 2. 编译固件

Maixduino 使用 `esp32` 板型：

```powershell
idf.py -DBOARD=esp32 build
```

编译成功后主要产物：

| 文件 | 说明 |
|------|------|
| `build/nina-fw.bin` | 应用固件（烧录地址 `0x30000`） |
| `build/bootloader/bootloader.bin` | Bootloader（烧录地址 `0x1000`） |
| `build/partition_table/partition-table.bin` | 分区表（烧录地址 `0x8000`） |

构建完成后 CMake 会自动调用 `combine.py` 生成合并固件（可选，用于整片烧录）。

## 3. 烧录至 COM 口

### 3.1 查看可用串口

```powershell
[System.IO.Ports.SerialPort]::getportnames()
```

Maixduino 上 ESP32 的 USB 转串口在本机通常为 **COM16**（以设备管理器为准）。

### 3.2 推荐方式：esptool 115200 波特率

本机实测 **460800 默认波特率连接不稳定**，**115200 + 多次重试** 最可靠：

```powershell
. C:\Espressif\tools\Microsoft.v5.5.PowerShell_profile.ps1
cd D:\K210_Test_App\nina-fw

esptool.py --chip esp32 -p COM16 -b 115200 `
  --connect-attempts 30 `
  --before default_reset --after hard_reset `
  write_flash --flash_mode dio --flash_freq 40m --flash_size 2MB `
  0x1000  build/bootloader/bootloader.bin `
  0x30000 build/nina-fw.bin `
  0x8000  build/partition_table/partition-table.bin
```

将 `COM16` 替换为实际端口。

### 3.3 一键编译 + 烧录

```powershell
. C:\Espressif\tools\Microsoft.v5.5.PowerShell_profile.ps1
cd D:\K210_Test_App\nina-fw

idf.py -DBOARD=esp32 build
esptool.py --chip esp32 -p COM16 -b 115200 --connect-attempts 30 `
  --before default_reset --after hard_reset `
  write_flash --flash_mode dio --flash_freq 40m --flash_size 2MB `
  0x1000  build/bootloader/bootloader.bin `
  0x30000 build/nina-fw.bin `
  0x8000  build/partition_table/partition-table.bin
```

### 3.4 使用 idf.py flash（备选）

若连接稳定，也可：

```powershell
idf.py -p COM16 -b 115200 flash
```

默认 `idf.py flash` 使用 460800 波特率，本机可能报 `Failed to connect to ESP32: No serial data received`，此时改用 3.2 节的 `esptool.py` 命令。

## 4. 烧录成功标志

终端输出类似以下内容即表示成功：

```
Chip is ESP32-D0WDQ6 (revision v1.0)
MAC: fc:f5:c4:0a:94:a4
...
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

## 5. 串口监视（可选）

查看 ESP32 启动日志：

```powershell
idf.py -p COM16 monitor
```

退出监视：`Ctrl + ]`

## 6. 常见问题

### 连接失败：`No serial data received`

1. 确认 COM 口号正确（设备管理器）
2. 关闭占用串口的程序（串口助手、其他 monitor 等）
3. 改用 **115200** 波特率和 `--connect-attempts 30`
4. 手动进入下载模式：按住 **BOOT/IO0**，点 **RESET**，松开 RESET 后再松开 BOOT
5. 确认 K210 未占用 ESP32 串口

### 写入中断：`The chip stopped responding`

- 多为波特率过高或 USB 线/Hub 不稳定
- 改用 115200 重新完整烧录（会覆盖之前未完成的写入）

### `export.bat` / `idf.py` 找不到

使用 EIM 激活脚本，勿用默认 `export.bat`：

```powershell
. C:\Espressif\tools\Microsoft.v5.5.PowerShell_profile.ps1
```

### VS Code ESP-IDF 扩展

`.vscode/settings.json` 中已配置：

- `idf.currentSetup`: `D:\IDF\.espressif\v5.5\esp-idf`
- `idf.flashType`: `UART`

在 VS Code 中选择 COM 口后，建议将 Flash Baud Rate 设为 **115200** 以提高烧录成功率。

## 7. 命令速查

```powershell
# 激活环境
. C:\Espressif\tools\Microsoft.v5.5.PowerShell_profile.ps1
cd D:\K210_Test_App\nina-fw

# 仅编译
idf.py -DBOARD=esp32 build

# 编译并烧录 COM16（推荐参数）
idf.py -DBOARD=esp32 build; esptool.py --chip esp32 -p COM16 -b 115200 --connect-attempts 30 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 40m --flash_size 2MB 0x1000 build/bootloader/bootloader.bin 0x30000 build/nina-fw.bin 0x8000 build/partition_table/partition-table.bin

# 擦除整片 Flash（慎用）
esptool.py --chip esp32 -p COM16 erase_flash

# 读取芯片信息
esptool.py --chip esp32 -p COM16 chip_id
```
