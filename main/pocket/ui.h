#pragma once
#include "model.h"
#include "xiaozhi_model.h"
/* The controller holds the LVGL lock for every UI call. */
void pf_ui_show(pf_page_t page, const pf_config_t *config, const pf_profile_t *profile, unsigned track, const char *ap, const char *password);
void pf_ui_update(const pf_playback_t *play, bool connected, int battery, uint32_t ticks);
void pf_ui_message(const char *text);
/* Release home image/file and optional heatmap pixels before a page change. */
void pf_ui_image_close(void);
bool pf_ui_font_check(void);
void pf_ui_github(const pf_github_t *github);
void pf_ui_xiaozhi(const pf_xz_snapshot_t *state, uint32_t ticks, uint8_t volume);

void pf_ui_details(const pf_profile_t *profile, const pf_weather_t *weather, int64_t epoch,
                   uint64_t now_ms, const pf_timer_t *focus, uint32_t focus_minutes,
                   const pf_timer_t *stopwatch, unsigned selection);
