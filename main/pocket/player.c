#include "services.h"
#include "bsp_audio.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

static const char *TAG = "pocket_audio";
static TaskHandle_t s_worker;
static SemaphoreHandle_t s_lock;
static EventGroupHandle_t s_events;
static atomic_uint s_generation, s_track;
static atomic_bool s_requested, s_paused, s_radio;
static pf_playback_t s_status;
#define WORKER_IDLE BIT0

static void status(pf_play_state_t state, const char *error)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = state;
    snprintf(s_status.error, sizeof(s_status.error), "%s", error ? error : "");
    xSemaphoreGive(s_lock);
}
static bool cancelled(unsigned generation)
{
    return generation != atomic_load(&s_generation) || !atomic_load(&s_requested);
}
static void heap_log(const char *stage)
{
    ESP_LOGI(TAG, "%s heap=%u largest=%u stack=%u", stage,
        (unsigned)esp_get_free_heap_size(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)uxTaskGetStackHighWaterMark(NULL));
}
static void play(unsigned generation, unsigned track, bool radio)
{
    esp_http_client_handle_t http = NULL;
    esp_audio_simple_dec_handle_t decoder = NULL;
    uint8_t *input = NULL, *output = NULL;
    pf_config_t *config = malloc(sizeof(*config));
    char url[PF_URL];
    const char *error = NULL;
    uint8_t volume = 35;
    bool owns_http = false;
    uint64_t played_us = 0;
    bool produced = false, finished = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status = (pf_playback_t){.state = PF_CONNECTING, .track = track};
    xSemaphoreGive(s_lock);
    if (!config) { error = "内存不足"; goto done; }
    pf_config_get(config);
    if (!radio && track >= config->count) { error = "请先添加音乐来源"; goto done; }
    if (radio) {
        pf_track_t station;
        if (!pf_radio_track(track, &station)) { error = "当前城市暂无电台"; goto done; }
        strcpy(url, station.url);
    } else strcpy(url, config->tracks[track].url);
    volume = config->volume;
    free(config); config = NULL;
    if (!pf_network_ready()) { error = "未联网，请进入手机设置"; goto done; }
    if (!strncmp(url, "https://", 8)) {
        for (unsigned i = 0; i < 100 && time(NULL) < 1704067200; i++) {
            if (cancelled(generation)) goto done;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (time(NULL) < 1704067200) { error = "网络校时失败，请稍后重试"; goto done; }
    }
    while (!(owns_http = pf_http_take(100))) if (cancelled(generation)) goto done;
    if (cancelled(generation)) goto done;
    heap_log("before HTTP");
    esp_http_client_config_t cfg = {
        .url = url, .timeout_ms = 1800, .buffer_size = 2048,
        .buffer_size_tx = 512, .crt_bundle_attach = esp_crt_bundle_attach,
        .max_redirection_count = 4, .user_agent = "PocketFocus/0.1",
    };
    http = esp_http_client_init(&cfg);
    if (!http) { error = "无法建立连接"; goto done; }
    esp_http_client_set_header(http, "Accept", "audio/mpeg,*/*;q=0.5");
    /* perform() owns redirect handling; open/read requires doing it explicitly. */
    for (unsigned redirects = 0;; redirects++) {
        if (cancelled(generation)) goto done;
        if (esp_http_client_open(http, 0) != ESP_OK) { error = "连接失败，请检查音乐直链"; goto done; }
        int64_t length = esp_http_client_fetch_headers(http);
        int code = esp_http_client_get_status_code(http);
        if (length < 0) { error = "服务器响应失败"; goto done; }
        if (code >= 300 && code < 400 && redirects < 4) {
            if (esp_http_client_set_redirection(http) != ESP_OK) { error = "重定向无效"; goto done; }
            esp_http_client_close(http);
            continue;
        }
        if (code != 200) { error = "音乐服务器拒绝请求"; goto done; }
        break;
    }
    heap_log("after TLS");
    input = malloc(2048);
    output = malloc(4608);
    if (!input || !output) { error = "播放内存不足"; goto done; }
    esp_audio_simple_dec_cfg_t dec_cfg = {.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3};
    if (esp_audio_simple_dec_open(&dec_cfg, &decoder) != ESP_AUDIO_ERR_OK) { error = "解码器启动失败"; goto done; }
    if (bsp_audio_init() != ESP_OK || bsp_audio_wake() != ESP_OK) { error = "音频硬件初始化失败"; goto done; }
    bsp_audio_set_volume(volume);
    heap_log("after decoder");
    uint32_t rate = 0;
    unsigned empty_output_bytes = 0;
    while (!cancelled(generation)) {
        while (atomic_load(&s_paused) && !cancelled(generation)) {
            status(PF_PAUSED, NULL);
            vTaskDelay(pdMS_TO_TICKS(40));
        }
        if (cancelled(generation)) break;
        int got = esp_http_client_read(http, (char *)input, 2048);
        if (got < 0) { error = "网络中断，按确定键重试"; break; }
        if (got == 0) {
            finished = esp_http_client_is_complete_data_received(http);
            if (!finished) error = "连接超时，按确定键重试";
            break;
        }
        esp_audio_simple_dec_raw_t raw = {.buffer = input, .len = got,
            .eos = esp_http_client_is_complete_data_received(http)};
        while (raw.len && !cancelled(generation)) {
            while (atomic_load(&s_paused) && !cancelled(generation)) {
                status(PF_PAUSED, NULL);
                vTaskDelay(pdMS_TO_TICKS(40));
            }
            if (cancelled(generation)) break;
            esp_audio_simple_dec_out_t out = {.buffer = output, .len = 4608};
            esp_audio_err_t ret = esp_audio_simple_dec_process(decoder, &raw, &out);
            if (ret != ESP_AUDIO_ERR_OK || raw.consumed > raw.len || (!raw.consumed && !out.decoded_size)) {
                ESP_LOGE(TAG, "MP3 decode failed: ret=%d input=%u consumed=%u output=%u needed=%u",
                    ret, (unsigned)raw.len, (unsigned)raw.consumed, (unsigned)out.decoded_size, (unsigned)out.needed_size);
                heap_log("decoder failure");
                error = ret == ESP_AUDIO_ERR_MEM_LACK ? "播放内存不足" : "音乐解码失败，请重试";
                goto done;
            }
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
            if (out.decoded_size) {
                esp_audio_simple_dec_info_t info;
                if (esp_audio_simple_dec_get_info(decoder, &info) != ESP_AUDIO_ERR_OK || info.bits_per_sample != 16 || (info.channel != 1 && info.channel != 2) || info.sample_rate < 8000 || info.sample_rate > 48000 || out.decoded_size > 4608 || out.decoded_size % (2 * info.channel)) { error = "不支持此音频格式"; goto done; }
                if (rate != info.sample_rate) {
                    if (bsp_audio_set_format(info.sample_rate, 16, 1) != ESP_OK) { error = "音频采样率设置失败"; goto done; }
                    rate = info.sample_rate;
                }
                size_t samples = out.decoded_size / (2 * info.channel);
                int16_t *pcm = (int16_t *)output;
                if (info.channel == 2) for (size_t i = 0; i < samples; i++) pcm[i] = ((int32_t)pcm[i * 2] + pcm[i * 2 + 1]) / 2;
                uint8_t requested_volume = pf_volume_get();
                if (volume != requested_volume) {
                    volume = requested_volume;
                    bsp_audio_set_volume(volume);
                    ESP_LOGI(TAG, "Volume applied: %u%%", (unsigned)volume);
                }
                if (bsp_audio_write(pcm, samples * 2) != ESP_OK) { error = "音频输出失败"; goto done; }
                if (!produced) {
                    ESP_LOGI(TAG, "MP3 PCM started: rate=%u source_channels=%u volume=%u",
                        (unsigned)rate, (unsigned)info.channel, (unsigned)volume);
                    heap_log("first PCM");
                }
                produced = true;
                empty_output_bytes = 0;
                played_us += samples * 1000000ULL / rate;
                xSemaphoreTake(s_lock, portMAX_DELAY);
                s_status.state = PF_PLAYING;
                s_status.seconds = played_us / 1000000;
                xSemaphoreGive(s_lock);
            } else {
                empty_output_bytes += raw.consumed;
                if (empty_output_bytes > 128 * 1024) { error = "音频数据无效或标签过大"; goto done; }
            }
        }
        vTaskDelay(1);
    }
done:
    free(config);
    if (http) esp_http_client_cleanup(http);
    if (decoder) esp_audio_simple_dec_close(decoder);
    if (owns_http) pf_http_give();
    free(input);
    free(output);
    if (bsp_audio_sleep() != ESP_OK && !error) error = "音频休眠失败";
    heap_log("after cleanup");
    if (!cancelled(generation)) status(error ? PF_ERROR : (finished && produced ? PF_FINISHED : PF_ERROR), error ? error : (produced ? NULL : "未收到可播放的音频"));
}
static void worker(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (atomic_load(&s_requested)) {
            unsigned generation = atomic_load(&s_generation);
            unsigned track = atomic_load(&s_track);
            play(generation, track, atomic_load(&s_radio));
            xSemaphoreTake(s_lock, portMAX_DELAY);
            if (generation == atomic_load(&s_generation)) atomic_store(&s_requested, false);
            xSemaphoreGive(s_lock);
        }
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (!atomic_load(&s_requested)) xEventGroupSetBits(s_events, WORKER_IDLE);
        xSemaphoreGive(s_lock);
    }
}
esp_err_t pf_player_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_events = xEventGroupCreate();
    if (!s_lock || !s_events) return ESP_ERR_NO_MEM;
    if (esp_audio_dec_register_default() != ESP_AUDIO_ERR_OK || esp_audio_simple_dec_register_default() != ESP_AUDIO_ERR_OK) return ESP_FAIL;
    if (xTaskCreate(worker, "pocket_audio", 10240, NULL, 4, &s_worker) != pdPASS) return ESP_ERR_NO_MEM;
    xEventGroupSetBits(s_events, WORKER_IDLE);
    return ESP_OK;
}
static esp_err_t start(unsigned track, bool radio)
{
    if (!s_worker || track >= PF_TRACKS) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    atomic_store(&s_track, track);
    atomic_store(&s_radio, radio);
    atomic_store(&s_paused, false);
    atomic_fetch_add(&s_generation, 1);
    atomic_store(&s_requested, true);
    xEventGroupClearBits(s_events, WORKER_IDLE);
    s_status.state = PF_CONNECTING;
    s_status.error[0] = 0;
    xSemaphoreGive(s_lock);
    xTaskNotifyGive(s_worker);
    return ESP_OK;
}
esp_err_t pf_player_start(unsigned track) { return start(track, false); }
esp_err_t pf_player_radio(unsigned track) { return start(track, true); }
esp_err_t pf_player_stop(void)
{
    if (!s_worker) return ESP_OK;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    atomic_store(&s_requested, false);
    atomic_fetch_add(&s_generation, 1);
    xEventGroupClearBits(s_events, WORKER_IDLE);
    xSemaphoreGive(s_lock);
    xTaskNotifyGive(s_worker);
    if (!(xEventGroupWaitBits(s_events, WORKER_IDLE, false, true, pdMS_TO_TICKS(12000)) & WORKER_IDLE)) return ESP_ERR_TIMEOUT;
    status(PF_IDLE, NULL);
    return ESP_OK;
}
void pf_player_pause(void) { atomic_store(&s_paused, !atomic_load(&s_paused)); }
void pf_player_snapshot(pf_playback_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_status;
    xSemaphoreGive(s_lock);
    out->volume = pf_volume_get();
}
