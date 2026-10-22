#pragma once
#include "xiaozhi_model.h"
#include "esp_err.h"

esp_err_t pf_xz_init(void);
void pf_xz_enter(void);
bool pf_xz_active(void);
esp_err_t pf_xz_toggle(void);
/* Controller only: wait for cleanup before deleting the page or starting audio. */
esp_err_t pf_xz_stop(void);
void pf_xz_snapshot(pf_xz_snapshot_t *out);
