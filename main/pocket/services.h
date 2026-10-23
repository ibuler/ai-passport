#pragma once
#include "model.h"
#include "esp_err.h"

esp_err_t pf_store_init(void);
void pf_config_get(pf_config_t *out);
bool pf_music_url(unsigned track, char out[PF_URL]);
esp_err_t pf_config_save(const pf_config_t *config);
uint8_t pf_volume_get(void);
esp_err_t pf_volume_adjust(int direction);
const char *pf_image_path(void);
esp_err_t pf_image_activate(unsigned slot);
unsigned pf_image_slot(void);

esp_err_t pf_network_init(void);
bool pf_network_ready(void);
esp_err_t pf_settings_start(void);
esp_err_t pf_settings_stop(void);
const char *pf_ap_name(void);
const char *pf_ap_password(void);

esp_err_t pf_player_init(void);
/* Controller-only, blocking: start pauses feeds (up to their stop timeout)
 * before allocating playback tasks/FIFO; stop joins both tasks and releases
 * audio before resuming feeds. Retry a failed stop before opening another app.
 * Natural EOF retains the player/feed pause until explicit stop or advance. */
esp_err_t pf_player_start(unsigned track);
esp_err_t pf_player_stop(void);
void pf_player_pause(void);
void pf_player_snapshot(pf_playback_t *out);

void pf_profile_get(pf_profile_t *out);
esp_err_t pf_profile_save(const pf_profile_t *profile);
esp_err_t pf_player_radio(unsigned station);
bool pf_http_take(uint32_t wait_ms);
void pf_http_give(void);
bool pf_settings_active(void);
esp_err_t pf_weather_init(void);
/* Controller-only: cooperative stop/recreate; cached results and requests survive. */
esp_err_t pf_feeds_pause(void);
esp_err_t pf_feeds_resume(void);
void pf_weather_refresh(void);
void pf_weather_snapshot(pf_weather_t *out);

void pf_radio_refresh(void);
void pf_radio_catalog(pf_catalog_t *out);
bool pf_radio_track(unsigned index, pf_track_t *out);

unsigned pf_radio_revision(void);
unsigned pf_radio_config(pf_config_t *out, const char *city, char *note, size_t cap);
void pf_github_username_get(char out[PF_GITHUB_USER]);
esp_err_t pf_github_username_save(const char *username);
void pf_github_refresh(void);
void pf_github_snapshot(pf_github_t *out);
