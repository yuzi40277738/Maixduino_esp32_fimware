/*
  board.h - Pin definitions for Maixduino (K210 + ESP32)
  Modified to match hardware: MOSI=14, MISO=23, SCK=18, CS=5, BUSY=25
*/

#ifndef BOARD_H
#define BOARD_H

// ============================================
// 修改说明：以下引脚配置完全匹配 K210 / Maixduino 硬件
// 原始值：MOSI=12, MISO=23, SCK=18, CS=5, BUSY=33
// 修改后：MOSI=14, MISO=23, SCK=18, CS=5, BUSY=25
// ============================================

// SPIS for WiFi (SPI 从机接口)
#define AIRLIFT_MOSI  14   // 原为 12，改为 14
#define AIRLIFT_MISO  23   // 保持不变
#define AIRLIFT_SCK   18   // 保持不变
#define AIRLIFT_CS    5    // 保持不变
#define AIRLIFT_BUSY  25   // 原为 33，改为 25（对应 K210 检测引脚）

// UART for BLE HCI（蓝牙 HCI 串口，本固件已强制关闭蓝牙，以下定义不影响使用）
#define AIRLIFT_RTS   AIRLIFT_BUSY   // 与 BUSY 复用（蓝牙不用）
#define AIRLIFT_CTS   0              // BOOT 引脚，保持不变

#endif