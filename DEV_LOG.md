# NINA 固件开发日志

## 版本: v3.1.0 - 流式传输优化
**日期**: 2026.05.30  
**分支**: v3.0.0  
**状态**: 开发中

---

## 📋 开发目标

将 NINA SPI 通信从"请求-响应"模式优化为"流式传输"模式，提升大数据传输效率。

---

## 🔍 问题分析

### 当前架构问题 (v2.0.1 / v3.0.0)

```
当前模式：请求-响应 (Request-Response)

K210 (主机)          ESP32 (从机)
    │                     │
    ├──── ① 发送命令 ────►│
    │                     │  处理命令（阻塞）
    │                     │
    ◄──── ② 拉取响应 ─────┤
    │                     │
```

**问题点：**
1. ❌ 每次命令需要 **2 次 SPI 传输**，效率低
2. ❌ **同步阻塞**模式，ESP32 处理期间 K210 无法做其他事
3. ❌ **单次包大小限制** (约 4076 字节 payload)
4. ❌ 大数据传输需要 K210 端手动分包

---

## 🚀 优化方案

### 架构设计

```
优化后：流式传输 (Streaming)

K210 (主机)          ESP32 (从机)
    │                     │
    ├─── ① 启动流 ──────►│
    │                     │  后台产生数据
    │                     │  → 写入环形缓冲区
    │                     │
    ◄─── ② 持续拉取 ──────┤
    │                     │
    ◄─── ③ 持续拉取 ──────┤
    │                     │
    ◄─── ④ 流结束 ────────┤
    │                     │
```

---

### 1. 新增命令定义 (0xF0-0xFF 范围)

| 命令号 | 命令名 | 说明 |
|---------|---------|------|
| `0xF0` | `STREAM_START` | 启动流式传输 |
| `0xF1` | `STREAM_PULL` | 拉取流数据 |
| `0xF2` | `STREAM_STATUS` | 查询流状态 |
| `0xF3` | `STREAM_ABORT` | 中止流传输 |

---

### 2. 协议格式

#### STREAM_START 请求 (K210 → ESP32)
```
[0xE0][0xF0][参数数量][原命令号][原参数...][0xEE]
```

#### STREAM_PULL 响应 (ESP32 → K210)
```
[0xE0][0xF1][标志位][数据长度][数据...][0xEE]
  │
  └── 标志位:
      bit0 = 1  更多数据 (MORE_DATA)
      bit1 = 1  流结束 (STREAM_END)
      bit2 = 1  错误 (ERROR)
```

#### STREAM_STATUS 响应
```
[0xE0][0xF2][状态][已传输字节(4)][总字节(4)][0xEE]
```

---

### 3. 核心数据结构

```cpp
// 环形缓冲区
typedef struct {
    uint8_t* buffer;
    uint32_t size;
    uint32_t head;      // 写入指针
    uint32_t tail;      // 读取指针
} ring_buffer_t;

// 流上下文
typedef struct {
    uint8_t  cmd;           // 原始命令号
    uint32_t total_size;    // 总数据量 (0 = 未知)
    uint32_t transferred;   // 已传输量
    bool     is_active;     // 流是否活跃
    bool     is_complete;   // 是否完成
    uint8_t  error_code;    // 错误码
    
    ring_buffer_t ring_buf; // 环形缓冲区
    xSemaphore_t mutex;     // 互斥锁
} stream_context_t;
```

---

### 4. 适用场景

| 命令 | 当前方式 | 流式方式 | 收益 |
|------|---------|---------|------|
| `getDataTcp(0x2C)` | 每次最多 4KB，多次调用 | 一次启动，持续拉取 | SPI 传输减少 50% |
| `scanNetworks(0x27)` | 阻塞等待扫描完成 | 后台扫描，流式返回 | K210 可并行处理 |
| `readFile(0x61)` | 分包读取 | 连续传输 | 吞吐量提升 30-50% |

---

### 5. 错误处理

| 场景 | 处理策略 |
|------|---------|
| **缓冲区溢出** | 设置 OVERFLOW 标志，K210 加快拉取 |
| **数据超时** | 超过 5 秒无新数据，返回超时 |
| **传输中断** | 查询 STREAM_STATUS 断点续传 |
| **命令错误** | 返回错误码，流自动终止 |

---

## 📁 文件变更计划

| 文件 | 操作 | 说明 |
|------|------|------|
| `main/stream_handler.h` | 新增 | 流控制模块头文件 |
| `main/stream_handler.cpp` | 新增 | 流控制模块实现 |
| `main/CommandHandler.h` | 修改 | 集成流命令声明 |
| `main/CommandHandler.cpp` | 修改 | 集成流命令处理 |
| `main/sketch.ino.cpp` | 修改 | SPI 循环流状态处理 |
| `CHANGELOG` | 修改 | 版本变更记录 |
| `README.md` | 修改 | 流式传输使用说明 |

---

## ✅ 兼容性保证

1. **向后兼容**: 原有命令 (0x10-0x7F) 保持不变
2. **可选启用**: K210 可选择是否使用流式传输
3. **渐进迁移**: 先实现 TCP 数据流，再扩展到其他场景

---

## 📊 性能预估

| 指标 | 当前模式 | 流式模式 | 提升 |
|------|---------|---------|------|
| SPI 传输次数 | N×2 次 | N+1 次 | **-50%** |
| CPU 利用率 | K210 等待阻塞 | K210 并行处理 | **显著提升** |
| 吞吐量 | 受命令开销限制 | 接近 SPI 极限 | **+30~50%** |

---

## 🚧 开发进度

- [x] 方案设计与文档
- [x] 核心模块: stream_handler.h/cpp
- [x] 集成: CommandHandler 流命令
- [x] 集成: sketch.ino SPI 循环（无需修改，通过 CommandHandler 自动集成）
- [ ] 单元测试
- [x] 文档更新
- [x] 代码审查
- [ ] 编译验证

## 📝 已实现功能

### 核心模块 (stream_handler.h/cpp)
- [x] 环形缓冲区实现 (32KB)
- [x] 流上下文管理 + 任务句柄
- [x] 互斥锁保护，线程安全
- [x] stream_init/deinit/start/pull/status/abort 接口
- [x] stream_write_data/set_complete/set_error 接口

### 三个流式传输场景实现
- [x] **TCP 数据流** (0x2C) - tcp_data_stream_task
  - 支持 TCP/UDP/TLS 三种模式
  - 批量读取 (1024字节/次)，替代原单字节读取
  - 自动检测可用数据，实时写入环形缓冲区
- [x] **WiFi 扫描流** (0x27) - scan_networks_stream_task
  - 后台扫描，流式返回每个 AP 信息
  - 每个 AP 包含: SSID + RSSI + 加密类型 + BSSID + 信道
- [x] **文件读取流** (0x61) - read_file_stream_task
  - 支持文件名解析
  - 批量读取 (1024字节/次)，自动写入缓冲区
  - 错误处理：文件打开失败返回错误码

### CommandHandler 集成
- [x] 0xF0 STREAM_START 命令
- [x] 0xF1 STREAM_PULL 命令
- [x] 0xF2 STREAM_STATUS 命令
- [x] 0xF3 STREAM_ABORT 命令
- [x] 扩展命令范围至 0xF0-0xFF
- [x] 自动识别 0x2C/0x27/0x61 并启动对应后台任务

---

## 🐛 Bug 修复记录 (2026.05.30)

### 修复的问题

| 序号 | Bug 描述 | 位置 | 修复方案 |
|------|---------|------|---------|
| 1 | **TCP socket 越界** - param_len < 2 或 socket >= 10 时崩溃 | `tcp_data_stream_task` | 增加参数长度检查 + socket 范围校验 |
| 2 | **文件参数解析越界** - param_offset 无边界检查 | `read_file_stream_task` | 新增 `CHECK_PARAM_BOUND` 宏，每次访问前检查 |
| 3 | **任务句柄泄漏** - stream_abort 只清空句柄不删除任务 | `stream_abort` | 保存旧句柄，调用 `vTaskDelete(old_task)` 安全清理 |
| 4 | **环形缓冲满数据丢失** - 缓冲区满时直接丢弃数据 | `tcp_data_stream_task` / `read_file_stream_task` | 增加重试机制（最多 50 次 = 1 秒等待） |
| 5 | **WiFi BSSID 指针失效** - WiFi.BSSID() 返回指针可能失效 | `scan_networks_stream_task` | 拷贝 BSSID 到本地栈数组 |
| 6 | **SSID 长度无限制** - SSID 过长导致 ap_info[256] 溢出 | `scan_networks_stream_task` | 限制 SSID 最大 32 字节 |
| 7 | **streamPullCmd 响应格式错误** - 数据和长度字段位置冲突 | `streamPullCmd` | 修正参数位置：flags→response[4], length→response[5], data→response[6] |

### 错误码定义

| 错误码 | 含义 |
|---------|------|
| 1 | 文件打开失败 |
| 2 | 参数越界 / 参数不足 |
| 3 | socket 号超出范围 |

### CMake 集成
- [x] 添加 stream_handler.cpp 到编译列表

---

## 📝 备注

- 开发分支: `v3.0.0`
- 目标版本: `v3.1.0`
- 风险等级: 中 (需充分测试兼容性)
