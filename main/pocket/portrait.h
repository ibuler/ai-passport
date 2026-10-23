#pragma once
#include "lvgl.h"
#define PF_PORTRAIT_W 128
#define PF_PORTRAIT_H 128
#define PF_PORTRAIT_COUNT 9
#define PF_PORTRAIT_FRAME_BYTES (64 + PF_PORTRAIT_W * PF_PORTRAIT_H / 2)
extern const lv_image_dsc_t pf_portrait_frames[PF_PORTRAIT_COUNT];
