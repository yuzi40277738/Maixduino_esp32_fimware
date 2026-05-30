# MQTT TLS：服务端鉴别（单向 TLS）

ESP32 作为客户端，只需**校验 MQTT 服务端**身份（服务端鉴别），不需要在设备上生成证书，也不需要客户端证书双向认证。

## 做法

1. 在 **MQTT 服务端**（如 Mosquitto）生成 CA 并签发 `server.crt`（流程只在服务器上做）。
2. 将服务端目录中的 **`ca.crt`**（签发 `server.crt` 的那张 CA）复制到本目录：

   ```
   certs/ca.crt
   ```

3. 重新编译固件。固件会把 `ca.crt` 嵌入，在 NINA `TLS_MODE` 连接时调用 `setCACert(ca.crt)`，用该 CA 验证服务端出示的 `server.crt` 证书链。

## 与 K210 网关的关系

- K210 通过 SPI 下发 `startClient`（`TLS_MODE` + Broker + 端口）。
- **CA 信任完全在本 ESP32 固件**；K210 Web **没有** CA 粘贴/上传，Flash **不**存 CA。
- 换 CA 须更新本目录 `ca.crt` 并 **重烧 ESP32**。

## 说明

- `ca.crt` 必须从**服务器拷贝**，不是在 ESP32 上新建 CA。
- 仅需服务端鉴别时，**不要**启用 NINA 的 `setClientCert` / `setCertKey`（那是客户端证书，用于双向 TLS）。

## `certs/config/` 部署包（团队分发）

仓库内 `certs/config/` 为 **Mosquitto 服务端完整部署包**（与已烧录 NINA 固件配套），便于现场安装，勿自行另建一套 CA。

| 文件 | 用途 | 装 Broker 是否必需 | 给 ESP32 / 编译 |
|------|------|-------------------|-----------------|
| `ca.crt` | 签发 CA（公钥） | ✅ `cafile` | 须与根目录 `certs/ca.crt` 一致并编入固件 |
| `server.crt` | Broker 证书 | ✅ `certfile` | 否（仅服务器出示） |
| `server.key` | Broker 私钥 | ✅ `keyfile` | 否（**不**打进固件） |
| `ca.key` | CA 私钥，用于**续签/新签** server | ❌ 仅跑现有服务不必 | 否 |
| `mosquitto.conf` | 监听 8883 等配置 | ✅ 建议 | 否 |
| `server.csr` / `ca.srl` | 生成过程文件 | 否 | 否 |

拷贝到服务器示例路径（与 `mosquitto.conf` 一致）：

```
/mosquitto/config/ca.crt
/mosquitto/config/server.crt
/mosquitto/config/server.key
```

## 固件固化的是什么？（常见误解）

- NINA **只嵌入** `certs/ca.crt`（CA **公钥证书**），**不包含**任何 `.key`。
- TLS 连接时：Broker 出示 `server.crt` → ESP32 用内置 `ca.crt` **验证是否由该 CA 签发**。
- 这不是「用 CA 去解私钥」，而是 **校验证书链**。

## 别人自己重新生成证书会怎样？

若同事 **另建 CA** 并自签新的 `server.crt`：

- 与固件内嵌的 `ca.crt` **不是同一信任根**
- ESP32 **拒绝连接**（证书校验失败）

要与 **已烧录的 NINA 固件** 配合，Broker 必须使用 **同一 CA 签发的** `server.crt`（即本仓库 `certs/config/` 这一套），或更换 `certs/ca.crt` 后 **重新编译、烧录 ESP32**。

## 私钥要不要分给部署同事？

| 私钥 | 是否建议分发 | 原因 |
|------|--------------|------|
| **`server.key`** | ✅ 要 | Mosquitto 开 8883 TLS **必须**有服务器私钥；与 ESP32 无关，仅 Broker 本机使用 |
| **`ca.key`** | 视情况 | **仅部署现有证书**：不必给。只有需要 **独立续期/重签** `server.crt` 时才给管证书的人 |

**最少部署包（与当前固件匹配）：** `ca.crt` + `server.crt` + `server.key` + `mosquitto.conf`。

**不能**指望接收方「自己生成服务端密钥」还能连上已固化 CA 的固件——除非他们用 **你的 `ca.key`** 在同一张 CA 下重签，或你更新固件中的 `ca.crt`。

## Git 与安全

- 提交记录：`3b1ac24`（证书与配置）、`6c1cfe5`（`ca.key`、`server.key`，供团队部署）。
- 若仓库为 **公开** GitHub，私钥进 Git 有泄露风险；建议 **私有仓库** 或私钥改走加密渠道单独传递。

## 现场验收（2026-05-30）

- Broker `192.168.3.106:8883`，Client `K210_C40A94A4`
- Mosquitto：`negotiated TLSv1.2 cipher ECDHE-RSA-AES256-GCM-SHA384`
- 网关串口：`[MQTT] TLS socket up 192.168.3.106:8883`
- 详见网关仓库 [docs/MQTT_TLS_MOSQUITTO.md](../../ModbusRTU_Mqtt_Gateway/docs/MQTT_TLS_MOSQUITTO.md) §4
