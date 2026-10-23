#include "ui.h"
#include "github.h"
#include "portrait.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifndef PF_HOST_RENDER
#include "services.h"
#endif
LV_FONT_DECLARE(pocket_font_16);
static lv_font_t s_font;
static lv_obj_t *s_root, *s_status, *s_time, *s_battery, *s_indicator, *s_art;
static lv_obj_t *s_bars[17];
static pf_page_t s_page;
static bool s_radio_empty, s_connected;
static lv_obj_t *s_date, *s_city, *s_weather, *s_aux, *s_menu[PF_APP_COUNT], *s_menu_bg[PF_APP_COUNT];
static lv_obj_t *s_portrait, *s_subtitle;
static unsigned s_portrait_frame;
static lv_obj_t *s_weather_icon, *s_git_chart;
static pf_weather_icon_t s_icon = PF_ICON_UNKNOWN;
static pf_github_t s_git_cached;
static bool s_git_drawn;
#define CHART_W (PF_GITHUB_WEEKS * 8 - 1)
#define CHART_H 55
#define CHART_STRIDE 104
/* Indexed RGB palette + 4-bit pixels: 5.7 KiB, one LVGL image object. */
#define CHART_BYTES (64 + CHART_STRIDE * CHART_H)
/* Own pixels only while home is visible; voice/audio reclaim the whole block. */
static uint8_t *s_git_pixels;
static lv_image_dsc_t s_git_dsc = {
    .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_I4, .w=CHART_W, .h=CHART_H, .stride=CHART_STRIDE},
    .data_size=CHART_BYTES, .data=NULL,
};
static FILE *s_image;
static uint8_t *s_pixels;
static pf_image_info_t s_info;
static lv_image_dsc_t s_dsc;
static unsigned s_frame;
static uint32_t s_frame_at;
static uint16_t s_delay;
#define BG 0x09151C
#define INK 0xE2EBE6
#define MUTED 0x88A6A8
#define ACCENT 0x87E3BE
static lv_obj_t *text(const char *value, int x, int y, int w, uint32_t color, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(s_root);
    lv_obj_set_pos(label, x, y); lv_obj_set_width(label, w);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, font ? font : &s_font, 0);
    lv_label_set_text(label, value);
    return label;
}
static lv_obj_t *block(int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(s_root);
    lv_obj_remove_style_all(obj); lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}
static void shape(lv_layer_t *layer, int x, int y, int w, int h, int radius, uint32_t color)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(color); d.bg_opa = LV_OPA_COVER; d.radius = radius;
    lv_area_t a = {x,y,x+w-1,y+h-1}; lv_draw_rect(layer, &d, &a);
}
static void stroke(lv_layer_t *layer, int x, int y, int x2, int y2, uint32_t color, int width)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.p1 = (lv_point_precise_t){x,y}; d.p2 = (lv_point_precise_t){x2,y2};
    d.color = lv_color_hex(color); d.width = width; d.round_start = d.round_end = true;
    lv_draw_line(layer, &d);
}
static void weather_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e); lv_area_t a; lv_obj_get_coords(obj, &a);
    lv_layer_t *layer = lv_event_get_layer(e);
    int x = a.x1, y = a.y1;
    const uint32_t sun = 0xF4D781, cloud = 0xB5D1DD, rain = 0x73B6DE;
    if (s_icon == PF_ICON_UNKNOWN) { stroke(layer,x+10,y+16,x+22,y+16,MUTED,2); return; }
    if (s_icon == PF_ICON_SUN || s_icon == PF_ICON_PARTLY) {
        int cx = x + (s_icon == PF_ICON_SUN ? 16 : 11), cy = y + (s_icon == PF_ICON_SUN ? 16 : 10);
        shape(layer,cx-5,cy-5,11,11,LV_RADIUS_CIRCLE,sun);
        static const int8_t rays[][4] = {{0,-9,0,-12},{6,-6,8,-8},{9,0,12,0},{6,6,8,8},{0,9,0,12},{-6,6,-8,8},{-9,0,-12,0},{-6,-6,-8,-8}};
        for (unsigned i=0;i<8;i++) stroke(layer,cx+rays[i][0],cy+rays[i][1],cx+rays[i][2],cy+rays[i][3],sun,2);
        if (s_icon == PF_ICON_SUN) return;
    }
    shape(layer,x+3,y+12,12,12,LV_RADIUS_CIRCLE,cloud);
    shape(layer,x+10,y+7,15,16,LV_RADIUS_CIRCLE,cloud);
    shape(layer,x+20,y+13,10,10,LV_RADIUS_CIRCLE,cloud);
    shape(layer,x+7,y+17,19,7,3,cloud);
    if (s_icon == PF_ICON_RAIN) for (int i=0;i<3;i++) stroke(layer,x+9+i*7,y+27,x+7+i*7,y+31,rain,2);
    if (s_icon == PF_ICON_THUNDER) {
        stroke(layer,x+18,y+23,x+13,y+27,sun,3); stroke(layer,x+13,y+27,x+19,y+27,sun,3); stroke(layer,x+19,y+27,x+14,y+31,sun,3);
    }
    if (s_icon == PF_ICON_SNOW) for (int i=0;i<3;i++) {
        int cx=x+9+i*7; stroke(layer,cx-2,y+28,cx+2,y+28,INK,1); stroke(layer,cx,y+26,cx,y+30,INK,1);
    }
    if (s_icon == PF_ICON_FOG) { stroke(layer,x+4,y+26,x+26,y+26,MUTED,1); stroke(layer,x+7,y+30,x+29,y+30,MUTED,1); }
}
static void weather_icon_create(int x, int y)
{
    s_weather_icon = lv_obj_create(s_root); lv_obj_remove_style_all(s_weather_icon);
    lv_obj_set_pos(s_weather_icon,x,y); lv_obj_set_size(s_weather_icon,32,32);
    lv_obj_remove_flag(s_weather_icon,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_weather_icon,weather_draw,LV_EVENT_DRAW_MAIN,NULL);
}
static void weather_icon_update(bool valid, int code)
{
    pf_weather_icon_t icon = valid ? pf_weather_icon(code) : PF_ICON_UNKNOWN;
    if (s_weather_icon && icon != s_icon) { s_icon = icon; lv_obj_invalidate(s_weather_icon); }
}
/* Unknown dynamic glyphs are represented deliberately, rather than disappearing. */
static void supported(char *out, size_t cap, const char *input)
{
    size_t used = 0;
    while (*input && used + 5 < cap) {
        const char *start = input;
        uint8_t c = (uint8_t)*input++;
        uint32_t cp = c;
        unsigned n = 0;
        if (c >= 0xc0 && c < 0xe0) { cp = c & 31; n = 1; }
        else if (c >= 0xe0 && c < 0xf0) { cp = c & 15; n = 2; }
        else if (c >= 0xf0) { cp = c & 7; n = 3; }
        while (n-- && *input) cp = (cp << 6) | ((uint8_t)*input++ & 63);
        lv_font_glyph_dsc_t glyph = {0};
        if (lv_font_get_glyph_dsc(&s_font, &glyph, cp, 0) && !glyph.is_placeholder) {
            size_t len = input - start; memcpy(out + used, start, len); used += len;
        } else out[used++] = '?';
    }
    out[used] = 0;
}
void pf_ui_image_close(void)
{
    if (s_image) fclose(s_image);
    s_image = NULL;
    if (s_art) { lv_obj_delete(s_art); s_art = NULL; }
    free(s_pixels); s_pixels = NULL;
    /* The caller holds the LVGL lock: delete all users before freeing pixels. */
    if (s_git_chart) { lv_obj_delete(s_git_chart); s_git_chart = NULL; }
    lv_image_cache_drop(&s_git_dsc);
    free(s_git_pixels); s_git_pixels = NULL; s_git_dsc.data = NULL;
    s_git_drawn = false;
}
static bool next_frame(void)
{
    uint8_t d[2];
    if (!s_image || fread(d, 1, 2, s_image) != 2 || !pf_delay_valid(d[0] | (uint16_t)d[1] << 8) || fread(s_pixels, 1, PF_FRAME_BYTES, s_image) != PF_FRAME_BYTES) return false;
    s_delay = d[0] | (uint16_t)d[1] << 8;
    return true;
}
static bool image_open(void)
{
#ifdef PF_HOST_RENDER
    return false;
#else
    struct stat st;
    const char *path = pf_image_path();
    if (stat(path, &st) != 0) return false;
    s_image = fopen(path, "rb");
    uint8_t header[16];
    if (!s_image || fread(header, 1, 16, s_image) != 16 || !pf_image_header(header, st.st_size, &s_info)) goto fail;
    uint8_t chunk[512]; uint32_t crc = 0; size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), s_image))) crc = pf_crc32(crc, chunk, n);
    if (crc != s_info.crc || ferror(s_image) || fseek(s_image, 16, SEEK_SET)) goto fail;
    s_pixels = malloc(PF_FRAME_BYTES);
    if (!s_pixels || !next_frame()) goto fail;
    s_frame = 0; s_frame_at = 0;
    s_dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = PF_IMAGE_W, .h = PF_IMAGE_H, .stride = PF_IMAGE_W * 2}, .data_size = PF_FRAME_BYTES, .data = s_pixels};
    s_art = lv_image_create(s_root); lv_image_set_src(s_art, &s_dsc); lv_obj_set_pos(s_art, 18, 58);
    return true;
fail:
    pf_ui_image_close(); return false;
#endif
}
static void art_default(void)
{
    for (int i = 0; i < 17; i++) {
        int h = 8 + ((i * i * 7 + 19) % (s_page == PF_PLAYER ? 43 : 67));
        s_bars[i] = block(37 + i * 10, 176 - h, 5, h, i % 3 ? ACCENT : 0x529CBA);
        lv_obj_set_style_radius(s_bars[i], 2, 0);
    }
}
void pf_ui_show(pf_page_t page, const pf_config_t *c, const pf_profile_t *profile, unsigned track, const char *ap, const char *password)
{
    pf_ui_image_close();
    lv_obj_t *old = s_root;
    if (old) lv_obj_clean(old);
    s_root = lv_obj_create(NULL); lv_obj_remove_style_all(s_root);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    memset(s_bars, 0, sizeof(s_bars)); memset(s_menu, 0, sizeof(s_menu));
    s_font = pocket_font_16; s_font.fallback = &lv_font_montserrat_14;
    s_page = page; s_radio_empty = page == PF_RADIO && !c->count;
    static const char *headers[] = {"PASSPORT", "YOUR APPS", "MUSIC", "CITY RADIO", "WEATHER", "FOCUS TIMER", "STOPWATCH", "SETTINGS", "XIAOZHI"};
    text(headers[page], 23, 18, 154, MUTED, &lv_font_montserrat_14);
    s_battery = text("--%", 184, 18, 40, MUTED, &lv_font_montserrat_14);
    block(18, 45, 204, 1, 0x294047);
    s_status = NULL; s_time = NULL; s_indicator = NULL; s_date = NULL; s_city = NULL; s_weather = NULL; s_aux = NULL;
    s_portrait = NULL; s_subtitle = NULL; s_portrait_frame = 0;
    s_weather_icon = NULL; s_icon = PF_ICON_UNKNOWN;
    s_git_chart = NULL; s_git_drawn = false;
    char safe[PF_TITLE + 4];
    if (page == PF_HOME) {
        if (!image_open()) {
            text("名片图", 40, 88, 72, ACCENT, NULL);
            text("后台上传", 34, 115, 80, MUTED, NULL);
        }
        supported(safe, sizeof(safe), c->name);
        lv_obj_t *name = text(safe, 18, 160, 96, INK, NULL);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0); lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
        supported(safe, sizeof(safe), profile->company[0] ? profile->company : "设置公司名称");
        lv_obj_t *company = text(safe, 18, 184, 96, MUTED, NULL);
        lv_obj_set_style_text_align(company, LV_TEXT_ALIGN_CENTER, 0); lv_label_set_long_mode(company, LV_LABEL_LONG_MODE_DOTS);
        s_time = text("--:--", 126, 65, 96, ACCENT, &lv_font_montserrat_28);
        s_date = text("等待校时", 126, 106, 96, MUTED, NULL);
        s_city = text("设置城市", 126, 133, 96, INK, NULL); lv_label_set_long_mode(s_city, LV_LABEL_LONG_MODE_DOTS);
        block(126, 159, 96, 1, 0x294047);
        weather_icon_create(123,162);
        s_weather = text("-- C", 158, 167, 64, ACCENT, NULL);
        s_aux = text("等待天气", 126, 197, 96, MUTED, NULL); lv_obj_set_height(s_aux, 46);
        lv_obj_set_y(s_aux,194); lv_obj_set_height(s_aux,20);
        block(18, 216, 204, 1, 0x294047);
        s_git_pixels = calloc(1, CHART_BYTES);
        if (s_git_pixels) {
            s_git_dsc.data = s_git_pixels;
            s_git_chart = lv_image_create(s_root); lv_image_set_src(s_git_chart,&s_git_dsc); lv_obj_set_pos(s_git_chart,16,225);
        }
        block(18, 286, 204, 1, 0x294047);
        text("长按 确定:应用 下:后台", 24, 292, 192, MUTED, NULL);
    } else if (page == PF_LAUNCHER) {
        static const char *apps[] = {"小智", "音乐", "城市收音机", "天气", "番茄钟", "秒表", "手机后台"};
        for (unsigned i = 0; i < PF_APP_COUNT; i++) {
            s_menu_bg[i] = block(18, 57 + i * 29, 204, 27, BG); lv_obj_set_style_radius(s_menu_bg[i], 7, 0);
            s_menu[i] = text(apps[i], 31, 61 + i * 29, 180, INK, NULL);
        }
        text("上 / 下选择  确定打开", 24, 275, 192, INK, NULL);
        text("长按确定  返回名片", 24, 297, 192, MUTED, NULL);
    } else if (page == PF_XIAOZHI) {
        text("小智", 24, 57, 95, INK, NULL);
        s_aux = text("音量 35%", 130, 57, 86, MUTED, NULL);
        s_portrait = lv_image_create(s_root);
        lv_image_set_src(s_portrait, &pf_portrait_frames[0]);
        lv_obj_align(s_portrait, LV_ALIGN_TOP_MID, 0, 102);
        s_status = text("", 24, 80, 192, ACCENT, NULL);
        lv_label_set_long_mode(s_status, LV_LABEL_LONG_MODE_DOTS);
        s_subtitle = text("", 24, 239, 192, INK, NULL);
        lv_obj_set_height(s_subtitle, 40); lv_label_set_long_mode(s_subtitle, LV_LABEL_LONG_MODE_DOTS);
        text("确定:开启 / 关闭对话", 24, 281, 192, MUTED, NULL);
        text("长按:上/下音量 确定返回", 24, 301, 192, MUTED, NULL);
    } else if (page == PF_PLAYER || page == PF_RADIO) {
        supported(safe, sizeof(safe), page == PF_RADIO ? (profile->city[0] ? profile->city : "请先设置城市") : "给自己一段专注时间");
        text(safe, 24, 58, 192, MUTED, NULL);
        text(page == PF_RADIO ? "ON AIR" : "LISTEN", 24, 86, 192, ACCENT, &lv_font_montserrat_28);
        char volume[24]; snprintf(volume, sizeof(volume), "音量 %u%%", c->volume);
        s_aux = text(volume, 136, 102, 80, MUTED, NULL);
        art_default();
        supported(safe, sizeof(safe), track < c->count ? c->tracks[track].title : page == PF_RADIO ? "正在查找城市电台" : "请添加音乐来源");
        lv_obj_t *title = text(safe, 24, 184, 192, INK, NULL); lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
        s_time = text("00:00", 24, 208, 90, ACCENT, &lv_font_montserrat_20);
        s_indicator = text("", 119, 212, 97, MUTED, NULL); lv_obj_set_style_text_align(s_indicator, LV_TEXT_ALIGN_RIGHT, 0);
        s_status = text("确定键开始播放", 24, 234, 192, MUTED, NULL); lv_label_set_long_mode(s_status, LV_LABEL_LONG_MODE_DOTS);
        text("上 / 下切换  确定播放", 24, 257, 192, INK, NULL);
        text("长按上/下:音量+/-", 24, 278, 192, MUTED, NULL);
        text("长按确定  返回应用", 24, 299, 192, MUTED, NULL);
    } else if (page == PF_WEATHER) {
        supported(safe, sizeof(safe), profile->city[0] ? profile->city : "请在后台设置城市"); text(safe, 24, 62, 192, INK, NULL);
        s_time = text("-- C", 24, 105, 192, ACCENT, &lv_font_montserrat_32);
        s_weather = text("等待天气", 24, 155, 192, INK, NULL);
        lv_obj_set_width(s_weather,148); weather_icon_create(184,148);
        s_aux = text("", 24, 190, 192, MUTED, NULL);
        s_status = text("", 24, 237, 192, MUTED, NULL); lv_label_set_long_mode(s_status, LV_LABEL_LONG_MODE_DOTS);
        text("Open-Meteo / CC BY 4.0", 24, 265, 192, MUTED, &lv_font_montserrat_14);
        text("确定刷新  长按确定返回", 24, 286, 192, INK, NULL);
    } else if (page == PF_FOCUS || page == PF_STOPWATCH) {
        text(page == PF_FOCUS ? "番茄钟" : "秒表", 24, 64, 192, INK, NULL);
        text(page == PF_FOCUS ? "一次只做一件事" : "记录此刻的每一秒", 24, 98, 192, MUTED, NULL);
        s_time = text("00:00", 24, 150, 192, ACCENT, &lv_font_montserrat_32);
        s_status = text("确定开始", 24, 207, 192, INK, NULL);
        text(page == PF_FOCUS ? "暂停时上 / 下调整时长" : "上键清零  确定开始暂停", 24, 254, 192, MUTED, NULL);
        text("长按确定  返回应用", 24, 286, 192, INK, NULL);
    } else {
        text("手机后台", 24, 62, 192, INK, NULL);
        text("1  连接设备 Wi-Fi", 24, 101, 192, MUTED, NULL);
        text(ap, 24, 127, 198, ACCENT, &lv_font_montserrat_14);
        char pw[48]; snprintf(pw, sizeof(pw), "密码  %s", password);
        text(pw, 24, 149, 192, INK, NULL);
        text("2  浏览器打开", 24, 188, 192, MUTED, NULL);
        text("192.168.4.1", 24, 213, 192, ACCENT, &lv_font_montserrat_20);
        s_status = text("名片 / 城市 / 音乐 / 电台", 24, 252, 192, MUTED, NULL); lv_label_set_long_mode(s_status, LV_LABEL_LONG_MODE_DOTS);
        text("长按确定  返回应用", 24, 286, 192, INK, NULL);
    }
    lv_screen_load(s_root); if (old) lv_obj_delete(old);
}
void pf_ui_message(const char *message)
{
    if (s_status) { char safe[49];pf_notice_text(safe,sizeof(safe),message);lv_label_set_text(s_status,safe); }
}
void pf_ui_xiaozhi(const pf_xz_snapshot_t *state, uint32_t ticks, uint8_t volume)
{
    if(s_page != PF_XIAOZHI || !s_portrait)return;
    unsigned frame = pf_xz_portrait(state, ticks);
    if(frame != s_portrait_frame) { lv_image_set_src(s_portrait, &pf_portrait_frames[frame]); s_portrait_frame = frame; }
    char safe[256];
    if(state->state==PF_XZ_ERROR)pf_notice_text(safe,sizeof(safe),state->message);
    else supported(safe,sizeof(safe),state->message);
    lv_obj_set_y(s_status,state->state==PF_XZ_ERROR?236:80);
    lv_label_set_text(s_status,safe);
    supported(safe,sizeof(safe),state->state==PF_XZ_ERROR?"":state->code[0] ? state->code : state->text); lv_label_set_text(s_subtitle,safe);
    lv_label_set_text_fmt(s_aux,"音量 %u%%",volume);
}
void pf_ui_update(const pf_playback_t *p, bool connected, int battery, uint32_t ticks)
{
    s_connected = connected;
    if (battery >= 0) lv_label_set_text_fmt(s_battery, "%d%%", battery); else lv_label_set_text(s_battery, "--%");
    if (s_page == PF_HOME) {
        if (s_image && s_info.frames > 1 && (uint32_t)(ticks - s_frame_at) >= s_delay) {
            s_frame_at = ticks; s_frame = (s_frame + 1) % s_info.frames;
            if (!s_frame) fseek(s_image, 16, SEEK_SET);
            if (!next_frame()) pf_ui_image_close();
            else { lv_image_cache_drop(&s_dsc); lv_obj_invalidate(s_art); }
        }
    } else if (s_page == PF_PLAYER || s_page == PF_RADIO) {
        static const char *states[] = {"待播放", "连接中", "播放中", "已暂停", "已结束", "播放失败"};
        lv_label_set_text_fmt(s_time, "%02lu:%02lu", (unsigned long)(p->seconds / 60), (unsigned long)(p->seconds % 60));
        lv_label_set_text(s_indicator, states[p->state]);
        lv_label_set_text_fmt(s_aux, "音量 %u%%", p->volume);
        char error[49];pf_notice_text(error,sizeof(error),p->error);
        if (!s_radio_empty) lv_label_set_text(s_status, error[0] ? error : p->state == PF_PAUSED ? "确定键继续播放" : p->state == PF_PLAYING ? (s_page == PF_PLAYER ? "列表循环播放" : "正在播放") : "确定键开始 / 重试");
    }
    if (s_bars[0]) for (unsigned i = 0; i < 17; i++) {
        int h = 8 + ((i * i * 7 + 19 + (p->state == PF_PLAYING ? ticks / 120 : 0) * (i % 4 + 1)) % 43);
        lv_obj_set_height(s_bars[i], h); lv_obj_set_y(s_bars[i], 176 - h);
    }
}
void pf_ui_details(const pf_profile_t *profile, const pf_weather_t *w, int64_t epoch,
                   uint64_t now, const pf_timer_t *focus, uint32_t minutes,
                   const pf_timer_t *stopwatch, unsigned selection)
{
    char safe[PF_TITLE + 4];
    bool valid = w->valid && !strcmp(w->city, profile->city);
    weather_icon_update(valid,w->code);
    if (s_page == PF_HOME) {
        if (epoch >= 1704067200) {
            time_t local = epoch + profile->utc_minutes * 60; struct tm tm; gmtime_r(&local, &tm);
            lv_label_set_text_fmt(s_time, "%02d:%02d", tm.tm_hour, tm.tm_min);
            lv_label_set_text_fmt(s_date, "%02d月%02d日", tm.tm_mon + 1, tm.tm_mday);
        }
        supported(safe, sizeof(safe), profile->city[0] ? profile->city : "设置城市"); lv_label_set_text(s_city, safe);
        if (valid) {
            lv_label_set_text_fmt(s_weather, "%s%d.%d C", w->temperature_tenths < 0 ? "-" : "", abs(w->temperature_tenths) / 10, abs(w->temperature_tenths) % 10);
            lv_label_set_text(s_aux, pf_weather_description(w->code));
        } else { lv_label_set_text(s_weather, "-- C"); lv_label_set_text(s_aux, profile->city[0] ? "等待天气" : "后台设置"); }
    } else if (s_page == PF_LAUNCHER) {
        for (unsigned i = 0; i < PF_APP_COUNT; i++) {
            lv_obj_set_style_bg_color(s_menu_bg[i], lv_color_hex(i == selection ? 0x1E4648 : BG), 0);
            lv_obj_set_style_text_color(s_menu[i], lv_color_hex(i == selection ? ACCENT : INK), 0);
        }
    } else if (s_page == PF_WEATHER) {
        if (valid) {
            lv_label_set_text_fmt(s_time, "%s%d.%d C", w->temperature_tenths < 0 ? "-" : "", abs(w->temperature_tenths) / 10, abs(w->temperature_tenths) % 10);
            lv_label_set_text(s_weather, pf_weather_description(w->code));
            lv_label_set_text_fmt(s_aux, "湿度 %d%%\n风速 %d.%d km/h", w->humidity, w->wind_tenths / 10, w->wind_tenths % 10);
        }
        lv_label_set_text(s_status, !profile->city[0] ? "后台设置城市后显示天气" : w->loading ? "正在更新天气" : w->message[0] ? w->message : valid ? (!s_connected ? "离线天气缓存" : w->stale ? "缓存天气，确定键刷新" : "天气已更新") : "等待联网与校时");
    } else if (s_page == PF_FOCUS || s_page == PF_STOPWATCH) {
        bool is_focus = s_page == PF_FOCUS;
        uint64_t elapsed = pf_timer_elapsed(is_focus ? focus : stopwatch, now);
        uint64_t total = (uint64_t)minutes * 60000;
        uint64_t seconds = is_focus ? (elapsed >= total ? 0 : (total - elapsed + 999) / 1000) : elapsed / 1000;
        lv_label_set_text_fmt(s_time, "%02lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
        bool running = is_focus ? focus->running : stopwatch->running;
        lv_label_set_text(s_status, is_focus && elapsed >= total ? "完成了，确定再来一轮" : running ? "计时中，确定暂停" : elapsed ? "已暂停，确定继续" : "确定开始");
    }
}
void pf_ui_github(const pf_github_t *g)
{
    if (s_page != PF_HOME || !s_git_chart || !s_git_pixels) return;
    pf_github_t fallback;
    if (!g->valid) { pf_github_default(&fallback); g=&fallback; }
    if (s_git_drawn && !memcmp(g->levels,s_git_cached.levels,sizeof(g->levels)) && !strcmp(g->username,s_git_cached.username)) return;
    memset(s_git_pixels,0,CHART_BYTES);
    static const uint32_t colors[] = {BG,0x1B3037,0x24523F,0x34865A,0x52B879,0x95E6AD};
    for (unsigned i=0;i<sizeof(colors)/sizeof(colors[0]);i++) {
        lv_color32_t c = lv_color_to_32(lv_color_hex(colors[i]),LV_OPA_COVER); memcpy(s_git_pixels+i*4,&c,4);
    }
    for (unsigned i=0;i<PF_GITHUB_DAYS;i++) if (g->levels[i]<=4) {
        unsigned x=(i/7)*8,y=(i%7)*8;
        for (unsigned dy=0;dy<7;dy++) for (unsigned dx=0;dx<7;dx++) {
            unsigned px=x+dx,at=64+(y+dy)*CHART_STRIDE+px/2;
            unsigned shift=(px&1) ? 0 : 4;
            s_git_pixels[at]=(s_git_pixels[at]&~(15u<<shift)) | (g->levels[i]+1u)<<shift;
        }
    }
    s_git_cached = *g; s_git_drawn = true;
    lv_image_cache_drop(&s_git_dsc); lv_obj_invalidate(s_git_chart);
}
bool pf_ui_font_check(void)
{
    s_font = pocket_font_16; s_font.fallback = &lv_font_montserrat_14;
    const uint32_t points[] = {0x4e2d, 0x6587, 0x64ad, 0x653e, 0x8bbe, 0x7f6e, 0x4e50, 0x3001, 0xff0c, '?'};
    for (unsigned i = 0; i < sizeof(points) / sizeof(points[0]); i++) {
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(&s_font, &glyph, points[i], 0) || glyph.is_placeholder) return false;
    }
    return true;
}
