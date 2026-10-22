#include "furina.h"
#ifdef PF_HOST_RENDER
extern const unsigned char pf_furina_data[];
#else
extern const unsigned char pf_furina_data[] asm("_binary_furina_rgb565a8_start");
#endif
/* Direct Flash-backed RGB565A8 images: no full-frame allocation or decompression. */
#define FRAME(n) { .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565A8, \
    .w=128,.h=128,.stride=256}, .data_size=49152, .data=pf_furina_data+(n)*49152 }
const lv_image_dsc_t pf_furina_frames[9]={FRAME(0),FRAME(1),FRAME(2),FRAME(3),FRAME(4),FRAME(5),FRAME(6),FRAME(7),FRAME(8)};
