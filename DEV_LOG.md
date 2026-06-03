# Maixduino nina-fw 开发日志

> 发布版本以 [CHANGELOG](CHANGELOG) 为准；本文件记录会话级决策与验证。

---

## 2026-06-03 — NTP 时间同步优化

### 背景

原有 NTP 配置存在以下问题：
1. 服务器不可靠 — `cn.pool.ntp.org` 为志愿服务器池，质量参差不齐
2. 无时区设置 — `time()` 返回 UTC 时间，与北京时间差 8 小时
3. 首次同步慢 — SNTP 默认首次轮询需 64 秒
4. 无同步回调 — 无法确认 NTP 何时同步完成
5. 重同步间隔过长 — 12 小时才重新同步一次

### 代码变更摘要

| 操作 | 文件 | 说明 |
|------|------|------|
| 修改 | `main/CommandHandler.cpp` `_setupNTP()` | 替换 NTP 服务器、加时区、加回调、加立即同步 |
| 修改 | `main/sketch.ino.cpp` `loop()` | 重同步间隔从 12h 改为 1h |

### NTP 服务器变更

| 优先级 | 旧 | 新 |
|--------|----|----|
| 0 | `0.cn.pool.ntp.org` | `ntp.aliyun.com`（阿里云） |
| 1 | `1.time1.aliyun.com` | `ntp.tencent.com`（腾讯云 CDN） |
| 2 | `2.ntp.ntsc.ac.cn` | `time.windows.com`（微软全球 CDN） |

### 新增功能

1. **时区设置** — `setenv("TZ", "CST-8", 1)` + `tzset()`，ESP32 侧 `localtime()` 输出北京时间
2. **同步回调** — `_ntpSyncCallback()`，同步完成后串口打印 `NTP synced: YYYY-MM-DD HH:MM:SS`
3. **立即同步** — `esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED)`，首次连接几秒内完成同步，之后自动切回正常轮询
4. **同步标志** — `s_ntpSynced` 变量，供其他模块判断 NTP 是否已同步

### 注意事项

- `time(&now)` 返回的 Unix 时间戳始终为 UTC，**不受时区影响**
- K210 端的 +28800 秒转北京时间逻辑**无需改动**
- 时区仅影响 ESP32 侧的 `localtime()` 和日志输出

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
