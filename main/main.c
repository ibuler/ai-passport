#include "pocket/services.h"
#include "pocket/ui.h"
#include "pocket/xiaozhi.h"
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "key_guard.h"
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } input_t;
static QueueHandle_t s_inputs;
static pf_config_t s_config;
static pf_profile_t s_profile;
static unsigned s_radio_revision;
static pf_timer_t s_focus, s_stopwatch;
static unsigned s_minutes = 25, s_selection;
static const pf_page_t s_apps[] = {PF_XIAOZHI, PF_PLAYER, PF_RADIO, PF_WEATHER, PF_FOCUS, PF_STOPWATCH, PF_SETTINGS};
static pf_page_t s_page = PF_HOME;
static unsigned s_track;
static key_guard_t s_key_guard;
static volatile bool s_input_ready;
static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (key_guard_consume(&s_key_guard, event == BSP_BTN_RELEASE, esp_timer_get_time())) return;
    if (!s_input_ready) return;
    if (event != BSP_BTN_CLICK && event != BSP_BTN_LONG) return;
    input_t value = {button, event};
    xQueueSend(s_inputs, &value, 0);
}
static void show(pf_page_t page)
{
    pf_config_get(&s_config);
    pf_profile_get(&s_profile);
    char radio_note[64] = {0};
    if (page == PF_RADIO) s_radio_revision = pf_radio_config(&s_config, s_profile.city, radio_note, sizeof(radio_note));
    if (s_track >= s_config.count) s_track = 0;
    if (bsp_lvgl_lock(1000)) {
        pf_ui_show(page, &s_config, &s_profile, s_track, pf_ap_name(), pf_ap_password());
        if (page == PF_RADIO && !s_config.count) pf_ui_message(!s_profile.city[0] ? "请在后台配置城市" : !pf_network_ready() ? "未联网，请进入手机后台" : radio_note);
        bsp_lvgl_unlock(); s_page = page;
    }
}
static void message(const char *text)
{
    if (bsp_lvgl_lock(1000)) { pf_ui_message(text); bsp_lvgl_unlock(); }
}
static void open_settings(void)
{
    if (pf_player_stop() != ESP_OK) { message("正在停止播放，请重试"); return; }
    if (pf_xz_stop() != ESP_OK) { message("正在停止小智，请重试"); return; }
    if (bsp_lvgl_lock(1000)) { pf_ui_image_close(); bsp_lvgl_unlock(); }
    if (pf_settings_start() == ESP_OK) show(PF_SETTINGS);
    else { show(PF_HOME); message("设置服务启动失败"); }
}
static void handle(input_t value)
{
    if (s_page == PF_SETTINGS) {
        if (value.button == BSP_BTN_OK && value.event == BSP_BTN_LONG) {
            message("正在关闭设置...");
            if (pf_settings_stop() == ESP_OK) { show(PF_LAUNCHER); pf_weather_refresh(); pf_github_refresh(); }
            else message("关闭失败，请重试");
        }
        return;
    }
    if (value.event == BSP_BTN_LONG && (s_page == PF_PLAYER || s_page == PF_RADIO || s_page == PF_XIAOZHI) &&
        (value.button == BSP_BTN_UP || value.button == BSP_BTN_DOWN)) {
        if (pf_volume_adjust(value.button == BSP_BTN_UP ? 1 : -1) != ESP_OK) message("音量保存失败，请重试");
        return;
    }
    if (value.event == BSP_BTN_LONG && value.button == BSP_BTN_DOWN) { open_settings(); return; }
    if (value.event == BSP_BTN_LONG && value.button == BSP_BTN_OK) {
        if (pf_xz_stop() != ESP_OK) { message("正在停止小智，请重试"); return; }
        if (pf_player_stop() != ESP_OK) { message("正在停止播放，请重试"); return; }
        show(s_page == PF_LAUNCHER ? PF_HOME : PF_LAUNCHER); return;
    }
    if (value.event != BSP_BTN_CLICK) return;
    if (s_page == PF_HOME) return;
    if (s_page == PF_LAUNCHER) {
        if (value.button != BSP_BTN_OK) s_selection = pf_track_step(s_selection, value.button == BSP_BTN_UP ? -1 : 1, PF_APP_COUNT);
        else if (s_apps[s_selection] == PF_SETTINGS) open_settings();
        else {
            s_track = 0; show(s_apps[s_selection]);
            if (s_page == PF_XIAOZHI) pf_xz_enter();
            if (s_page == PF_RADIO) pf_radio_refresh();
            if (s_page == PF_WEATHER) pf_weather_refresh();
        }
        return;
    }
    uint64_t now = esp_timer_get_time() / 1000;
    if (s_page == PF_XIAOZHI) {
        if (value.button == BSP_BTN_OK) (void)pf_xz_toggle();
        return;
    }
    if (s_page == PF_FOCUS) {
        if (value.button == BSP_BTN_OK) {
            if (pf_timer_elapsed(&s_focus, now) >= (uint64_t)s_minutes * 60000) pf_timer_reset(&s_focus);
            pf_timer_toggle(&s_focus, now);
        } else if (!s_focus.running) {
            s_minutes = value.button == BSP_BTN_UP ? (s_minutes < 60 ? s_minutes + 5 : 60) : (s_minutes > 5 ? s_minutes - 5 : 5);
            pf_timer_reset(&s_focus);
        }
        return;
    }
    if (s_page == PF_STOPWATCH) {
        if (value.button == BSP_BTN_OK) pf_timer_toggle(&s_stopwatch, now);
        else if (value.button == BSP_BTN_UP) pf_timer_reset(&s_stopwatch);
        return;
    }
    if (s_page == PF_WEATHER) { if (value.button == BSP_BTN_OK) pf_weather_refresh(); return; }
    if (value.button == BSP_BTN_OK) {
        if (s_page == PF_RADIO && !s_config.count) { pf_radio_refresh(); message("正在查找当前城市电台"); return; }
        pf_playback_t state; pf_player_snapshot(&state);
        if (state.state == PF_PLAYING || state.state == PF_PAUSED || state.state == PF_CONNECTING) pf_player_pause();
        else if (s_page == PF_RADIO) pf_player_radio(s_track);
        else pf_player_start(s_track);
    } else if (s_config.count) {
        if (pf_player_stop() != ESP_OK) { message("正在停止播放，请重试"); return; }
        s_track = pf_track_step(s_track, value.button == BSP_BTN_UP ? -1 : 1, s_config.count);
        show(s_page);
        if (s_page == PF_RADIO) pf_player_radio(s_track); else pf_player_start(s_track);
    }
}
void app_main(void)
{
    ESP_LOGI("pocket", "Pocket Passport 0.3.1 / ESP32-C3");
    ESP_ERROR_CHECK(pf_store_init());
    ESP_ERROR_CHECK(bsp_i2c_init());
    ESP_ERROR_CHECK(bsp_display_init());
    if (!bsp_lvgl_init()) { ESP_LOGE("pocket", "LVGL initialization failed"); return; }
    bsp_display_backlight(75);
    (void)bsp_battery_init();
    ESP_ERROR_CHECK(pf_network_init());
    ESP_ERROR_CHECK(pf_player_init());
    ESP_ERROR_CHECK(pf_xz_init());
    ESP_ERROR_CHECK(pf_weather_init());
    s_inputs = xQueueCreate(8, sizeof(input_t));
    if (!s_inputs) { ESP_LOGE("pocket", "input queue allocation failed"); return; }
    if (bsp_lvgl_lock(1000)) {
        bool fonts_ok = pf_ui_font_check(); bsp_lvgl_unlock();
        if (!fonts_ok) { ESP_LOGE("pocket", "CJK glyph coverage probe failed"); return; }
    }
    show(PF_HOME);
    key_guard_arm(&s_key_guard, esp_timer_get_time(), 10000000);
    ESP_ERROR_CHECK(bsp_button_init(on_key, NULL));
    key_guard_sync_released(&s_key_guard, bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN);
    s_input_ready = true;
    uint32_t battery_at = 0; int battery = -1;
    for (;;) {
        input_t value;
        if (xQueueReceive(s_inputs, &value, pdMS_TO_TICKS(100)) == pdTRUE) handle(value);
        uint64_t now = esp_timer_get_time() / 1000;
        uint32_t ticks = (uint32_t)now;
        if (s_focus.running && pf_timer_elapsed(&s_focus, now) >= (uint64_t)s_minutes * 60000) {
            pf_timer_toggle(&s_focus, now); s_focus.elapsed_ms = (uint64_t)s_minutes * 60000;
        }
        if (s_page == PF_RADIO && pf_radio_revision() != s_radio_revision) show(PF_RADIO);
        pf_weather_t weather; pf_weather_snapshot(&weather);
        pf_github_t github; pf_github_snapshot(&github);
        if (!battery_at || (uint32_t)(ticks - battery_at) >= 30000) { battery_at = ticks; battery = bsp_battery_soc(); }
        pf_playback_t play; pf_player_snapshot(&play);
        pf_xz_snapshot_t xiaozhi; pf_xz_snapshot(&xiaozhi);
        unsigned next;
        if (pf_playlist_advance(s_page, &play, s_config.count, &next)) {
            if (pf_player_stop() == ESP_OK) {
                ESP_LOGI("pocket", "Playlist advance: %u -> %u", play.track, next);
                s_track = next;
                show(PF_PLAYER);
                pf_player_start(s_track);
            } else message("正在停止播放，请重试");
            pf_player_snapshot(&play);
        }
        if (bsp_lvgl_lock(100)) {
            pf_ui_update(&play, pf_network_ready(), battery, ticks);
            if (s_page == PF_XIAOZHI) pf_ui_xiaozhi(&xiaozhi, ticks, pf_volume_get());
            pf_ui_details(&s_profile, &weather, time(NULL), now, &s_focus, s_minutes, &s_stopwatch, s_selection);
            pf_ui_github(&github);
            bsp_lvgl_unlock();
        }
    }
}
