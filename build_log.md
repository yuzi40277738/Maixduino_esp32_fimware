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