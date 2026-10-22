#pragma once
#include "../runtime_stubs/runtime.h"
#include <inttypes.h>
#define BIT1 2
#define BIT2 4
#define BIT3 8
typedef unsigned EventBits_t;
typedef void *QueueHandle_t;
unsigned xEventGroupGetBits(EventGroupHandle_t);
QueueHandle_t xQueueCreate(unsigned,unsigned);
int xQueueSend(QueueHandle_t,const void *,uint32_t);
int xQueueReceive(QueueHandle_t,void *,uint32_t);
void vQueueDelete(QueueHandle_t);
void vTaskSuspend(TaskHandle_t);
int64_t esp_timer_get_time(void);
unsigned esp_get_minimum_free_heap_size(void);
#define ESP_MAC_WIFI_STA 1
esp_err_t esp_read_mac(uint8_t *,int);
void esp_fill_random(void *,size_t);
typedef unsigned nvs_handle_t;
#define NVS_READWRITE 1
esp_err_t nvs_open(const char *,int,nvs_handle_t *);
esp_err_t nvs_get_str(nvs_handle_t,const char *,char *,size_t *);
esp_err_t nvs_set_str(nvs_handle_t,const char *,const char *);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
#define HTTP_METHOD_POST 1
int esp_http_client_set_method(esp_http_client_handle_t,int);
int esp_http_client_write(esp_http_client_handle_t,const char *,int);
esp_err_t bsp_audio_read(void *,size_t);
typedef const char *esp_event_base_t;
typedef void (*event_cb_t)(void *,esp_event_base_t,int32_t,void *);
#define ESP_EVENT_ANY_ID -1
#define WEBSOCKET_EVENT_ANY -1
#define WEBSOCKET_EVENT_CONNECTED 1
#define WEBSOCKET_EVENT_DISCONNECTED 2
#define WEBSOCKET_EVENT_DATA 3
#define WEBSOCKET_EVENT_ERROR 4
#define MQTT_EVENT_CONNECTED 1
#define MQTT_EVENT_DISCONNECTED 2
#define MQTT_EVENT_DATA 3
#define MQTT_EVENT_ERROR 4
typedef void *esp_websocket_client_handle_t;
typedef struct {
    const char *uri,*headers;
    void (*crt_bundle_attach)(void);
    bool disable_auto_reconnect;
    int network_timeout_ms,task_stack,task_prio,buffer_size;
} esp_websocket_client_config_t;
typedef struct {
    const char *data_ptr;
    int data_len;
    bool fin;
    uint8_t op_code;
    int payload_len,payload_offset;
} esp_websocket_event_data_t;
esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t *);
int esp_websocket_register_events(esp_websocket_client_handle_t,int,event_cb_t,void *);
int esp_websocket_client_start(esp_websocket_client_handle_t);
int esp_websocket_client_stop(esp_websocket_client_handle_t);
int esp_websocket_client_destroy(esp_websocket_client_handle_t);
int esp_websocket_client_send_text(esp_websocket_client_handle_t,const char *,int,uint32_t);
int esp_websocket_client_send_bin(esp_websocket_client_handle_t,const char *,int,uint32_t);
typedef void *esp_mqtt_client_handle_t;
typedef struct {
    struct { struct { const char *uri; } address; struct { void (*crt_bundle_attach)(void); } verification; } broker;
    struct { const char *client_id,*username; struct { const char *password; } authentication; } credentials;
    struct { unsigned keepalive; } session;
    struct { bool disable_auto_reconnect; unsigned timeout_ms; } network;
    struct { unsigned stack_size,priority; } task;
    struct { unsigned size,out_size; } buffer;
} esp_mqtt_client_config_t;
typedef struct { int current_data_offset,total_data_len,data_len;const char *data; } esp_mqtt_event_t;
typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *);
int esp_mqtt_client_register_event(esp_mqtt_client_handle_t,int,event_cb_t,void *);
int esp_mqtt_client_start(esp_mqtt_client_handle_t);
int esp_mqtt_client_stop(esp_mqtt_client_handle_t);
int esp_mqtt_client_destroy(esp_mqtt_client_handle_t);
int esp_mqtt_client_publish(esp_mqtt_client_handle_t,const char *,const char *,int,int,int);
typedef struct { unsigned dummy; } mbedtls_aes_context;
void mbedtls_aes_init(mbedtls_aes_context *);
void mbedtls_aes_free(mbedtls_aes_context *);
int mbedtls_aes_setkey_enc(mbedtls_aes_context *,const uint8_t *,unsigned);
int mbedtls_aes_crypt_ctr(mbedtls_aes_context *,size_t,size_t *,uint8_t *,uint8_t *,const uint8_t *,uint8_t *);
void mbedtls_platform_zeroize(void *,size_t);
typedef struct {
    int sample_rate,channel,bits_per_sample,bitrate,frame_duration,application_mode,complexity;
    bool enable_vbr;
} esp_opus_enc_config_t;
typedef struct { unsigned sample_rate,channel;int frame_duration;bool self_delimited; } esp_opus_dec_cfg_t;
#define ESP_OPUS_ENC_FRAME_DURATION_60_MS 5
#define ESP_OPUS_DEC_FRAME_DURATION_60_MS 5
#define ESP_OPUS_ENC_APPLICATION_VOIP 0
typedef struct { uint8_t *buffer;unsigned len; } esp_audio_enc_in_frame_t;
typedef struct { uint8_t *buffer;unsigned len,encoded_bytes; } esp_audio_enc_out_frame_t;
typedef struct { uint8_t *buffer;unsigned len,consumed; } esp_audio_dec_in_raw_t;
typedef struct { uint8_t *buffer;unsigned len,decoded_size; } esp_audio_dec_out_frame_t;
typedef struct { unsigned sample_rate,channel,bits_per_sample; } esp_audio_dec_info_t;
int esp_opus_enc_open(void *,uint32_t,void **);
int esp_opus_enc_get_frame_size(void *,int *,int *);
int esp_opus_enc_process(void *,esp_audio_enc_in_frame_t *,esp_audio_enc_out_frame_t *);
void esp_opus_enc_close(void *);
int esp_opus_dec_open(void *,uint32_t,void **);
int esp_opus_dec_decode(void *,esp_audio_dec_in_raw_t *,esp_audio_dec_out_frame_t *,esp_audio_dec_info_t *);
void esp_opus_dec_close(void *);
