#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PF_TRACKS 8
#define PF_TITLE 96
#define PF_URL 512
#define PF_IMAGE_W 96
#define PF_IMAGE_H 96
#define PF_IMAGE_FRAMES 48
#define PF_FRAME_BYTES (PF_IMAGE_W * PF_IMAGE_H * 2)
#define PF_IMAGE_MAX (16 + PF_IMAGE_FRAMES * (2 + PF_FRAME_BYTES))

typedef struct { char title[PF_TITLE]; char url[PF_URL]; } pf_track_t;
typedef struct {
    uint32_t version;
    char name[PF_TITLE];
    uint8_t volume;
    uint8_t count;
    pf_track_t tracks[PF_TRACKS];
} pf_config_t;
typedef struct { uint16_t frames, flags; uint32_t crc; } pf_image_info_t;
typedef enum { PF_HOME, PF_LAUNCHER, PF_PLAYER, PF_RADIO, PF_WEATHER, PF_FOCUS, PF_STOPWATCH, PF_SETTINGS } pf_page_t;
typedef enum { PF_IDLE, PF_CONNECTING, PF_PLAYING, PF_PAUSED, PF_FINISHED, PF_ERROR } pf_play_state_t;
typedef struct {
    pf_play_state_t state;
    uint32_t seconds;
    unsigned track;
    uint8_t volume;
    char error[64];
} pf_playback_t;

void pf_defaults(pf_config_t *config);
bool pf_text_valid(const char *text, size_t capacity, bool allow_empty);
bool pf_url_valid(const char *url);
bool pf_config_valid(const pf_config_t *config);
unsigned pf_track_step(unsigned current, int direction, unsigned count);
uint8_t pf_volume_step(uint8_t current, int direction);
uint32_t pf_crc32(uint32_t crc, const void *data, size_t length);
bool pf_image_header(const uint8_t header[16], size_t bytes, pf_image_info_t *info);
bool pf_delay_valid(uint16_t milliseconds);

#define PF_RADIOS 6
#define PF_CITY 64
typedef struct { char city[PF_CITY]; pf_track_t track; } pf_radio_t;
typedef struct {
    uint32_t version;
    char company[PF_TITLE];
    char city[PF_CITY];
    int32_t latitude_e4, longitude_e4;
    int16_t utc_minutes;
    uint8_t radio_count;
    pf_radio_t radios[PF_RADIOS];
} pf_profile_t;
typedef struct { uint64_t elapsed_ms, started_ms; bool running; } pf_timer_t;
typedef struct {
    bool valid, stale, loading;
    int temperature_tenths, code, humidity, wind_tenths;
    char city[PF_CITY], message[64];
    uint64_t updated_ms;
} pf_weather_t;
void pf_profile_defaults(pf_profile_t *profile);
bool pf_profile_valid(const pf_profile_t *profile);
unsigned pf_radio_step(const pf_profile_t *profile, unsigned current, int direction);
unsigned pf_radio_count(const pf_profile_t *profile);
uint64_t pf_timer_elapsed(const pf_timer_t *timer, uint64_t now_ms);
void pf_timer_toggle(pf_timer_t *timer, uint64_t now_ms);
void pf_timer_reset(pf_timer_t *timer);
const char *pf_weather_description(int code);

typedef struct {
    unsigned revision, count;
    char city[PF_CITY], message[64];
    pf_track_t tracks[PF_RADIOS];
} pf_catalog_t;

bool pf_json_depth_valid(const char *body, size_t bytes);

#define PF_GITHUB_USER 40
#define PF_GITHUB_WEEKS 26
#define PF_GITHUB_DAYS (PF_GITHUB_WEEKS * 7)
typedef struct {
    char username[PF_GITHUB_USER];
    bool valid, stale, loading;
    uint8_t levels[PF_GITHUB_DAYS]; /* Sunday first; 255 means outside the date range. */
    int64_t first_day;
    uint64_t updated_ms;
    char message[64];
} pf_github_t;
bool pf_github_username_valid(const char *username);
typedef enum { PF_ICON_UNKNOWN, PF_ICON_SUN, PF_ICON_PARTLY, PF_ICON_CLOUD,
               PF_ICON_FOG, PF_ICON_RAIN, PF_ICON_SNOW, PF_ICON_THUNDER } pf_weather_icon_t;
pf_weather_icon_t pf_weather_icon(int code);
