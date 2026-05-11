# 最新适配内容：
基于最新nina-fw (Maixduino/K210 适配版)
基于官方 nina-fw 修改，专为 Maixduino (K210 + ESP32) 硬件平台定制，适配 ESP-IDF 5.x，解决原生固件无法在 K210 上正常运行 WiFi/MQTT 等网络功能的问题。
项目概述
本项目是对 Arduino NINA 固件的深度定制修改，将原本面向 Adafruit Airlift 开发板的固件，完整移植适配到 Maixduino (K210 + ESP32) 硬件，修复 SPI 引脚、WiFi 模式、网络协议、编译依赖等核心问题，确保 ESP32 协处理器稳定为 K210 提供网络服务。
核心修改内容
1. 硬件引脚适配（关键）
修改 boards/esp32/board.h，重定义 SPI 硬件引脚，完全匹配 K210 硬件电路：
```
plaintext

信号	原生固件引脚	修改后引脚	用途说明
MOSI	12	14	K210 SPI 从机数据输入
BUSY	33	25	K210 状态检测专用引脚
MISO/SCK/CS	23/18/5	保持不变	基础 SPI 通信引脚
```
2. 固件运行模式修改
重写 main/sketch.ino.cpp 核心逻辑：
强制开启 WiFi 模式，彻底禁用蓝牙功能
删除冲突的 ADC 初始化代码，避免驱动报错
新增 NVS 闪存初始化，保证 WiFi 配置持久化生效
移除无用的 ESP32C6 兼容代码，简化逻辑
优化主循环，提升 WiFi 后台任务运行稳定性
3. 网络功能优化
增强 main/CommandHandler.cpp，解决网络连接问题：
优化 WiFi 连接逻辑，强制设置 STA 模式
新增 DNS 备用方案：DHCP 分配失败时自动使用 8.8.8.8 公共 DNS
适配 ESP-IDF 5.x API，注释冲突的 NTP 初始化逻辑
提升 WiFi 连接、域名解析的稳定性
4. 编译与工具链适配
main/CMakeLists.txt：新增 NVS 组件依赖，修复编译缺失问题
combine.py：修复 UTF-8 编码报错，提升脚本兼容性
新增 VS Code 配置文件，适配本地 ESP-IDF 5.5 工具链
生成完整依赖锁定文件，保证编译环境一致性
适配目标平台
主控：K210 (Maixduino)
协处理器：ESP32（负责 WiFi / 网络通信）
编译环境：ESP-IDF v5.5
主要功能
✅ 稳定 WiFi 连接（支持 DHCP / 手动 DNS）
✅ MQTT 客户端通信（适配国内服务器，解决连接失败问题）
✅ DNS 域名解析（自带备用 DNS）
✅ SPI 与 K210 高速通信
✅ 兼容标准 NINA 协议指令
使用说明
本固件仅用于 Maixduino/K210 开发板，不可用于其他 ESP32 硬件
基于 ESP-IDF 5.x 编译，不兼容低版本 IDF
直接编译烧录至 ESP32 协处理器即可使用
网络连接失败时，优先使用国内 MQTT 服务器 + 开放端口
文件结构说明
```
plaintext
nina-fw/
├── boards/esp32/board.h       # SPI 硬件引脚配置
├── main/
│   ├── sketch.ino.cpp         # 固件入口，核心模式修改
│   ├── CommandHandler.cpp/.h  # WiFi/网络指令处理
│   ├── CMakeLists.txt         # 编译依赖配置
│   ├── http_client.c          # HTTP 客户端功能
│   └── 其他源码文件
├── Makefile                   # 编译脚本
├── combine.py                 # 固件打包脚本（编码修复）
└── .vscode/                   # VS Code + ESP-IDF 配置
```
总结
本修改版 nina-fw 彻底解决了 K210 + ESP32 平台上：
SPI 通信异常
WiFi 无法启动
DNS 解析失败
MQTT TCP 连接失败 / 协议错误
编译报错、驱动冲突
是 Maixduino 开发板使用 ESP32 网络功能的专用固件。

# Adafruit fork of the Arduino NINA-W102 firmware

[![Build Status](https://travis-ci.com/adafruit/nina-fw.svg?branch=master)](https://travis-ci.com/adafruit/nina-fw)

This is the Adafruit fork of the Arduino NINA-W102 firmware. The original
repository is located at https://github.com/arduino/nina-fw

This firmware uses [Espressif's IDF](https://github.com/espressif/esp-idf)

## Contributing to nina-fw

Please be aware that by contributing to this project
you are agreeing to the [Code of Conduct](https://github.com/adafruit/nina-fw/blob/master/code-of-conduct.md).
Contributors who follow the [Code of Conduct](https://github.com/adafruit/nina-fw/blob/master/code-of-conduct.md)
are welcome to submit pull requests and they will be promptly
reviewed by project admins. Please join the [Discord](https://adafru.it/discord) too.

The NINA firmware version needs to be updated in two places in this repo:
1. CommandHandler.cpp
1. CHANGELOG

## Building

The firmware shipped in Adafruit's products is compiled following these
instructions. These may differ from the instructions included in the
original Arduino firmware repository.

1. [Download the ESP32 toolchain](https://docs.espressif.com/projects/esp-idf/en/v3.3.1/get-started/index.html#setup-toolchain)
1. Extract it and add it to your `PATH`: `export PATH=$PATH:<path/to/toolchain>/bin`
1. Clone **v5.5** of the IDF: `git clone --branch v5.5 --recursive https://github.com/espressif/esp-idf.git`
1. Set the `IDF_PATH` environment variable: `export IDF_PATH=<path/to/idf>`
1. `git submodule update --init` to fetch the `certificates` submodule.
1. Run `idf.py -DBOARD=your-board build` to build the firmware (in the directory of this readme).
Where `your-board` can be found in boards/ folder, e.g. `esp32` or `fruitjam_c6`
1. You may need to set up a python3 `venv` to avoid Python library version issues.
1. You should have a file named `NINA_W102-x.x.x.bin` in the top directory
1. Use appropriate tools (`esptool.py`, appropriate pass-through firmware etc)
   to load this binary file onto your board.
    a. If you do not know how to do this, [we have an excellent guide on the Adafruit Learning System for upgrading your ESP32's firmware](https://learn.adafruit.com/upgrading-esp32-firmware)

## Packaging
The `make` command produces a bunch of binary files that must be flashed at very precise locations, making `esptool` commandline quite complicated.
Instead, once the firmware has been compiled, you can invoke `combine.py` script to produce a monolithic binary that can be flashed at 0x0.
```
make
python combine.py
```
This produces `NINA_W102.bin-{version}` file (a different name can be specified as parameter). To flash this file you can use https://learn.adafruit.com/upgrading-esp32-firmware

## Build a new certificate list (based on the Google Android root CA list)
```bash
git clone https://android.googlesource.com/platform/system/ca-certificates
cp nina-fw/tools/nina-fw-create-roots.sh ca-certificates/files
cd ca-certificates/files
./nina-fw-create-roots.sh
cp roots.pem ../../nina-fw/data/roots.pem
```

## Check certificate list against URL list
```bash
cd tools
./sslcheck.sh -c ../data/roots.pem -l url_lists/url_list_moz.com.txt -e
```

## License

Copyright (c) 2018-2019 Arduino SA. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
