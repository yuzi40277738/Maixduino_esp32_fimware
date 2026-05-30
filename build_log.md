# nina-fw 编译日志

**编译时间:** 2026-05-24 21:37 ~ 22:33
**目标板:** esp32
**IDF 版本:** v5.5
**编译器:** xtensa-esp-elf-gcc 14.2.0

---

## 一、编译结果

| 文件 | 大小 | 状态 |
|------|------|------|
| `build_esp32/nina-fw.bin` | 0x10f380 (1.08MB) | ✅ 已生成 |
| `build_esp32/nina-fw.elf` | - | ✅ 已生成 |
| `build_esp32/bootloader/bootloader.bin` | 0x4710 (18KB) | ✅ 已生成 |
| `build_esp32/partition_table/partition-table.bin` | - | ✅ 已生成 |

**固件分区占用:** 0x10f380 / 0x180000 bytes (71% 已用, 29% 空闲)
**Bootloader 占用:** 0x4710 / 0x10000 bytes (63% 已用, 37% 空闲)

---

## 二、修复的问题（共 4 项）

### 1. PowerShell Profile 语法错误

- **文件:** `C:\Espressif\tools\Microsoft.v5.5.PowerShell_profile.ps1`
- **问题:** 环境变量 `_IDF.PY_COMPLETE` 包含点号（`.`），使用 `$env:'VAR'` 语法报错
- **修复:** 将 `$env:'_IDF.PY_COMPLETE'` 改为 `${env:_IDF.PY_COMPLETE}`
- **涉及行:** 82, 83, 89, 91

### 2. xtensa-esp-elf 工具链不完整

- **问题:** `libexec/gcc/xtensa-esp-elf/14.2.0/` 目录缺少 `cc1.exe`、`cc1plus.exe` 等核心编译器文件
- **原因:** 工具链安装不完整或部分文件被误删除
- **修复:** 从本地缓存 `C:\Espressif\tools\dist\xtensa-esp-elf-14.2.0_20241119-x86_64-w64-mingw32.zip` 重新解压完整工具链

### 3. CMake 路径转义问题

- **文件:** `main/CMakeLists.txt`
- **问题:** `$ENV{IDF_PATH}/components/esp_netif` 在 Windows 上解析为 `D:\IDF\.espressif\v5.5\esp-idf`，其中 `\v` 被 CMake 解释为垂直制表符转义序列
- **修复:** 使用 `file(TO_CMAKE_PATH "$ENV{IDF_PATH}" IDF_PATH_CMAKE)` 将路径正斜杠化

### 4. http_client.c SCM 元数据注入

- **文件:** `main/http_client.c:16`
- **问题:** VS Code 意外将 SCM 历史记录 URL 字符串 `{scm-history-item:...}` 注入到代码中
- **修复:** 删除该 URL 字符串，恢复正常的 `if (buffer == NULL) { return -1; }` 逻辑

---

## 三、烧录命令

```bash
# 通过 idf.py（自动检测端口）
idf.py -p COM端口 flash

# 或直接通过 esptool.py
python -m esptool --chip esp32 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 2MB --flash_freq 40m ^
  0x1000 build_esp32/bootloader/bootloader.bin ^
  0x8000 build_esp32/partition_table/partition-table.bin ^
  0x30000 build_esp32/nina-fw.bin
```

---

## 四、系统环境

- **操作系统:** Windows 10
- **ESP-IDF 路径:** D:\IDF\.espressif\v5.5\esp-idf
- **Python 环境:** C:\Espressif\tools\python\v5.5\venv
- **构建系统:** Ninja + CMake 3.30.2

---

## 五、2026-05-25 编译与烧录记录

**日期:** 2026-05-25
**Git 提交:** `873dd3a` — //替换为国内服务器
**目标板:** esp32 (Maixduino / K210 + ESP32)
**构建目录:** `build/`（非 `build_esp32/`）

### 5.1 编译

**命令:**
```powershell
$env:IDF_TOOLS_PATH = "C:\Espressif\tools"
idf.py -B build -DBOARD=esp32 fullclean
idf.py -B build -DBOARD=esp32 build
```

**结果:** ✅ 成功（约 24 分钟）

| 文件 | 大小 | 状态 |
|------|------|------|
| `build/nina-fw.bin` | 0x10f380 (1.08 MB) | ✅ 已生成 |
| `build/nina-fw.elf` | - | ✅ 已生成 |
| `build/bootloader/bootloader.bin` | 0x4710 (18 KB) | ✅ 已生成 |
| `build/partition_table/partition-table.bin` | 0xC00 (3 KB) | ✅ 已生成 |

**固件分区占用:** 0x10f380 / 0x180000 bytes (71% 已用, 29% 空闲)
**Bootloader 占用:** 0x4710 / 0x10000 bytes (63% 已用, 37% 空闲)

**编译前问题:** 首次 `idf.py build` 因 CMake 缓存路径不一致失败（旧路径 `d:/nina-fw` vs 当前 `D:/K210_Test_App/nina-fw`），`fullclean` 后重建成功。

**环境变量:** 需设置 `IDF_TOOLS_PATH=C:\Espressif\tools`（否则报 `espidf.constraints.v5.5.txt` 不存在）。

### 5.2 烧录

#### 烧录尝试记录

| 序号 | 工具 | 端口 | 波特率 | 结果 | 错误 / 说明 |
|------|------|------|--------|------|-------------|
| 1 | idf.py | COM13 | 460800 | ❌ 失败 | `Failed to connect to ESP32: No serial data received`（误用 K210 口） |
| 2 | ESP Flash Download Tool | COM13 | 高波特率 | ❌ 失败 | `8-download data fail`（通信不稳定，端口或波特率不对） |
| 3 | idf.py | COM14 | 115200 | ✅ 成功 | 识别 ESP32-D0WDQ6，三文件 Hash verified |

#### 成功烧录命令（idf.py）

```powershell
$env:IDF_TOOLS_PATH = "C:\Espressif\tools"
idf.py -B build -DBOARD=esp32 -p COM14 -b 115200 flash
```

等效 esptool 命令（在 `build/` 目录下执行）：

```powershell
python -m esptool --chip esp32 -p COM14 -b 115200 ^
  --before default_reset --after hard_reset write_flash ^
  --flash_mode dio --flash_freq 40m --flash_size 2MB ^
  0x1000 bootloader/bootloader.bin ^
  0x8000 partition_table/partition-table.bin ^
  0x30000 nina-fw.bin
```

或使用 `build/flash_args` 一键烧录：

```powershell
cd build
python -m esptool --chip esp32 -p COM14 -b 115200 --before default_reset --after hard_reset write_flash @flash_args
```

`build/flash_args` 内容：

```
--flash_mode dio --flash_freq 40m --flash_size 2MB
0x1000 bootloader/bootloader.bin
0x30000 nina-fw.bin
0x8000 partition_table/partition-table.bin
```

#### ESP Flash Download Tool 配置（GUI 烧录）

| 配置项 | 值 |
|--------|-----|
| chipType | ESP32 |
| COM 口 | **COM14**（ESP32 口，非 K210 的 COM13） |
| BAUD | **115200**（失败时可试 74880） |
| SPI SPEED | 40 MHz |
| SPI MODE | DIO |
| Flash Size | 2 MB |

| 文件路径 | 烧录地址 | 说明 |
|----------|----------|------|
| `build/bootloader/bootloader.bin` | `0x1000` | Bootloader，大小 0x4710（约 18 KB） |
| `build/partition_table/partition-table.bin` | `0x8000` | 分区表 |
| `build/nina-fw.bin` | `0x30000` | 主应用固件，大小 0x10f380（约 1.08 MB） |

**常见报错:** `8-download data fail` → 降低波特率至 115200，确认 COM 口为 ESP32，检查 USB 数据线。

#### 设备信息（2026-05-25 烧录时 esptool 识别）

| 项目 | 值 |
|------|-----|
| 芯片 | ESP32-D0WDQ6 (revision v1.0) |
| 特性 | WiFi, BT, Dual Core, 240 MHz |
| 晶振 | 40 MHz |
| MAC 地址 | `24:6f:28:95:5e:70` |
| esptool 版本 | v4.12.dev2 |

#### 写入详情与校验

| 文件 | 地址 | 原始大小 | 压缩后 | 耗时 | Hash |
|------|------|----------|--------|------|------|
| `bootloader/bootloader.bin` | 0x1000 | 18192 B (0x4710) | 12290 B | 1.4 s | ✅ verified |
| `nina-fw.bin` | 0x30000 | 1110912 B (0x10f380) | 727748 B | 65.8 s | ✅ verified |
| `partition_table/partition-table.bin` | 0x8000 | 3072 B (0xC00) | 135 B | 0.1 s | ✅ verified |

**Flash 擦除范围:**
- 0x00001000 ~ 0x00005fff（bootloader）
- 0x00008000 ~ 0x00008fff（分区表）
- 0x00030000 ~ 0x0013ffff（主固件）

**烧录完成后:** esptool 通过 RTS 引脚硬复位，设备自动重启。

#### 分区表（partitions.csv）

| 分区名 | 类型 | 偏移 | 大小 | 说明 |
|--------|------|------|------|------|
| nvs | data/nvs | 0x9000 | 0x6000 | NVS 存储 |
| phy_init | data/phy | 0xF000 | 0x1000 | PHY 初始化 |
| certs | data | 0x10000 | 0x20000 | 证书 |
| factory | app/factory | **0x30000** | 0x180000 | 主应用（nina-fw.bin 烧录于此） |
| storage | data/spiffs | 0x1B0000 | 0x40000 | SPIFFS 文件系统 |

**固件占用:** 0x10f380 / 0x180000 = 71% 已用，29% 空闲。

#### 烧录前检查清单

1. ✅ 使用 **ESP32** 对应的 COM 口（本机为 COM14）
2. ✅ 关闭占用串口的程序（monitor、Arduino IDE 等）
3. ✅ 使用数据线（非纯充电线）
4. ✅ 必要时手动进入下载模式：按住 BOOT → 按 RESET → 松开 RESET → 松开 BOOT
5. ✅ 波特率优先 **115200**

#### 烧录后验证

```powershell
# 串口监视（ESP32 口）
idf.py -p COM14 monitor

# 退出监视器: Ctrl+]
```

**2026-05-25 串口捕获:** COM14 @ 115200，复位后 8 秒内无 UART 输出（Release 构建 `setDebug(0)` 关闭调试）。功能验证需 K210 侧 SPI 通信或 Debug 构建。

### 5.2.1 Maixduino 双 USB 串口（K210 / ESP32）

Maixduino 板载 **两个 USB 转串口**，在 Windows 设备管理器中端口号通常较大，例如 **COM13、COM14** 等。**一个是 K210，一个是 ESP32**，不可混用。

| COM 口 | 对应芯片 | 用途 | 本次实测 |
|--------|----------|------|----------|
| **COM14** | **ESP32** | 烧录 NINA 固件、ESP32 串口调试 | ✅ `idf.py flash` 识别 ESP32-D0WDQ6，烧录成功 |
| **COM13** | **K210**（推测） | K210 程序下载 / 调试 | ❌ 对 ESP32 烧录报 `No serial data received` |

**如何区分两个口（端口号每次可能变化）：**

1. 设备管理器 → 端口(COM 和 LPT)，拔掉 USB 再看哪个 COM 消失。
2. 对疑似 ESP32 的口执行 `idf.py -p COMx flash`，若出现 `Chip is ESP32` 即为 ESP32 口。
3. K210 口用于 MaixPy / K210 固件下载，不能用来烧 ESP32 NINA 固件。

**烧录 NINA 固件务必使用 ESP32 对应的 COM 口：**

```powershell
$env:IDF_TOOLS_PATH = "C:\Espressif\tools"
idf.py -p COM14 -b 115200 flash    # COM14 为本机实测 ESP32 口，请以实际为准
```

**进入 ESP32 下载模式：** 按住 BOOT → 按 RESET → 松开 RESET → 松开 BOOT，然后立即烧录。

### 5.3 串口启动日志

详见 **§5.2 烧录后验证**。简要：`idf.py -p COM14 monitor`，Release 构建默认无 UART 调试输出。

### 5.4 IDE 配置修复（IntelliSense 误报）

| 文件 | 修改 |
|------|------|
| `.vscode/c_cpp_properties.json` | 修正 compilerPath 为 `C:/Espressif/tools/...`；添加工具链 include；指向 `build/compile_commands.json` |
| `.vscode/settings.json` | 添加 `C_Cpp.default.*` 与 `clangd.arguments` |
| `.clangd` | 过滤 ESP32 专用 GCC 参数；使用 `build/` 编译数据库 |
| `main/sketch.ino.cpp` | 移除未使用的 `esp_log.h`、`esp_partition.h` |

### 5.5 常用命令速查

```powershell
# 环境
$env:IDF_TOOLS_PATH = "C:\Espressif\tools"

# 编译
idf.py -B build -DBOARD=esp32 build

# 烧录（推荐 115200）
idf.py -p COM14 -b 115200 flash

# 编译 + 烧录
idf.py -p COM14 -b 115200 build flash

# 串口监视
idf.py -p COM14 monitor
```

---

## 六、2026-05-30 MQTT TLS（Mosquitto CA 嵌入）编译与烧录

**日期:** 2026-05-30  
**项目:** `D:\K210_Test_App\nina-fw`（Maixduino NINA 固件，ESP32 + K210 SPI）  
**分支:** `nina-fw-v1.1.0`  
**Git 基线:** `e552cfc`（工作区另有未提交：MQTT CA 嵌入相关改动）  
**目标板:** `esp32`（Maixduino，ESP32 口 **COM16**）  
**构建目录:** `build/`  
**IDF:** v5.5，`IDF_TOOLS_PATH=C:\Espressif\tools`

### 6.1 改造目标

| 项目 | 说明 |
|------|------|
| TLS 模式 | K210 经 SPI 下发 NINA **`TLS_MODE`**，连接 Mosquitto **8883** |
| 鉴别方式 | **仅服务端鉴别**（单向 TLS）：ESP32 校验 Broker 的 `server.crt` |
| 信任根 | 使用 **MQTT 服务端** 签发的 CA，拷贝为 `certs/ca.crt` 编译嵌入固件 |
| 不做 | 在 ESP32 上生成 CA；不把 `ca.key` / `server.key` 打进固件 |

### 6.2 证书核对（与 Mosquitto 服务端一致）

**服务端配置目录（用户拷贝）:** `C:\Users\Administrator\Desktop\config`

| 文件 | 用途 |
|------|------|
| `ca.crt` | 签发 CA（嵌入 ESP32） |
| `server.crt` | Broker 证书（仅服务器使用） |
| `server.key` / `ca.key` | 私钥（**勿**放入固件仓库） |
| `mosquitto.conf` | `listener 8883`，`cafile` / `certfile` / `keyfile` 指向 `/mosquitto/config/` |

**核对结果:**

| 检查项 | 结果 |
|--------|------|
| `Desktop\config\ca.crt` → `nina-fw\certs\ca.crt` | ✅ 已覆盖为服务端同一份（SHA256 一致） |
| `server.crt` 签发者 | ✅ `CN=mosquitto-ca`，与 `ca.crt` 一致 |
| 原 `CA/ca.crt` 与桌面拷贝 | ⚠️ 曾不一致（本地多 22 字节，换行差异）；已用服务端文件替换 |

`certs/ca.crt` SHA256: `6C60A5DC33A2339876FA5F4202189DFA05053E828924E5F361EBF2665EF274E9`

### 6.3 代码改动摘要

| 文件 | 改动 |
|------|------|
| `main/CMakeLists.txt` | 存在 `certs/ca.crt` 时 `EMBED_TXTFILES`，定义 `NINA_MQTT_CA_EMBED` |
| `main/CommandHandler.cpp` | `TLS_MODE` 下 `setCACert(mqtt_ca_crt)`；否则仍用 Mozilla 根证书包 |
| `certs/ca.crt` | 从 Mosquitto 服务器拷贝 |
| `certs/README.md` | MQTT 单向 TLS 说明 |
| `CA/ca.crt` | 已删除（迁至 `certs/`） |

### 6.4 编译

**命令:**

```powershell
$env:IDF_TOOLS_PATH = "C:\Espressif\tools"
$env:IDF_PATH = "D:\IDF\.espressif\v5.5\esp-idf"
cd D:\K210_Test_App\nina-fw
idf.py -B build -DBOARD=esp32 build
```

**结果:** ✅ 成功

| 文件 | 大小 | 状态 |
|------|------|------|
| `build/nina-fw.bin` | 0xfe380 (1041280 B) | ✅ 已生成 |
| `build/nina-fw.elf` | - | ✅ 已生成 |
| `build/bootloader/bootloader.bin` | 0x4710 (18 KB) | ✅ 已生成 |
| `build/partition_table/partition-table.bin` | 0xC00 (3 KB) | ✅ 已生成 |
| `build/ca.crt.S` / `_binary_ca_crt_*` | 已链接进 `libmain` | ✅ CA 已嵌入 |

**固件分区占用:** 0xfe380 / 0x180000 bytes（约 66% 已用，34% 空闲）

### 6.5 烧录

**ESP32 串口:** COM16（用户确认；此前 COM14 在本机不可用）

**命令:**

```powershell
$env:IDF_TOOLS_PATH = "C:\Espressif\tools"
cd D:\K210_Test_App\nina-fw
idf.py -B build -DBOARD=esp32 -p COM16 -b 115200 flash
```

#### 烧录尝试记录

| 序号 | 端口 | 结果 | 说明 |
|------|------|------|------|
| 1 | COM14 | ❌ 失败 | 端口不存在 |
| 2 | COM16 | ❌ 失败 | `Invalid head of packet` / `No serial data received`（未进下载模式） |
| 3 | COM16 | ✅ 成功 | 用户配合 BOOT+RESET 后重试 |

#### 成功烧录详情（2026-05-30）

| 文件 | 地址 | 原始大小 | 压缩后 | 校验 |
|------|------|----------|--------|------|
| `bootloader/bootloader.bin` | 0x1000 | 0x4710 | - | Hash verified |
| `nina-fw.bin` | 0x30000 | 1041280 B | 672094 B（约 60.8 s） | Hash verified |
| `partition_table/partition-table.bin` | 0x8000 | 3072 B | 135 B | Hash verified |

**结束后:** esptool 经 RTS 硬复位，设备自动重启。

### 6.6 验证建议

1. K210 网关 Web：**TLS** 开启，端口 **8883**，Broker 主机名与 Mosquitto `server.crt` CN/SAN 一致。  
2. Mosquitto 监听 `8883`，`require_certificate false`（仅服务端证书）。  
3. 不要用 `setClientCert` / `setCertKey`（除非需要双向 TLS）。

### 6.7 常用命令（COM16）

```powershell
$env:IDF_TOOLS_PATH = "C:\Espressif\tools"
cd D:\K210_Test_App\nina-fw

# 仅编译
idf.py -B build -DBOARD=esp32 build

# 烧录
idf.py -B build -DBOARD=esp32 -p COM16 -b 115200 flash

# 编译 + 烧录
idf.py -B build -DBOARD=esp32 -p COM16 -b 115200 build flash
```