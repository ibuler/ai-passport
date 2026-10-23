#include "portrait.h"
#if LV_BIN_DECODER_RAM_LOAD
#error "Portraits require line decoding to avoid full-frame ARGB allocations"
#endif
#ifdef PF_HOST_RENDER
extern const unsigned char pf_portrait_data[];
#else
extern const unsigned char pf_portrait_data[] asm("_binary_huolinger_i4_start");
#endif
/* Flash-backed BGRA palette + I4 pixels; LVGL decodes one 512-byte line. */
#define FRAME(n) { .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I4, \
    .w=PF_PORTRAIT_W,.h=PF_PORTRAIT_H,.stride=PF_PORTRAIT_W/2}, \
    .data_size=PF_PORTRAIT_FRAME_BYTES, .data=pf_portrait_data+(n)*PF_PORTRAIT_FRAME_BYTES }
const lv_image_dsc_t pf_portrait_frames[PF_PORTRAIT_COUNT]={FRAME(0),FRAME(1),FRAME(2),FRAME(3),FRAME(4),FRAME(5),FRAME(6),FRAME(7),FRAME(8)};
