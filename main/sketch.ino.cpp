/*
  Main firmware for NINA module on Maixduino
  - Force WiFi mode (Bluetooth disabled)
  - No ADC initialization (avoid driver_ng conflict)
  - SPI pins via board.h
*/

#include <rom/uart.h>

extern "C" {
  #include "esp_private/periph_ctrl.h"
  #include "soc/gpio_periph.h"
  #include "soc/periph_defs.h"
  #include <driver/uart.h>
  #include <esp_bt.h>
  #include "esp_spiffs.h"
  #include "nvs_flash.h"
  #include <stdio.h>
  #include <sys/types.h>
  #include <dirent.h>
}

#include <Arduino.h>
#include <SPIS.h>
#include <WiFi.h>
#include "CommandHandler.h"

// 注意：不再包含 <driver/adc.h>，完全移除 ADC 功能

#define SPI_BUFFER_LEN SPI_MAX_DMA_LEN

// 调试输出默认开启
int debug = 1;

// 定义硬件版本宏（用于蓝牙部分，但蓝牙已禁用）
#define UNO_WIFI_REV2   1

#include "board.h"

#define AIRLIFT 1
#define NINA_PRINTF(...) do { if (debug) { ets_printf(__VA_ARGS__); } } while (0)

#if defined(CONFIG_IDF_TARGET_ESP32)
  extern const struct __sFILE_fake __sf_fake_stdin;
  extern const struct __sFILE_fake __sf_fake_stdout;
  extern const struct __sFILE_fake __sf_fake_stderr;

  // SPIS 实例化（引脚来自 board.h，已修改为 14,23,18,5,25）
  SPISClass SPIS(VSPI_HOST, 1, AIRLIFT_MOSI, AIRLIFT_MISO, AIRLIFT_SCK, AIRLIFT_CS, AIRLIFT_BUSY);
#endif

// 阻止 Arduino 框架释放蓝牙内存（强制 WiFi）
extern "C" bool btInUse() {
  return true;
}

uint8_t* commandBuffer;
uint8_t* responseBuffer;

void dumpBuffer(const char* label, uint8_t data[], int length) {
  ets_printf("%s: ", label);
  for (int i = 0; i < length; i++) {
    ets_printf("%02x", data[i]);
  }
  ets_printf("\r\n");
}

void setDebug(int d) {
  debug = d;
  if (debug) {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[1], 0);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[3], 0);
    const char* default_uart_dev = "/dev/uart/0";
    _GLOBAL_REENT->_stdin  = fopen(default_uart_dev, "r");
    _GLOBAL_REENT->_stdout = fopen(default_uart_dev, "w");
    _GLOBAL_REENT->_stderr = fopen(default_uart_dev, "w");
    uart_div_modify(CONFIG_CONSOLE_UART_NUM, (APB_CLK_FREQ << 4) / 115200);
    ets_install_uart_printf();
    uart_tx_switch(CONFIG_CONSOLE_UART_NUM);
  } else {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[1], PIN_FUNC_GPIO);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[3], PIN_FUNC_GPIO);
#if CONFIG_IDF_TARGET_ESP32
    _GLOBAL_REENT->_stdin  = (FILE*) &__sf_fake_stdin;
    _GLOBAL_REENT->_stdout = (FILE*) &__sf_fake_stdout;
    _GLOBAL_REENT->_stderr = (FILE*) &__sf_fake_stderr;
#endif
    ets_install_putc1(NULL);
    ets_install_putc2(NULL);
  }
}

void setupWiFi();
void setupBluetooth();   // 保留声明但不再调用

// ============================================
// setup() 修改：
// 1. 强制调用 setupWiFi()，忽略 CS 引脚电平（禁用蓝牙）
// 2. 移除所有 ADC 初始化代码（避免 driver_ng 冲突）
// ============================================
void setup() {
#ifndef CMAKE_BUILD_TYPE_DEBUG
  setDebug(0);
#endif

#if !AIRLIFT
  pinMode(15, INPUT);
  pinMode(21, INPUT);
#endif

  // 直接进入 WiFi 模式，不再判断 CS 引脚
  setupWiFi();
}

// 蓝牙初始化函数（已不被调用，保留以防恢复）
void setupBluetooth() {
  NINA_PRINTF("*** BLUETOOTH\n");
  periph_module_enable(PERIPH_UART1_MODULE);
  periph_module_enable(PERIPH_UHCI0_MODULE);
  esp_bt_controller_config_t btControllerConfig = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

#if defined(CONFIG_IDF_TARGET_ESP32)
#if defined(AIRLIFT)
  uart_set_pin(UART_NUM_1, 1, 3, AIRLIFT_RTS, AIRLIFT_CTS);
#elif defined(UNO_WIFI_REV2)
  uart_set_pin(UART_NUM_1, 1, 3, 33, 0);
#elif defined(NANO_RP2040_CONNECT)
  uart_set_pin(UART_NUM_1, 1, 3, 33, 12);
#else
  uart_set_pin(UART_NUM_1, 23, 12, 18, 5);
#endif
  uart_set_hw_flow_ctrl(UART_NUM_1, UART_HW_FLOWCTRL_CTS_RTS, 5);
  btControllerConfig.hci_uart_no = UART_NUM_1;
#if defined(AIRLIFT)
  btControllerConfig.hci_uart_baudrate = 115200;
#elif defined(UNO_WIFI_REV2) || defined(NANO_RP2040_CONNECT)
  btControllerConfig.hci_uart_baudrate = 115200;
#else
  btControllerConfig.hci_uart_baudrate = 912600;
#endif
#endif

  esp_err_t ret = esp_bt_controller_init(&btControllerConfig);
  if (ESP_OK != ret) {
    setDebug(1);
    NINA_PRINTF("esp_bt_controller_init failed: 0x%x\n", ret);
    while (1) {}
  }
  while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE);
  esp_bt_controller_enable(ESP_BT_MODE_BLE);
#if defined(CONFIG_IDF_TARGET_ESP32)
  esp_bt_sleep_enable();
#endif
  vTaskSuspend(NULL);
  while (1) {
    vTaskDelay(portMAX_DELAY);
  }
}

void setupWiFi() {
  NINA_PRINTF("WIFI ON\n");

  // 初始化 NVS（WiFi 需要）
  esp_err_t nvs_ret = nvs_flash_init();
  if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
  SPIS.begin();

  esp_vfs_spiffs_conf_t conf = {
    .base_path = "/fs",
    .partition_label = "storage",
    .max_files = 20,
    .format_if_mount_failed = true
  };
  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  (void) ret;

  if (WiFi.status() == WL_NO_SHIELD) {
    if (!debug) {
      setDebug(1);
    }
    NINA_PRINTF("*** NOSHIELD\n");
    while (1); // no shield
  }

  commandBuffer = (uint8_t*)heap_caps_malloc(SPI_BUFFER_LEN, MALLOC_CAP_DMA);
  responseBuffer = (uint8_t*)heap_caps_malloc(SPI_BUFFER_LEN, MALLOC_CAP_DMA);

  NINA_PRINTF("*** CommandHandler Begin\n");
  CommandHandler.begin();

  // 初始化 NTP 和 WiFi 事件处理（只需一次）
  // _setupAfterWifiBegin() 在 CommandHandler.cpp 中定义，但此处无法直接调用
  // 改为在 setPassPhrase/setNet 首次调用时自动完成
}

void loop() {
  // 等待 SPI 命令（使用超时，让 WiFi 后台任务有机会运行）
  memset(commandBuffer, 0x00, SPI_BUFFER_LEN);
  int commandLength = SPIS.transfer(NULL, commandBuffer, SPI_BUFFER_LEN);

  if (commandLength == 0) {
    // 没有 SPI 命令时，让出 CPU 给 WiFi 任务
    delay(1);
    return;
  }

  if (debug) {
    dumpBuffer("COMMAND", commandBuffer, commandLength);
  }

  // 处理命令
  memset(responseBuffer, 0x00, SPI_BUFFER_LEN);
  int responseLength = CommandHandler.handle(commandBuffer, responseBuffer);

  // 返回响应
  SPIS.transfer(responseBuffer, NULL, responseLength);

  if (debug) {
    dumpBuffer("RESPONSE", responseBuffer, responseLength);
  }
}