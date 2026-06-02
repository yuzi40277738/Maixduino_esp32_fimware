# Maixduino nina-fw 开发日志

> 发布版本以 [CHANGELOG](CHANGELOG) 为准；本文件记录会话级决策与验证。

---

## 2026-05-31 — 退回流式传输 → v3.3.2

### 背景

v3.3.0–v3.3.1 引入流式 SPI 架构（`0xF0`–`0xF3` + `stream_handler` 32KB 环形缓冲 + 后台任务），目标减少 TCP/WiFi 扫描/文件读取的 SPI 往返次数。联调结论：**架构失败**，K210/ESP32 协同不稳定，故整体回退。

### 代码变更摘要

| 操作 | 文件 |
|------|------|
| 删除 | `main/stream_handler.h`, `main/stream_handler.cpp` |
| 恢复 | `CommandHandler::handle()` 仅 `command[1] < NUM_COMMAND_HANDLERS` |
| 更新 | `main/CMakeLists.txt`, `README.md`, `CHANGELOG` |
| 版本 | `FIRMWARE_VERSION` → **3.3.2** |

### 编译与烧录

```text
板型:     esp32 (Maixduino)
编译:     idf.py -DBOARD=esp32 build  → OK
应用大小: build/nina-fw.bin ≈ 0xfe450 (较流式版略小)
烧录:     COM14, 115200, esptool write_flash
          0x1000  bootloader
          0x30000 nina-fw.bin
          0x8000  partition-table
芯片:     ESP32-D0WDQ6 v1.0, MAC 24:6f:28:95:5e:70
```

### 串口启动（参考）

上电后可见 ROM 引导信息；若 Flash 为 4MB 而镜像头为 2MB，会出现：

```text
W spi_flash: Detected size(4096k) larger than the size in the binary image header(2048k).
```

属预期警告，不影响当前分区布局。

### 后续

- K210 网关固件勿再调用 `0xF0`–`0xF3`
- 性能优化若需重做，宜在 K210 侧批量拼包或缩小单次 SPI 负载，而非 ESP32 侧双协议栈

---

## 2026-05-30 — 流式传输尝试（已废弃，见 v3.3.2）

- v3.3.0: 新增 `stream_handler` 与 `0xF0`–`0xF3`
- v3.3.1: `NetworkClient` API 编译修复
- 详见 CHANGELOG `3.3.0-maixduino-v3.2` / `3.3.1-maixduino` 条目（历史保留，勿再启用）
