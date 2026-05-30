#ifndef STREAM_HANDLER_H
#define STREAM_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define STREAM_BUFFER_SIZE      (32 * 1024)
#define STREAM_FLAG_MORE_DATA   (1 << 0)
#define STREAM_FLAG_END         (1 << 1)
#define STREAM_FLAG_ERROR       (1 << 2)
#define STREAM_FLAG_OVERFLOW    (1 << 3)

#define CMD_STREAM_START        0xF0
#define CMD_STREAM_PULL         0xF1
#define CMD_STREAM_STATUS       0xF2
#define CMD_STREAM_ABORT        0xF3

#define STREAM_CMD_GET_TCP_DATA (0x2C + 0x80)
#define STREAM_CMD_SCAN_NETWORKS (0x27 + 0x80)
#define STREAM_CMD_READ_FILE    (0x61 + 0x80)

typedef struct {
    uint8_t* buffer;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
} ring_buffer_t;

typedef struct {
    uint8_t  cmd;
    uint32_t total_size;
    uint32_t transferred;
    bool     is_active;
    bool     is_complete;
    uint8_t  error_code;
    uint32_t last_data_time;
    
    ring_buffer_t ring_buf;
    SemaphoreHandle_t mutex;
    TaskHandle_t task_handle;
    
    uint8_t  params[64];
    uint16_t param_len;
} stream_context_t;

#ifdef __cplusplus
extern "C" {
#endif

bool stream_init(void);
void stream_deinit(void);

bool stream_start(uint8_t original_cmd, const uint8_t* params, uint16_t param_len);
uint16_t stream_pull(uint8_t* out_buffer, uint16_t max_len, uint8_t* flags);
void stream_get_status(uint8_t* status, uint32_t* transferred, uint32_t* total);
void stream_abort(void);
bool stream_is_active(void);

bool stream_write_data(const uint8_t* data, uint16_t len);
void stream_set_complete(uint32_t total_size);
void stream_set_error(uint8_t error_code);

#ifdef __cplusplus
}
#endif

#endif
