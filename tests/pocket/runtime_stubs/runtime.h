#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM -2
#define ESP_ERR_INVALID_STATE -3
#define ESP_ERR_TIMEOUT -4
#define BIT0 1
#define BIT1 2
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
#define pdPASS 1
#define MALLOC_CAP_8BIT 4
typedef void *TaskHandle_t;
typedef void *SemaphoreHandle_t;
typedef void *EventGroupHandle_t;
typedef void (*esp_alloc_failed_hook_t)(size_t,uint32_t,const char *);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t,uint32_t);
int xSemaphoreGive(SemaphoreHandle_t);
EventGroupHandle_t xEventGroupCreate(void);
unsigned xEventGroupSetBits(EventGroupHandle_t,unsigned);
unsigned xEventGroupClearBits(EventGroupHandle_t,unsigned);
unsigned xEventGroupWaitBits(EventGroupHandle_t,unsigned,bool,bool,uint32_t);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
int xTaskCreate(void (*fn)(void *),const char *,unsigned,void *,unsigned,TaskHandle_t *);
unsigned ulTaskNotifyTake(bool,uint32_t);
void xTaskNotifyGive(TaskHandle_t);
void vTaskDelete(TaskHandle_t);
void vTaskDelay(uint32_t);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t);
unsigned esp_get_free_heap_size(void);
size_t heap_caps_get_largest_free_block(uint32_t);
esp_err_t heap_caps_register_failed_alloc_callback(esp_alloc_failed_hook_t);
void runtime_log(const char *,const char *,...);
#define ESP_LOGI(...) runtime_log(__VA_ARGS__)
#define ESP_LOGW(...) runtime_log(__VA_ARGS__)
#define ESP_LOGE(...) runtime_log(__VA_ARGS__)
typedef void *esp_http_client_handle_t;
typedef struct {
    const char *url;
    int timeout_ms,buffer_size,buffer_size_tx,max_redirection_count;
    bool disable_auto_redirect;
    void (*crt_bundle_attach)(void);
    const char *user_agent;
} esp_http_client_config_t;
void esp_crt_bundle_attach(void);
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *);
int esp_http_client_set_header(esp_http_client_handle_t,const char *,const char *);
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t,int);
int esp_http_client_open(esp_http_client_handle_t,int);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
int esp_http_client_set_redirection(esp_http_client_handle_t);
int esp_http_client_close(esp_http_client_handle_t);
int esp_http_client_read(esp_http_client_handle_t,char *,int);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t);
int esp_http_client_cleanup(esp_http_client_handle_t);
typedef int esp_audio_err_t;
#define ESP_AUDIO_ERR_OK 0
#define ESP_AUDIO_ERR_MEM_LACK -2
#define ESP_AUDIO_SIMPLE_DEC_TYPE_MP3 1
typedef void *esp_audio_simple_dec_handle_t;
typedef struct { int dec_type; } esp_audio_simple_dec_cfg_t;
typedef struct { uint8_t *buffer; uint32_t len; bool eos; uint32_t consumed; } esp_audio_simple_dec_raw_t;
typedef struct { uint8_t *buffer; uint32_t len,needed_size,decoded_size; } esp_audio_simple_dec_out_t;
typedef struct { unsigned bits_per_sample,channel,sample_rate; } esp_audio_simple_dec_info_t;
int esp_audio_dec_register_default(void);
int esp_audio_simple_dec_register_default(void);
int esp_audio_simple_dec_open(const esp_audio_simple_dec_cfg_t *,esp_audio_simple_dec_handle_t *);
int esp_audio_simple_dec_close(esp_audio_simple_dec_handle_t);
int esp_audio_simple_dec_process(esp_audio_simple_dec_handle_t,esp_audio_simple_dec_raw_t *,esp_audio_simple_dec_out_t *);
int esp_audio_simple_dec_get_info(esp_audio_simple_dec_handle_t,esp_audio_simple_dec_info_t *);
esp_err_t bsp_audio_init(void);
esp_err_t bsp_audio_init_playback(void);
esp_err_t bsp_audio_deinit(void);
esp_err_t bsp_audio_wake(void);
esp_err_t bsp_audio_sleep(void);
esp_err_t bsp_audio_set_format(uint32_t,uint8_t,uint8_t);
void bsp_audio_set_volume(uint8_t);
esp_err_t bsp_audio_write(const void *,size_t);

/* Single producer/consumer stream used by the music pipeline. */
typedef void *StreamBufferHandle_t;
StreamBufferHandle_t xStreamBufferCreate(size_t,size_t);
size_t xStreamBufferSend(StreamBufferHandle_t,const void *,size_t,uint32_t);
size_t xStreamBufferReceive(StreamBufferHandle_t,void *,size_t,uint32_t);
size_t xStreamBufferBytesAvailable(StreamBufferHandle_t);
void vStreamBufferDelete(StreamBufferHandle_t);
void vTaskSuspend(TaskHandle_t);
int64_t esp_timer_get_time(void);
