#include "services.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static pf_config_t s_config;
static pf_profile_t s_profile;
static char s_github_username[PF_GITHUB_USER] = "ibuler";
static SemaphoreHandle_t s_lock;
static nvs_handle_t s_nvs;
static unsigned s_slot;
static const char *s_paths[] = {"/data/home0.pfi", "/data/home1.pfi"};

esp_err_t pf_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err; /* Never erase unrelated settings to hide a failure. */
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    err = nvs_open("pocket_focus", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) return err;
    pf_defaults(&s_config);
    pf_config_t *saved = malloc(sizeof(*saved));
    if (!saved) return ESP_ERR_NO_MEM;
    size_t size = sizeof(*saved);
    if (nvs_get_blob(s_nvs, "config", saved, &size) == ESP_OK && size == sizeof(*saved) && pf_config_valid(saved)) s_config = *saved;
    free(saved);
    uint8_t volume;
    if (nvs_get_u8(s_nvs, "volume", &volume) == ESP_OK && volume <= 100) s_config.volume = volume;
    pf_profile_defaults(&s_profile);
    pf_profile_t *profile = malloc(sizeof(*profile));
    if (!profile) return ESP_ERR_NO_MEM;
    size = sizeof(*profile);
    if (nvs_get_blob(s_nvs, "profile", profile, &size) == ESP_OK && size == sizeof(*profile) && pf_profile_valid(profile)) s_profile = *profile;
    free(profile);
    char username[PF_GITHUB_USER];
    size = sizeof(username);
    if (nvs_get_str(s_nvs, "github_id", username, &size) == ESP_OK && pf_github_username_valid(username)) strcpy(s_github_username, username);
    uint8_t slot = 0;
    if (nvs_get_u8(s_nvs, "image_slot", &slot) == ESP_OK && slot < 2) s_slot = slot;
    const esp_vfs_spiffs_conf_t fs = {
        .base_path = "/data", .partition_label = "storage", .max_files = 3,
        .format_if_mount_failed = false,
    };
    return esp_vfs_spiffs_register(&fs);
}

void pf_config_get(pf_config_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_config;
    xSemaphoreGive(s_lock);
}
esp_err_t pf_config_save(const pf_config_t *config)
{
    if (!pf_config_valid(config)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = nvs_set_blob(s_nvs, "config", config, sizeof(*config));
    if (err == ESP_OK) err = nvs_set_u8(s_nvs, "volume", config->volume);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err == ESP_OK) s_config = *config;
    xSemaphoreGive(s_lock);
    return err;
}
uint8_t pf_volume_get(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint8_t volume = s_config.volume;
    xSemaphoreGive(s_lock);
    return volume;
}
esp_err_t pf_volume_adjust(int direction)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint8_t volume = pf_volume_step(s_config.volume, direction);
    esp_err_t err = ESP_OK;
    if (volume != s_config.volume) {
        /* Keep playback adjustments small: do not copy/rewrite the playlist blob. */
        err = nvs_set_u8(s_nvs, "volume", volume);
        if (err == ESP_OK) err = nvs_commit(s_nvs);
        if (err == ESP_OK) s_config.volume = volume;
    }
    xSemaphoreGive(s_lock);
    return err;
}
const char *pf_image_path(void) { return s_paths[s_slot]; }
unsigned pf_image_slot(void) { return s_slot; }
esp_err_t pf_image_activate(unsigned slot)
{
    if (slot > 1) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = nvs_set_u8(s_nvs, "image_slot", slot);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err == ESP_OK) s_slot = slot;
    xSemaphoreGive(s_lock);
    return err;
}

void pf_profile_get(pf_profile_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_profile; xSemaphoreGive(s_lock);
}
esp_err_t pf_profile_save(const pf_profile_t *profile)
{
    if (!pf_profile_valid(profile)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = nvs_set_blob(s_nvs, "profile", profile, sizeof(*profile));
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err == ESP_OK) s_profile = *profile;
    xSemaphoreGive(s_lock);
    return err;
}
void pf_github_username_get(char out[PF_GITHUB_USER])
{
    xSemaphoreTake(s_lock, portMAX_DELAY); strcpy(out, s_github_username); xSemaphoreGive(s_lock);
}
esp_err_t pf_github_username_save(const char *username)
{
    if (!pf_github_username_valid(username)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = nvs_set_str(s_nvs, "github_id", username);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err == ESP_OK) strcpy(s_github_username, username);
    xSemaphoreGive(s_lock); return err;
}
