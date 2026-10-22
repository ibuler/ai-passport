#include "services.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include <math.h>
#include "nvs.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONNECTED BIT0
#define FORM_MAX 7168
static const char *TAG = "pocket_net";
static EventGroupHandle_t s_events;
static SemaphoreHandle_t s_http;
static httpd_handle_t s_server;
static nvs_handle_t s_nvs;
static wifi_config_t s_saved;
static atomic_bool s_settings;
static atomic_bool s_trying;
static atomic_bool s_has_credentials;
static unsigned s_retries;
static char s_ap[32], s_password[13], s_token[33];
extern const unsigned char settings_html_start[] asm("_binary_settings_html_gz_start");
extern const unsigned char settings_html_end[] asm("_binary_settings_html_gz_end");

static void network_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retries = 0;
        xEventGroupSetBits(s_events, CONNECTED);
        (void)esp_netif_sntp_start();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_events, CONNECTED);
        if (!atomic_load(&s_trying) && atomic_load(&s_has_credentials) && s_retries++ < 8) esp_wifi_connect();
    }
}

esp_err_t pf_network_init(void)
{
    s_events = xEventGroupCreate();
    s_http = xSemaphoreCreateMutex();
    if (!s_events || !s_http) return ESP_ERR_NO_MEM;
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    if (!esp_netif_create_default_wifi_sta() || !esp_netif_create_default_wifi_ap()) return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "RAM storage");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event, NULL), TAG, "wifi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, network_event, NULL), TAG, "IP events");
    ESP_RETURN_ON_ERROR(nvs_open("pocket_focus", NVS_READWRITE, &s_nvs), TAG, "NVS");
    size_t bytes = sizeof(s_saved);
    if (nvs_get_blob(s_nvs, "wifi", &s_saved, &bytes) != ESP_OK || bytes != sizeof(s_saved)) memset(&s_saved, 0, sizeof(s_saved));
    atomic_store(&s_has_credentials, s_saved.sta.ssid[0] != 0);
    esp_sntp_config_t time_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    time_config.start = false;
    ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&time_config), TAG, "SNTP");
    uint8_t mac[6];
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP), TAG, "MAC");
    snprintf(s_ap, sizeof(s_ap), "Pocket-Focus-%02X%02X", mac[4], mac[5]);
    snprintf(s_password, sizeof(s_password), "%08lx", (unsigned long)esp_random());
    for (unsigned i = 0; i < 4; i++) snprintf(s_token + i * 8, 9, "%08lx", (unsigned long)esp_random());
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "STA");
    if (s_saved.sta.ssid[0]) ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &s_saved), TAG, "STA config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");
    if (s_saved.sta.ssid[0]) esp_wifi_connect();
    return ESP_OK;
}
bool pf_network_ready(void) { return (xEventGroupGetBits(s_events) & CONNECTED) != 0; }
bool pf_http_take(uint32_t wait_ms) { return xSemaphoreTake(s_http, pdMS_TO_TICKS(wait_ms)) == pdTRUE; }
void pf_http_give(void) { xSemaphoreGive(s_http); }
bool pf_settings_active(void) { return atomic_load(&s_settings); }
const char *pf_ap_name(void) { return s_ap; }
const char *pf_ap_password(void) { return s_password; }

static esp_err_t reply(httpd_req_t *r, const char *status, const char *message)
{
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_sendstr(r, message);
}
static bool authorized(httpd_req_t *r)
{
    char token[40];
    return atomic_load(&s_settings) && httpd_req_get_hdr_value_str(r, "X-Pocket-Token", token, sizeof(token)) == ESP_OK && !strcmp(token, s_token);
}
static bool receive(httpd_req_t *r, void *target, size_t bytes, int64_t deadline)
{
    char *p = target;
    while (bytes && atomic_load(&s_settings) && esp_timer_get_time() < deadline) {
        int got = httpd_req_recv(r, p, bytes > 1024 ? 1024 : bytes);
        if (got <= 0) return false;
        p += got;
        bytes -= got;
    }
    return bytes == 0;
}
static cJSON *body_json(httpd_req_t *r)
{
    if (!r->content_len || r->content_len > FORM_MAX) return NULL;
    char *body = calloc(1, r->content_len + 1);
    if (!body) return NULL;
    cJSON *json = NULL;
    if (receive(r, body, r->content_len, esp_timer_get_time() + 10000000) && pf_json_depth_valid(body, r->content_len)) {
        const char *end;
        json = cJSON_ParseWithLengthOpts(body, r->content_len + 1, &end, true);
        if (json && !cJSON_IsObject(json)) { cJSON_Delete(json); json = NULL; }
    }
    free(body);
    return json;
}
static bool copy_json(cJSON *root, const char *key, char *out, size_t cap)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || !pf_text_valid(item->valuestring, cap, true)) return false;
    strcpy(out, item->valuestring);
    return true;
}
static esp_err_t root_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(r, "Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; connect-src 'self'; frame-ancestors 'none'");
    return httpd_resp_send(r, (const char *)settings_html_start, settings_html_end - settings_html_start);
}
static cJSON *profile_json(void)
{
    pf_profile_t *p = malloc(sizeof(*p));
    if (!p) return NULL;
    pf_profile_get(p);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "company", p->company); cJSON_AddStringToObject(j, "city", p->city);
    cJSON_AddNumberToObject(j, "latitude_e4", p->latitude_e4); cJSON_AddNumberToObject(j, "longitude_e4", p->longitude_e4);
    cJSON_AddNumberToObject(j, "utc_minutes", p->utc_minutes);
    cJSON *radios = cJSON_AddArrayToObject(j, "radios");
    for (unsigned i = 0; i < p->radio_count; i++) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddStringToObject(r, "city", p->radios[i].city); cJSON_AddStringToObject(r, "title", p->radios[i].track.title);
        cJSON_AddStringToObject(r, "url", p->radios[i].track.url); cJSON_AddItemToArray(radios, r);
    }
    free(p); return j;
}
static bool integer(cJSON *j, const char *name, int min, int max, int *out)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(j, name);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < min || v->valuedouble > max || v->valuedouble != v->valueint) return false;
    *out = v->valueint; return true;
}
static esp_err_t profile_post(httpd_req_t *r)
{
    if (!authorized(r)) return reply(r, "403 Forbidden", "请重新打开设备后台");
    cJSON *j = body_json(r);
    pf_profile_t *p = calloc(1, sizeof(*p));
    int lat = 0, lon = 0, utc = 480;
    cJSON *radios = cJSON_GetObjectItemCaseSensitive(j, "radios");
    bool valid = p && j && copy_json(j, "company", p->company, PF_TITLE) && copy_json(j, "city", p->city, PF_CITY) &&
        integer(j, "latitude_e4", -900000, 900000, &lat) && integer(j, "longitude_e4", -1800000, 1800000, &lon) &&
        integer(j, "utc_minutes", -720, 840, &utc) && cJSON_IsArray(radios) && cJSON_GetArraySize(radios) <= PF_RADIOS;
    if (valid) {
        p->version = 1; p->latitude_e4 = lat; p->longitude_e4 = lon; p->utc_minutes = utc; p->radio_count = cJSON_GetArraySize(radios);
        for (unsigned i = 0; i < p->radio_count; i++) {
            cJSON *radio = cJSON_GetArrayItem(radios, i);
            if (!copy_json(radio, "city", p->radios[i].city, PF_CITY) || !copy_json(radio, "title", p->radios[i].track.title, PF_TITLE) || !copy_json(radio, "url", p->radios[i].track.url, PF_URL)) valid = false;
        }
        valid = valid && pf_profile_valid(p);
    }
    esp_err_t err = valid ? pf_profile_save(p) : ESP_ERR_INVALID_ARG;
    free(p); cJSON_Delete(j);
    return reply(r, err == ESP_OK ? "200 OK" : "400 Bad Request", err == ESP_OK ? "公司、城市及电台已保存，返回首页后生效" : "请检查城市坐标、时区及最多 6 条电台直链");
}
static esp_err_t github_post(httpd_req_t *r)
{
    if (!authorized(r)) return reply(r, "403 Forbidden", "请重新打开设备后台");
    cJSON *j = body_json(r);
    char username[PF_GITHUB_USER];
    bool valid = j && copy_json(j, "username", username, sizeof(username)) && pf_github_username_valid(username);
    esp_err_t err = valid ? pf_github_username_save(username) : ESP_ERR_INVALID_ARG;
    cJSON_Delete(j);
    if (err == ESP_OK) pf_github_refresh();
    return reply(r, err == ESP_OK ? "200 OK" : "400 Bad Request", err == ESP_OK ? "GitHub ID 已保存，退出后台后更新贡献图" : "请输入有效 GitHub ID，最多 39 位字母、数字或单连字符");
}
static esp_err_t config_get(httpd_req_t *r)
{
    pf_config_t *c = malloc(sizeof(*c));
    if (!c) return reply(r, "503 Service Unavailable", "内存不足");
    pf_config_get(c);
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "token", s_token);
    cJSON_AddStringToObject(json, "name", c->name);
    cJSON_AddNumberToObject(json, "volume", c->volume);
    cJSON_AddBoolToObject(json, "connected", pf_network_ready());
    char username[PF_GITHUB_USER]; pf_github_username_get(username);
    cJSON_AddStringToObject(json, "github_username", username);
    cJSON_AddItemToObject(json, "profile", profile_json());
    cJSON *tracks = cJSON_AddArrayToObject(json, "tracks");
    for (unsigned i = 0; i < c->count; i++) {
        cJSON *t = cJSON_CreateObject();
        cJSON_AddStringToObject(t, "title", c->tracks[i].title);
        cJSON_AddStringToObject(t, "url", c->tracks[i].url);
        cJSON_AddItemToArray(tracks, t);
    }
    char *body = cJSON_PrintUnformatted(json);
    free(c);
    cJSON_Delete(json);
    if (!body) return reply(r, "503 Service Unavailable", "内存不足");
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(r, body);
    free(body);
    return err;
}
static esp_err_t config_post(httpd_req_t *r)
{
    if (!authorized(r)) return reply(r, "403 Forbidden", "请重新打开设备设置页");
    cJSON *json = body_json(r);
    pf_config_t *c = calloc(1, sizeof(*c));
    bool valid = json && c;
    if (valid) {
        c->version = 1;
        cJSON *v = cJSON_GetObjectItemCaseSensitive(json, "volume");
        cJSON *tracks = cJSON_GetObjectItemCaseSensitive(json, "tracks");
        valid = copy_json(json, "name", c->name, sizeof(c->name)) && cJSON_IsNumber(v) && v->valuedouble >= 0 && v->valuedouble <= 100 && v->valuedouble == v->valueint && cJSON_IsArray(tracks) && cJSON_GetArraySize(tracks) <= PF_TRACKS;
        if (valid) {
            c->volume = v->valueint;
            c->count = cJSON_GetArraySize(tracks);
            for (unsigned i = 0; i < c->count; i++) {
                cJSON *t = cJSON_GetArrayItem(tracks, i);
                if (!copy_json(t, "title", c->tracks[i].title, PF_TITLE) || !copy_json(t, "url", c->tracks[i].url, PF_URL)) valid = false;
            }
            valid = valid && pf_config_valid(c);
        }
    }
    esp_err_t err = valid ? pf_config_save(c) : ESP_ERR_INVALID_ARG;
    free(c);
    cJSON_Delete(json);
    if (err == ESP_ERR_INVALID_ARG) return reply(r, "400 Bad Request", "检查名称、音量及最多 8 条 HTTP/HTTPS 音乐直链");
    return reply(r, err == ESP_OK ? "200 OK" : "500 Internal Server Error", err == ESP_OK ? "已保存，长按设备确定键返回" : "保存失败，原配置仍保留");
}
static bool playlist_valid(cJSON *json)
{
    int version;
    cJSON *tracks = cJSON_GetObjectItemCaseSensitive(json, "tracks");
    if (!cJSON_IsObject(json) || !integer(json, "version", 1, 1, &version) || !cJSON_IsArray(tracks) ||
        cJSON_GetArraySize(tracks) < 1 || cJSON_GetArraySize(tracks) > PF_TRACKS) return false;
    pf_track_t track;
    for (int i = 0; i < cJSON_GetArraySize(tracks); i++) {
        cJSON *item = cJSON_GetArrayItem(tracks, i);
        if (!copy_json(item, "title", track.title, PF_TITLE) || !pf_text_valid(track.title, PF_TITLE, false) ||
            !copy_json(item, "url", track.url, PF_URL) || !pf_url_valid(track.url)) return false;
    }
    return true;
}
static esp_err_t playlist_post(httpd_req_t *r)
{
    if (!authorized(r)) return reply(r, "403 Forbidden", "请重新打开设备后台");
    cJSON *request = body_json(r);
    char url[PF_URL];
    bool valid = request && copy_json(request, "url", url, sizeof(url)) && pf_url_valid(url);
    cJSON_Delete(request);
    if (!valid) return reply(r, "400 Bad Request", "请输入有效的 HTTP / HTTPS 歌单 JSON 地址");
    if (!pf_network_ready()) return reply(r, "503 Service Unavailable", "设备尚未联网，请先配置 Wi-Fi，或导入本地 JSON 文件");
    if (!pf_http_take(0)) return reply(r, "503 Service Unavailable", "网络忙，请稍后重试");
    esp_http_client_config_t cfg = {.url=url, .timeout_ms=2500, .buffer_size=1024, .buffer_size_tx=512,
        .crt_bundle_attach=esp_crt_bundle_attach, .user_agent="PocketPassport/0.3", .disable_auto_redirect=true};
    esp_http_client_handle_t http = esp_http_client_init(&cfg);
    char *body = NULL;
    cJSON *json = NULL;
    size_t used = 0;
    int64_t deadline = esp_timer_get_time() + 12000000;
    if (http && esp_http_client_open(http, 0) == ESP_OK) {
        int64_t length = esp_http_client_fetch_headers(http);
        if (length >= 0 && length <= FORM_MAX && esp_http_client_get_status_code(http) == 200) {
            body = malloc(FORM_MAX + 2);
            if (body) {
                while (used <= FORM_MAX && esp_timer_get_time() < deadline && pf_settings_active()) {
                    size_t remaining = FORM_MAX + 1 - used;
                    int n = esp_http_client_read(http, body + used, remaining > 1024 ? 1024 : remaining);
                    if (n <= 0) break;
                    used += n;
                    if (esp_http_client_is_complete_data_received(http)) break;
                }
                body[used] = 0;
                if (used <= FORM_MAX && pf_settings_active() && esp_http_client_is_complete_data_received(http) && pf_json_depth_valid(body, used))
                    json = cJSON_ParseWithLengthOpts(body, used + 1, NULL, true);
            }
        }
    }
    if (http) esp_http_client_cleanup(http);
    pf_http_give();
    valid = playlist_valid(json);
    cJSON_Delete(json);
    esp_err_t err;
    if (valid) {
        httpd_resp_set_type(r, "application/json");
        httpd_resp_set_hdr(r, "Cache-Control", "no-store");
        err = httpd_resp_send(r, body, used);
    } else err = reply(r, "400 Bad Request", "歌单读取失败：请检查网络、校时、直达 JSON 地址及 version: 1、1–8 首曲目，文件最大 7 KB；原配置未修改");
    free(body);
    return err;
}
static esp_err_t wifi_post(httpd_req_t *r)
{
    if (!authorized(r)) return reply(r, "403 Forbidden", "请重新打开设备设置页");
    cJSON *json = body_json(r);
    char ssid[33] = {0}, password[65] = {0};
    bool valid = json && copy_json(json, "ssid", ssid, sizeof(ssid)) && copy_json(json, "password", password, sizeof(password)) && ssid[0] && (strlen(password) == 0 || (strlen(password) >= 8 && strlen(password) <= 63));
    cJSON_Delete(json);
    if (!valid) return reply(r, "400 Bad Request", "Wi-Fi 名称最多 32 字节；密码为空或 8–63 字节");
    wifi_config_t candidate = {0};
    memcpy(candidate.sta.ssid, ssid, strlen(ssid));
    memcpy(candidate.sta.password, password, strlen(password));
    candidate.sta.threshold.authmode = password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    atomic_store(&s_trying, true);
    esp_wifi_disconnect();
    xEventGroupClearBits(s_events, CONNECTED);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &candidate);
    if (err == ESP_OK) err = esp_wifi_connect();
    int64_t deadline = esp_timer_get_time() + 15000000;
    while (err == ESP_OK && atomic_load(&s_settings) && !pf_network_ready() && esp_timer_get_time() < deadline) xEventGroupWaitBits(s_events, CONNECTED, false, false, pdMS_TO_TICKS(200));
    if (err == ESP_OK && pf_network_ready() && atomic_load(&s_settings)) {
        err = nvs_set_blob(s_nvs, "wifi", &candidate, sizeof(candidate));
        if (err == ESP_OK) err = nvs_commit(s_nvs);
        if (err == ESP_OK) { s_saved = candidate; atomic_store(&s_has_credentials, true); }
    } else err = ESP_ERR_TIMEOUT;
    if (err != ESP_OK) {
        esp_wifi_disconnect();
        xEventGroupClearBits(s_events, CONNECTED);
        esp_wifi_set_config(WIFI_IF_STA, &s_saved);
        if (s_saved.sta.ssid[0]) esp_wifi_connect();
    }
    atomic_store(&s_trying, false);
    return reply(r, err == ESP_OK ? "200 OK" : "400 Bad Request", err == ESP_OK ? "联网成功，已保存。热点切换信道时手机可能需要重新连接。" : "连接失败，已恢复原 Wi-Fi；请检查 2.4 GHz 网络及密码");
}
static esp_err_t image_post(httpd_req_t *r)
{
    if (!authorized(r)) return reply(r, "403 Forbidden", "请重新打开设备设置页");
    if (r->content_len < 16 || r->content_len > PF_IMAGE_MAX) return reply(r, "413 Content Too Large", "图片超出设备限制");
    uint8_t header[16], chunk[1024];
    int64_t deadline = esp_timer_get_time() + 120000000;
    pf_image_info_t info;
    if (!receive(r, header, sizeof(header), deadline) || !pf_image_header(header, r->content_len, &info)) return reply(r, "400 Bad Request", "图片格式无效");
    unsigned slot = 1 - pf_image_slot();
    char path[24];
    snprintf(path, sizeof(path), "/data/home%u.pfi", slot);
    FILE *f = fopen(path, "wb");
    if (!f) return reply(r, "500 Internal Server Error", "无法写入图片，原图片仍保留");
    bool ok = fwrite(header, 1, 16, f) == 16;
    size_t remaining = r->content_len - 16;
    uint32_t crc = 0;
    while (remaining && ok) {
        size_t n = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
        ok = receive(r, chunk, n, deadline) && fwrite(chunk, 1, n, f) == n;
        if (ok) { crc = pf_crc32(crc, chunk, n); remaining -= n; }
    }
    if (fclose(f) != 0) ok = false;
    ok = ok && crc == info.crc;
    /* Read back frame delays before publishing the inactive slot. */
    if (ok) {
        f = fopen(path, "rb");
        ok = f != NULL;
        for (unsigned i = 0; i < info.frames && ok; i++) {
            uint8_t delay[2];
            ok = !fseek(f, 16 + i * (2 + PF_FRAME_BYTES), SEEK_SET) && fread(delay, 1, 2, f) == 2 && pf_delay_valid(delay[0] | (uint16_t)delay[1] << 8);
        }
        if (f) fclose(f);
    }
    if (ok) ok = pf_image_activate(slot) == ESP_OK;
    if (!ok) remove(path);
    return reply(r, ok ? "200 OK" : "400 Bad Request", ok ? "首页图片已保存，返回首页即可查看" : "上传未完成或校验失败，原图片仍保留");
}
esp_err_t pf_settings_start(void)
{
    if (s_server) return ESP_OK;
    atomic_store(&s_settings, true);
    if (!pf_http_take(12000)) { atomic_store(&s_settings, false); return ESP_ERR_TIMEOUT; }
    wifi_config_t ap = {0};
    strcpy((char *)ap.ap.ssid, s_ap); strcpy((char *)ap.ap.password, s_password);
    ap.ap.ssid_len = strlen(s_ap); ap.ap.max_connection = 1; ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err != ESP_OK) goto fail;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192; cfg.max_open_sockets = 3; cfg.backlog_conn = 2;
    cfg.lru_purge_enable = true; cfg.recv_wait_timeout = 3; cfg.send_wait_timeout = 5;
    err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) goto fail;
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get},
        {.uri = "/api/config", .method = HTTP_GET, .handler = config_get},
        {.uri = "/api/config", .method = HTTP_POST, .handler = config_post},
        {.uri = "/api/profile", .method = HTTP_POST, .handler = profile_post},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_post},
        {.uri = "/api/image", .method = HTTP_POST, .handler = image_post},
        {.uri = "/api/github", .method = HTTP_POST, .handler = github_post},
        {.uri = "/api/playlist", .method = HTTP_POST, .handler = playlist_post},
    };
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        err = httpd_register_uri_handler(s_server, &routes[i]);
        if (err != ESP_OK) goto fail;
    }
    pf_http_give(); return ESP_OK;
fail:
    atomic_store(&s_settings, false);
    if (s_server) { httpd_stop(s_server); s_server = NULL; }
    esp_wifi_set_mode(WIFI_MODE_STA); pf_http_give(); return err;
}
esp_err_t pf_settings_stop(void)
{
    atomic_store(&s_settings, false);
    if (s_server) {
        esp_err_t err = httpd_stop(s_server);
        if (err != ESP_OK) return err;
        s_server = NULL;
    }
    return esp_wifi_set_mode(WIFI_MODE_STA);
}
