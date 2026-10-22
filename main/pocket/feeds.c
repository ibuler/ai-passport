#include "services.h"
#include "github.h"
#include "xiaozhi.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <stdatomic.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define WEATHER BIT0
#define RADIO BIT1
#define GITHUB BIT2
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
static EventGroupHandle_t s_events;
static atomic_bool s_stop;
static atomic_uint s_deferred;
#define FEEDS_DONE BIT0
static pf_weather_t s_weather;
static pf_catalog_t s_radio;
static uint64_t s_attempt;
static uint64_t s_github_attempt;
static pf_github_t s_github;
static bool github_yield(void)
{
    if (atomic_load(&s_stop) || pf_settings_active() || pf_xz_active()) return true;
    pf_playback_t play; pf_player_snapshot(&play);
    return play.state == PF_CONNECTING || play.state == PF_PLAYING || play.state == PF_PAUSED;
}

static void github(const char *username, int64_t today)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_github.loading = true;
    xSemaphoreGive(s_lock);
    char url[128];
    snprintf(url, sizeof(url), "https://github.com/users/%s/contributions", username);
    unsigned heap_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    unsigned largest_before = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    esp_http_client_config_t cfg = {.url=url, .timeout_ms=2000, .buffer_size=1024, .buffer_size_tx=512,
        .crt_bundle_attach=esp_crt_bundle_attach, .user_agent="PocketPassport/0.3", .disable_auto_redirect=true};
    esp_http_client_handle_t http = esp_http_client_init(&cfg);
    pf_github_parser_t parser;
    pf_github_parser_init(&parser, today, username);
    pf_github_t result;
    bool ok = false;
    esp_err_t opened = ESP_FAIL;
    int status = 0, last_read = 0; int64_t length = -1; size_t used = 0; bool complete = false;
    if (http && !pf_settings_active() && (opened = esp_http_client_open(http, 0)) == ESP_OK) {
        length = esp_http_client_fetch_headers(http); status = esp_http_client_get_status_code(http);
        if (length >= 0 && length <= 393216 && status == 200) {
            char chunk[1024];
            int64_t end = esp_timer_get_time() + 15000000;
            while (used < 393216 && esp_timer_get_time() < end && !github_yield()) {
                int n = esp_http_client_read(http, chunk, sizeof(chunk)); last_read = n;
                if (n <= 0) break;
                used += n; pf_github_parser_feed(&parser, chunk, n);
                if (parser.failed) break;
            }
            complete = esp_http_client_is_complete_data_received(http);
            ok = used <= 393216 && complete &&
                 !pf_settings_active() && pf_github_parser_finish(&parser, &result);
        }
    }
    if (http) esp_http_client_cleanup(http);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (ok) {
        result.updated_ms = esp_timer_get_time() / 1000;
        s_github = result;
    } else {
        s_github.loading = false; s_github.stale = true;
        strcpy(s_github.message, "贡献图暂不可用");
    }
    xSemaphoreGive(s_lock);
    ESP_LOGI("pocket_feeds", "GitHub @%s: %s", username, ok ? "26-week calendar updated" : "fetch unavailable; previous data retained");
    ESP_LOGI("pocket_feeds", "calendar fetch: UTC day=%lld open=%s HTTP=%d length=%lld read=%u complete=%d last_read=%d parser_failed=%d stack_free=%u heap_before=%u largest_before=%u",
        (long long)today, esp_err_to_name(opened), status, (long long)length, (unsigned)used, complete, last_read, parser.failed,
        (unsigned)uxTaskGetStackHighWaterMark(NULL), heap_before, largest_before);
}
static cJSON *fetch_json(const char *url, size_t cap)
{
    if (atomic_load(&s_stop) || pf_settings_active() || pf_xz_active()) return NULL;
    char *body = NULL;
    esp_http_client_config_t cfg = {.url=url, .timeout_ms=1500, .buffer_size=1024, .buffer_size_tx=512,
        .crt_bundle_attach=esp_crt_bundle_attach, .user_agent="PocketPassport/0.2"};
    esp_http_client_handle_t http = esp_http_client_init(&cfg);
    cJSON *json = NULL;
    if (http && esp_http_client_open(http, 0) == ESP_OK) {
        int64_t length = esp_http_client_fetch_headers(http);
        if (length >= 0 && length <= (int64_t)cap && esp_http_client_get_status_code(http) == 200) {
            if (length > 0) cap = (size_t)length;
            body = malloc(cap + 1);
            if (!body) { esp_http_client_cleanup(http); return NULL; }
            size_t used = 0;
            int64_t end = esp_timer_get_time() + 6000000;
            while (used < cap && esp_timer_get_time() < end && !atomic_load(&s_stop) && !pf_settings_active() && !pf_xz_active()) {
                int n = esp_http_client_read(http, body + used, cap - used > 1024 ? 1024 : cap - used);
                if (n <= 0) break;
                used += n;
            }
            body[used] = 0;
            if (esp_http_client_is_complete_data_received(http) && pf_json_depth_valid(body, used)) json = cJSON_ParseWithLengthOpts(body, used + 1, NULL, true);
        }
    }
    if (http) esp_http_client_cleanup(http);
    free(body);
    return json;
}
static bool number(cJSON *root, const char *key, double min, double max, double *out)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < min || v->valuedouble > max) return false;
    *out = v->valuedouble; return true;
}
static void weather(const pf_profile_t *profile)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (strcmp(s_weather.city, profile->city)) memset(&s_weather, 0, sizeof(s_weather));
    strcpy(s_weather.city, profile->city); s_weather.loading = true;
    xSemaphoreGive(s_lock);
    char url[320];
    double latitude = profile->latitude_e4 / 10000.0, longitude = profile->longitude_e4 / 10000.0;
    bool located = profile->latitude_e4 != 0 || profile->longitude_e4 != 0;
    if (!located) {
        char city[PF_CITY * 3];
        static const char hex[] = "0123456789ABCDEF";
        size_t at = 0;
        for (const unsigned char *c = (const unsigned char *)profile->city; *c; c++) { city[at++]='%'; city[at++]=hex[*c >> 4]; city[at++]=hex[*c & 15]; }
        city[at] = 0;
        snprintf(url, sizeof(url), "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=zh&format=json", city);
        cJSON *geo = fetch_json(url, 4096);
        cJSON *place = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(geo, "results"), 0);
        located = number(place, "latitude", -90, 90, &latitude) && number(place, "longitude", -180, 180, &longitude);
        cJSON_Delete(geo);
    }
    snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&forecast_days=1", latitude, longitude);
    cJSON *json = located ? fetch_json(url, 4096) : NULL;
    cJSON *current = cJSON_GetObjectItemCaseSensitive(json, "current");
    double temp, code, humidity, wind;
    bool ok = number(current, "temperature_2m", -100, 80, &temp) && number(current, "weather_code", 0, 99, &code) && number(current, "relative_humidity_2m", 0, 100, &humidity) && number(current, "wind_speed_10m", 0, 500, &wind);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_weather.loading = false; s_weather.stale = !ok;
    if (ok) {
        s_weather.valid = true; s_weather.temperature_tenths = lround(temp * 10); s_weather.code = (int)code;
        s_weather.humidity = (int)humidity; s_weather.wind_tenths = lround(wind * 10);
        s_weather.updated_ms = esp_timer_get_time() / 1000; s_weather.message[0] = 0;
    } else pf_notice_text(s_weather.message, sizeof(s_weather.message), "天气获取失败，确定键重试");
    xSemaphoreGive(s_lock); cJSON_Delete(json);
}
static bool copy_string(cJSON *root, const char *key, char *out, size_t cap)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(v) || !pf_text_valid(v->valuestring, cap, false)) return false;
    strcpy(out, v->valuestring); return true;
}
static void radio(const pf_profile_t *p)
{
    pf_catalog_t *list = calloc(1, sizeof(*list));
    if (!list) return;
    strcpy(list->city, p->city);
    for (unsigned i = 0; i < p->radio_count; i++) if (!strcmp(p->city, p->radios[i].city)) list->tracks[list->count++] = p->radios[i].track;
    if (!list->count) {
        char city[PF_CITY * 3], url[512];
        static const char hex[] = "0123456789ABCDEF";
        size_t at = 0;
        for (const unsigned char *c = (const unsigned char *)p->city; *c; c++) { city[at++]='%'; city[at++]=hex[*c >> 4]; city[at++]=hex[*c & 15]; }
        city[at] = 0;
        snprintf(url, sizeof(url), "https://de1.api.radio-browser.info/json/stations/search?name=%s&codec=MP3&codecExact=true&hidebroken=true&limit=6&order=clickcount&reverse=true", city);
        cJSON *json = fetch_json(url, 12288);
        if (cJSON_IsArray(json)) for (int i = 0; i < cJSON_GetArraySize(json) && list->count < PF_RADIOS; i++) {
            cJSON *station = cJSON_GetArrayItem(json, i), *hls = cJSON_GetObjectItemCaseSensitive(station, "hls");
            cJSON *codec = cJSON_GetObjectItemCaseSensitive(station, "codec");
            pf_track_t *t = &list->tracks[list->count];
            if (cJSON_IsString(codec) && !strcmp(codec->valuestring, "MP3") && cJSON_IsNumber(hls) && hls->valueint == 0 && copy_string(station, "name", t->title, PF_TITLE) && copy_string(station, "url_resolved", t->url, PF_URL) && pf_url_valid(t->url)) list->count++;
        }
        snprintf(list->message, sizeof(list->message), "%s", !json ? "电台目录暂不可用，请重试" : !list->count ? "此城市暂无 MP3 电台" : "来源 Radio Browser");
        cJSON_Delete(json);
    } else strcpy(list->message, "使用后台配置的本地电台");
    xSemaphoreTake(s_lock, portMAX_DELAY); list->revision = s_radio.revision + 1; s_radio = *list; xSemaphoreGive(s_lock);
    free(list);
}
static void worker(void *arg)
{
    (void)arg;
    uint32_t pending = 0;
    for (;;) {
        uint32_t wanted = 0;
        xTaskNotifyWait(0, UINT32_MAX, &wanted, pdMS_TO_TICKS(5000));
        pending |= atomic_exchange(&s_deferred, 0);
        if (atomic_load(&s_stop)) break;
        /* Check before allocating the multi-KiB profile, even when no HTTP is due. */
        if (github_yield()) continue;
        if (!pf_http_take(0)) continue;
        /* Playback/settings may have started between the check and the mutex. */
        if (github_yield()) { pf_http_give(); continue; }
        pf_profile_t *p = malloc(sizeof(*p));
        if (!p) { pf_http_give(); continue; }
        pf_profile_get(p);
        if ((pending & RADIO) && p->city[0] && pf_radio_count(p)) { radio(p); if (!atomic_load(&s_stop)) pending &= ~RADIO; }
        uint64_t now = esp_timer_get_time() / 1000;
        char username[PF_GITHUB_USER]; pf_github_username_get(username);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (strcmp(username, s_github.username)) {
            memset(&s_github, 0, sizeof(s_github)); strcpy(s_github.username, username);
            s_github_attempt = 0;
        }
        bool github_valid = s_github.valid;
        xSemaphoreGive(s_lock);
        pf_playback_t playing; pf_player_snapshot(&playing);
        bool idle = playing.state != PF_PLAYING && playing.state != PF_PAUSED && playing.state != PF_CONNECTING;
        if (!pf_network_ready() || time(NULL) < 1704067200 || pf_settings_active() || !idle) { free(p); pf_http_give(); continue; }
        if (!pf_settings_active()) {
            if (p->city[0] && (pending & RADIO)) { radio(p); if (!atomic_load(&s_stop)) pending &= ~RADIO; }
            if (p->city[0] && ((pending & WEATHER) || !s_attempt || now - s_attempt >= 900000)) { s_attempt = now; weather(p); if (!atomic_load(&s_stop)) pending &= ~WEATHER; }
            if (username[0] && !pf_settings_active() && ((pending & GITHUB) || !s_github_attempt || now - s_github_attempt >= (github_valid ? 3600000 : 300000))) {
                s_github_attempt = now; github(username, time(NULL) / 86400); if (!atomic_load(&s_stop)) pending &= ~GITHUB;
            }
        }
        free(p);
        pf_http_give();
    }
    atomic_fetch_or(&s_deferred, pending);
    xEventGroupSetBits(s_events, FEEDS_DONE);
    vTaskSuspend(NULL);
}
esp_err_t pf_weather_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_events = xEventGroupCreate();
    if (!s_lock || !s_events) return ESP_ERR_NO_MEM;
    return pf_feeds_resume();
}
esp_err_t pf_feeds_pause(void)
{
    if (!s_task) return ESP_OK;
    /* A timed-out stop may acknowledge later; retain that acknowledgement. */
    if (!atomic_exchange(&s_stop, true)) {
        xEventGroupClearBits(s_events, FEEDS_DONE);
        xTaskNotify(s_task, 0, eSetBits);
    }
    if (!(xEventGroupWaitBits(s_events, FEEDS_DONE, false, true, pdMS_TO_TICKS(10000)) & FEEDS_DONE)) return ESP_ERR_TIMEOUT;
    vTaskDelete(s_task); s_task = NULL;
    return ESP_OK;
}
esp_err_t pf_feeds_resume(void)
{
    if (s_task) {
        if (!atomic_load(&s_stop)) return ESP_OK;
        esp_err_t err = pf_feeds_pause();
        if (err != ESP_OK) return err;
    }
    atomic_store(&s_stop, false);
    xEventGroupClearBits(s_events, FEEDS_DONE);
    return xTaskCreate(worker, "pocket_feeds", 8192, NULL, 3, &s_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
static void refresh(unsigned bits)
{
    atomic_fetch_or(&s_deferred, bits);
    if (s_task) xTaskNotify(s_task, 0, eSetBits);
}
void pf_weather_refresh(void) { refresh(WEATHER); }
void pf_radio_refresh(void) { refresh(RADIO); }
void pf_github_refresh(void) { refresh(GITHUB); }
void pf_github_snapshot(pf_github_t *out)
{
    char username[PF_GITHUB_USER]; pf_github_username_get(username);
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_github; xSemaphoreGive(s_lock);
    if (strcmp(username, out->username)) { memset(out, 0, sizeof(*out)); strcpy(out->username, username); }
    if (out->valid && (uint64_t)(esp_timer_get_time() / 1000) - out->updated_ms >= 3600000) out->stale = true;
}
void pf_weather_snapshot(pf_weather_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_weather; xSemaphoreGive(s_lock);
    if (out->valid && (uint64_t)(esp_timer_get_time() / 1000) - out->updated_ms >= 900000) out->stale = true;
}
void pf_radio_catalog(pf_catalog_t *out) { xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_radio; xSemaphoreGive(s_lock); }
bool pf_radio_track(unsigned index, pf_track_t *out)
{
    bool found;
    xSemaphoreTake(s_lock, portMAX_DELAY); found = index < s_radio.count;
    if (found) *out = s_radio.tracks[index];
    xSemaphoreGive(s_lock); return found;
}

unsigned pf_radio_revision(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY); unsigned revision = s_radio.revision; xSemaphoreGive(s_lock); return revision;
}
unsigned pf_radio_config(pf_config_t *out, const char *city, char *note, size_t cap)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out->count = !strcmp(city, s_radio.city) ? s_radio.count : 0;
    for (unsigned i = 0; i < out->count; i++) out->tracks[i] = s_radio.tracks[i];
    snprintf(note, cap, "%s", !strcmp(city, s_radio.city) ? s_radio.message : "正在查找当前城市电台");
    unsigned revision = s_radio.revision;
    xSemaphoreGive(s_lock); return revision;
}
