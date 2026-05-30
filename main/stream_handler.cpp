#include "stream_handler.h"
#include <string.h>
#include "esp_log.h"
#include <stdio.h>

#include <Network.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

static const char* TAG = "STREAM";

static stream_context_t g_stream_ctx;
static bool g_initialized = false;

static uint32_t ring_available(ring_buffer_t* rb) {
    if (rb->head >= rb->tail) {
        return rb->size - rb->head + rb->tail;
    } else {
        return rb->tail - rb->head;
    }
}

static uint32_t ring_used(ring_buffer_t* rb) {
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    } else {
        return rb->size - rb->tail + rb->head;
    }
}

static bool ring_write(ring_buffer_t* rb, const uint8_t* data, uint32_t len) {
    if (len > ring_available(rb)) {
        return false;
    }
    
    for (uint32_t i = 0; i < len; i++) {
        rb->buffer[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->size;
    }
    return true;
}

static uint32_t ring_read(ring_buffer_t* rb, uint8_t* out, uint32_t max_len) {
    uint32_t used = ring_used(rb);
    uint32_t to_read = (used < max_len) ? used : max_len;
    
    for (uint32_t i = 0; i < to_read; i++) {
        out[i] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % rb->size;
    }
    return to_read;
}

bool stream_init(void) {
    if (g_initialized) {
        return true;
    }
    
    memset(&g_stream_ctx, 0, sizeof(g_stream_ctx));
    
    g_stream_ctx.ring_buf.buffer = (uint8_t*)malloc(STREAM_BUFFER_SIZE);
    if (!g_stream_ctx.ring_buf.buffer) {
        ESP_LOGE(TAG, "Failed to allocate stream buffer");
        return false;
    }
    
    g_stream_ctx.ring_buf.size = STREAM_BUFFER_SIZE;
    g_stream_ctx.ring_buf.head = 0;
    g_stream_ctx.ring_buf.tail = 0;
    
    g_stream_ctx.mutex = xSemaphoreCreateMutex();
    if (!g_stream_ctx.mutex) {
        free(g_stream_ctx.ring_buf.buffer);
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }
    
    g_stream_ctx.task_handle = NULL;
    g_initialized = true;
    ESP_LOGI(TAG, "Stream handler initialized (buffer: %d bytes)", STREAM_BUFFER_SIZE);
    return true;
}

void stream_deinit(void) {
    if (!g_initialized) {
        return;
    }
    
    stream_abort();
    
    if (g_stream_ctx.ring_buf.buffer) {
        free(g_stream_ctx.ring_buf.buffer);
        g_stream_ctx.ring_buf.buffer = NULL;
    }
    
    if (g_stream_ctx.mutex) {
        vSemaphoreDelete(g_stream_ctx.mutex);
        g_stream_ctx.mutex = NULL;
    }
    
    g_initialized = false;
}

static void tcp_data_stream_task(void* pvParameters) {
    ESP_LOGI(TAG, "TCP data stream task started");
    
    stream_context_t* ctx = &g_stream_ctx;
    
    // BugFix: 检查参数长度和 socket 范围
    if (ctx->param_len < 2) {
        stream_set_error(2);  // 参数不足
        ESP_LOGE(TAG, "TCP stream: param_len=%u < 2", ctx->param_len);
        vTaskDelete(NULL);
        return;
    }
    
    uint8_t socket = ctx->params[0];
    uint8_t peek = ctx->params[1];
    
    if (socket >= 10) {  // MAX_SOCKETS = 10
        stream_set_error(3);  // socket 越界
        ESP_LOGE(TAG, "TCP stream: socket=%u out of range", socket);
        vTaskDelete(NULL);
        return;
    }
    
    extern uint8_t socketTypes[];
    extern NetworkClient tcpClients[];
    extern NetworkUDP udps[];
    extern NetworkClientSecure tlsClients[];
    extern SemaphoreHandle_t socketMutex[];
    
    uint8_t temp_buf[1024];
    uint32_t total_read = 0;
    
    while (ctx->is_active) {
        if (xSemaphoreTake(socketMutex[socket], pdMS_TO_TICKS(100)) != pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        int avail = 0;
        if (socketTypes[socket] == 0) {
            avail = tcpClients[socket].available();
        } else if (socketTypes[socket] == 1) {
            avail = udps[socket].available();
        } else if (socketTypes[socket] == 2) {
            avail = tlsClients[socket].available();
        }
        
        if (avail <= 0) {
            xSemaphoreGive(socketMutex[socket]);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        
        int to_read = (avail < (int)sizeof(temp_buf)) ? avail : sizeof(temp_buf);
        
        if (peek) {
            for (int i = 0; i < to_read; i++) {
                if (socketTypes[socket] == 0) {
                    temp_buf[i] = tcpClients[socket].peek();
                } else if (socketTypes[socket] == 1) {
                    temp_buf[i] = udps[socket].peek();
                } else if (socketTypes[socket] == 2) {
                    temp_buf[i] = tlsClients[socket].peek();
                }
            }
        } else {
            if (socketTypes[socket] == 0) {
                to_read = tcpClients[socket].read(temp_buf, to_read);
            } else if (socketTypes[socket] == 1) {
                to_read = udps[socket].read(temp_buf, to_read);
            } else if (socketTypes[socket] == 2) {
                to_read = tlsClients[socket].read(temp_buf, to_read);
            }
        }
        
        xSemaphoreGive(socketMutex[socket]);
        
        if (to_read > 0) {
            // BugFix: 环形缓冲区满时等待有空间再写入
            bool write_success = false;
            int retry_count = 0;
            const int MAX_RETRIES = 50;  // 最多等待 1 秒
            
            while (!write_success && ctx->is_active && retry_count < MAX_RETRIES) {
                xSemaphoreTake(ctx->mutex, portMAX_DELAY);
                write_success = ring_write(&ctx->ring_buf, temp_buf, to_read);
                if (write_success) {
                    total_read += to_read;
                    ctx->last_data_time = millis();
                }
                xSemaphoreGive(ctx->mutex);
                
                if (!write_success) {
                    retry_count++;
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
            
            if (!write_success && ctx->is_active) {
                ESP_LOGW(TAG, "TCP stream: buffer full, data dropped (%u bytes)", to_read);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    stream_set_complete(total_read);
    ESP_LOGI(TAG, "TCP data stream task ended (total: %u)", total_read);
    vTaskDelete(NULL);
}

static void scan_networks_stream_task(void* pvParameters) {
    ESP_LOGI(TAG, "Scan networks stream task started");
    
    stream_context_t* ctx = &g_stream_ctx;
    
    int num = WiFi.scanNetworks();
    uint32_t total_written = 0;
    
    for (int i = 0; i < num && ctx->is_active; i++) {
        String ssid = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        uint8_t encryption = WiFi.encryptionType(i);
        // BugFix: 拷贝 BSSID 而不是保存指针，防止指针失效
        uint8_t bssid[6];
        uint8_t* bssid_ptr = WiFi.BSSID(i);
        if (bssid_ptr != NULL) {
            memcpy(bssid, bssid_ptr, 6);
        } else {
            memset(bssid, 0, 6);
        }
        int32_t channel = WiFi.channel(i);
        
        int ssidLen = ssid.length();
        // BugFix: 限制 SSID 长度防止缓冲区溢出
        if (ssidLen > 32) {
            ssidLen = 32;
        }
        uint8_t ap_info[256];
        int offset = 0;
        
        ap_info[offset++] = ssidLen;
        memcpy(&ap_info[offset], ssid.c_str(), ssidLen);
        offset += ssidLen;
        
        memcpy(&ap_info[offset], &rssi, 4);
        offset += 4;
        
        ap_info[offset++] = encryption;
        
        memcpy(&ap_info[offset], bssid, 6);
        offset += 6;
        
        memcpy(&ap_info[offset], &channel, 4);
        offset += 4;
        
        xSemaphoreTake(ctx->mutex, portMAX_DELAY);
        ring_write(&ctx->ring_buf, ap_info, offset);
        ctx->last_data_time = millis();
        xSemaphoreGive(ctx->mutex);
        
        total_written += offset;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    stream_set_complete(total_written);
    ESP_LOGI(TAG, "Scan networks stream task ended (total: %u, APs: %d)", total_written, num);
    vTaskDelete(NULL);
}

static void read_file_stream_task(void* pvParameters) {
    ESP_LOGI(TAG, "Read file stream task started");
    
    stream_context_t* ctx = &g_stream_ctx;
    
    char filename[32 + 1];
    memset(filename, 0, sizeof(filename));
    
    // BugFix: 安全解析参数，防止越界访问
    int param_offset = 0;
    #define CHECK_PARAM_BOUND(off) do { \
        if ((off) >= ctx->param_len) { \
            stream_set_error(2); \
            ESP_LOGE(TAG, "File stream: param out of bounds"); \
            vTaskDelete(NULL); \
            return; \
        } \
    } while(0)
    
    CHECK_PARAM_BOUND(param_offset);
    size_t offset_len = ctx->params[param_offset++];
    param_offset += offset_len;
    
    CHECK_PARAM_BOUND(param_offset);
    size_t len_len = ctx->params[param_offset++];
    param_offset += len_len;
    
    CHECK_PARAM_BOUND(param_offset);
    size_t filename_len = ctx->params[param_offset++];
    
    // BugFix: 限制文件名长度
    if (filename_len > 32) {
        filename_len = 32;
    }
    
    if (param_offset + filename_len > ctx->param_len) {
        stream_set_error(2);
        ESP_LOGE(TAG, "File stream: filename out of bounds");
        vTaskDelete(NULL);
        return;
    }
    
    memcpy(filename, &ctx->params[param_offset], filename_len);
    #undef CHECK_PARAM_BOUND
    
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        stream_set_error(1);
        ESP_LOGE(TAG, "Failed to open file: %s", filename);
        vTaskDelete(NULL);
        return;
    }
    
    uint8_t temp_buf[1024];
    uint32_t total_read = 0;
    
    while (ctx->is_active) {
        size_t bytes_read = fread(temp_buf, 1, sizeof(temp_buf), f);
        
        if (bytes_read == 0) {
            break;
        }
        
        // BugFix: 环形缓冲区满时等待有空间再写入
        bool write_success = false;
        int retry_count = 0;
        const int MAX_RETRIES = 50;
        
        while (!write_success && ctx->is_active && retry_count < MAX_RETRIES) {
            xSemaphoreTake(ctx->mutex, portMAX_DELAY);
            write_success = ring_write(&ctx->ring_buf, temp_buf, bytes_read);
            xSemaphoreGive(ctx->mutex);
            
            if (!write_success) {
                retry_count++;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
        
        if (!write_success && ctx->is_active) {
            ESP_LOGW(TAG, "File stream: buffer full, data dropped");
            break;
        }
        
        total_read += bytes_read;
        ctx->last_data_time = millis();
    }
    
    fclose(f);
    stream_set_complete(total_read);
    ESP_LOGI(TAG, "Read file stream task ended (total: %u)", total_read);
    vTaskDelete(NULL);
}

bool stream_start(uint8_t original_cmd, const uint8_t* params, uint16_t param_len) {
    if (!g_initialized) {
        if (!stream_init()) {
            return false;
        }
    }
    
    xSemaphoreTake(g_stream_ctx.mutex, portMAX_DELAY);
    
    if (g_stream_ctx.is_active) {
        xSemaphoreGive(g_stream_ctx.mutex);
        ESP_LOGW(TAG, "Stream already active");
        return false;
    }
    
    g_stream_ctx.cmd = original_cmd;
    g_stream_ctx.total_size = 0;
    g_stream_ctx.transferred = 0;
    g_stream_ctx.is_active = true;
    g_stream_ctx.is_complete = false;
    g_stream_ctx.error_code = 0;
    g_stream_ctx.last_data_time = millis();
    g_stream_ctx.ring_buf.head = 0;
    g_stream_ctx.ring_buf.tail = 0;
    
    if (param_len > 0 && params != NULL) {
        uint16_t copy_len = (param_len < sizeof(g_stream_ctx.params)) ? param_len : sizeof(g_stream_ctx.params);
        memcpy(g_stream_ctx.params, params, copy_len);
        g_stream_ctx.param_len = copy_len;
    } else {
        g_stream_ctx.param_len = 0;
    }
    
    xSemaphoreGive(g_stream_ctx.mutex);
    
    BaseType_t task_result = pdFAIL;
    
    if (original_cmd == 0x2C) {
        task_result = xTaskCreate(tcp_data_stream_task, "tcp_stream", 4096, NULL, 2, &g_stream_ctx.task_handle);
        ESP_LOGI(TAG, "Stream started: TCP data (cmd: 0x%02X)", original_cmd);
    } else if (original_cmd == 0x27) {
        task_result = xTaskCreate(scan_networks_stream_task, "scan_stream", 4096, NULL, 2, &g_stream_ctx.task_handle);
        ESP_LOGI(TAG, "Stream started: WiFi scan (cmd: 0x%02X)", original_cmd);
    } else if (original_cmd == 0x61) {
        task_result = xTaskCreate(read_file_stream_task, "file_stream", 4096, NULL, 2, &g_stream_ctx.task_handle);
        ESP_LOGI(TAG, "Stream started: Read file (cmd: 0x%02X)", original_cmd);
    } else {
        ESP_LOGW(TAG, "Stream started: Generic (cmd: 0x%02X)", original_cmd);
        return true;
    }
    
    if (task_result != pdPASS) {
        g_stream_ctx.is_active = false;
        ESP_LOGE(TAG, "Failed to create stream task");
        return false;
    }
    
    return true;
}

uint16_t stream_pull(uint8_t* out_buffer, uint16_t max_len, uint8_t* flags) {
    if (!g_initialized || !g_stream_ctx.is_active) {
        *flags = STREAM_FLAG_END;
        return 0;
    }
    
    xSemaphoreTake(g_stream_ctx.mutex, portMAX_DELAY);
    
    *flags = 0;
    uint16_t read_len = ring_read(&g_stream_ctx.ring_buf, out_buffer, max_len);
    g_stream_ctx.transferred += read_len;
    
    if (g_stream_ctx.is_complete && ring_used(&g_stream_ctx.ring_buf) == 0) {
        *flags |= STREAM_FLAG_END;
        g_stream_ctx.is_active = false;
        ESP_LOGI(TAG, "Stream completed (transferred: %u bytes)", g_stream_ctx.transferred);
    } else if (!g_stream_ctx.is_complete) {
        *flags |= STREAM_FLAG_MORE_DATA;
    }
    
    if (g_stream_ctx.error_code != 0) {
        *flags |= STREAM_FLAG_ERROR;
        g_stream_ctx.is_active = false;
        ESP_LOGE(TAG, "Stream error (code: %u)", g_stream_ctx.error_code);
    }
    
    xSemaphoreGive(g_stream_ctx.mutex);
    return read_len;
}

void stream_get_status(uint8_t* status, uint32_t* transferred, uint32_t* total) {
    if (!g_initialized) {
        *status = 0;
        *transferred = 0;
        *total = 0;
        return;
    }
    
    xSemaphoreTake(g_stream_ctx.mutex, portMAX_DELAY);
    
    *status = g_stream_ctx.is_active ? 1 : 0;
    *transferred = g_stream_ctx.transferred;
    *total = g_stream_ctx.total_size;
    
    xSemaphoreGive(g_stream_ctx.mutex);
}

void stream_abort(void) {
    if (!g_initialized) {
        return;
    }
    
    xSemaphoreTake(g_stream_ctx.mutex, portMAX_DELAY);
    
    g_stream_ctx.is_active = false;
    g_stream_ctx.is_complete = false;
    g_stream_ctx.ring_buf.head = 0;
    g_stream_ctx.ring_buf.tail = 0;
    
    // BugFix: 安全清理任务句柄，防止资源泄漏
    TaskHandle_t old_task = g_stream_ctx.task_handle;
    g_stream_ctx.task_handle = NULL;
    
    xSemaphoreGive(g_stream_ctx.mutex);
    
    if (old_task != NULL) {
        // 先给任务一点时间自行退出
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 检查任务是否还存在，如果还存在则强制删除
        // 注意：eTaskGetState 在 FreeRTOS 中需要配置 INCLUDE_eTaskGetState = 1
        // 这里我们采用安全的方式：直接删除（即使任务已退出，vTaskDelete 也是安全的）
        vTaskDelete(old_task);
    }
    
    ESP_LOGW(TAG, "Stream aborted");
}

bool stream_is_active(void) {
    if (!g_initialized) {
        return false;
    }
    return g_stream_ctx.is_active;
}

bool stream_write_data(const uint8_t* data, uint16_t len) {
    if (!g_initialized || !g_stream_ctx.is_active) {
        return false;
    }
    
    xSemaphoreTake(g_stream_ctx.mutex, portMAX_DELAY);
    
    bool success = ring_write(&g_stream_ctx.ring_buf, data, len);
    if (success) {
        g_stream_ctx.last_data_time = millis();
    }
    
    xSemaphoreGive(g_stream_ctx.mutex);
    
    return success;
}

void stream_set_complete(uint32_t total_size) {
    if (!g_initialized) {
        return;
    }
    
    xSemaphoreTake(g_stream_ctx.mutex, portMAX_DELAY);
    
    g_stream_ctx.is_complete = true;
    if (total_size > 0) {
        g_stream_ctx.total_size = total_size;
    }
    
    xSemaphoreGive(g_stream_ctx.mutex);
    
    ESP_LOGI(TAG, "Stream marked complete (total: %u)", total_size);
}

void stream_set_error(uint8_t error_code) {
    if (!g_initialized) {
        return;
    }
    
    xSemaphoreTake(g_stream_ctx.mutex, portMAX_DELAY);
    
    g_stream_ctx.error_code = error_code;
    
    xSemaphoreGive(g_stream_ctx.mutex);
    
    ESP_LOGE(TAG, "Stream error set: %u", error_code);
}
