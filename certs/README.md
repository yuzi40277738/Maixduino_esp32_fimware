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

## 现场验收（2026-05-30）

- Broker `192.168.3.106:8883`，Client `K210_C40A94A4`
- Mosquitto：`negotiated TLSv1.2 cipher ECDHE-RSA-AES256-GCM-SHA384`
- 网关串口：`[MQTT] TLS socket up 192.168.3.106:8883`
- 详见网关仓库 [docs/MQTT_TLS_MOSQUITTO.md](../../ModbusRTU_Mqtt_Gateway/docs/MQTT_TLS_MOSQUITTO.md) §4
