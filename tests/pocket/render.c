#include "pocket/ui.h"
#include "pocket/github.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
LV_FONT_DECLARE(pocket_font_16);
static uint16_t s_frame[240*320];
static uint16_t s_draw[240*40];
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    uint16_t *p=(uint16_t*)pixels;
    for(int y=area->y1;y<=area->y2;y++) for(int x=area->x1;x<=area->x2;x++) s_frame[y*240+x]=*p++;
    lv_display_flush_ready(display);
}
static void save(const char *name)
{
    lv_refr_now(NULL);
    char path[256]; snprintf(path,sizeof(path),"%s.ppm",name);
    FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"P6\n240 320\n255\n");
    for(unsigned i=0;i<240*320;i++) {
        uint16_t p=s_frame[i];
        unsigned char rgb[]={((p>>11)&31)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}
static void check_sources(const char *path)
{
    FILE *f=fopen(path,"rb"); assert(f);
    unsigned missing=0;
    for(int c;(c=fgetc(f))!=EOF;) {
        if(c<0x80) continue;
        unsigned cp=c&((c<0xe0)?31:(c<0xf0)?15:7);
        unsigned count=c<0xe0?1:c<0xf0?2:3;
        while(count--) { int n=fgetc(f); assert(n!=EOF); cp=(cp<<6)|(n&63); }
        lv_font_glyph_dsc_t d={0};
        if(!lv_font_get_glyph_dsc(&pocket_font_16,&d,cp,0) || d.is_placeholder) { fprintf(stderr,"Missing fixed glyph U+%04X in %s\n",cp,path); missing++; }
    }
    fclose(f); assert(!missing);
}
static lv_obj_t *find_label(lv_obj_t *obj,const char *text)
{
    if(lv_obj_check_type(obj,&lv_label_class) && !strcmp(lv_label_get_text(obj),text))return obj;
    for(unsigned i=0;i<lv_obj_get_child_count(obj);i++) {
        lv_obj_t *found=find_label(lv_obj_get_child(obj,i),text);if(found)return found;
    }
    return NULL;
}
static bool contains(lv_obj_t *obj, const char *text)
{
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), text)) return true;
    for (unsigned i=0;i<lv_obj_get_child_count(obj);i++) if (contains(lv_obj_get_child(obj,i),text)) return true;
    return false;
}
int main(int argc,char **argv)
{
    lv_init();
    lv_display_t *display=lv_display_create(240,320);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,s_draw,NULL,sizeof(s_draw),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    assert(pf_ui_font_check());
    assert(argc>1);
    for(int i=1;i<argc;i++) check_sources(argv[i]);
    lv_font_glyph_dsc_t absent={0};
    assert(!lv_font_get_glyph_dsc(&pocket_font_16,&absent,0x1f600,0) || absent.is_placeholder);
    pf_config_t config; pf_defaults(&config);
    strcpy(config.name, "开发者");
    pf_profile_t profile; pf_profile_defaults(&profile); strcpy(profile.company, "FoloToy"); strcpy(profile.city, "珠海");
    pf_weather_t weather={.valid=true,.temperature_tenths=235,.code=2,.humidity=55,.wind_tenths=112}; strcpy(weather.city, "珠海");
    pf_timer_t focus={0}, stopwatch={.elapsed_ms=125000};
    pf_github_t github={.valid=true}; strcpy(github.username,"test-user");
    for(unsigned i=0;i<PF_GITHUB_DAYS;i++) github.levels[i]=i%5;
    const char *source=getenv("PF_GITHUB_HTML");
    if(source) {
        FILE *f=fopen(source,"rb");assert(f); pf_github_parser_t parser;
        const char *day=getenv("PF_GITHUB_DAY");assert(day);
        pf_github_parser_init(&parser,strtoll(day,NULL,10),"ibuler");
        char chunk[1024];size_t n;while((n=fread(chunk,1,sizeof(chunk),f)))pf_github_parser_feed(&parser,chunk,n);
        fclose(f);assert(pf_github_parser_finish(&parser,&github));
    }
    pf_playback_t play={.state=PF_PLAYING,.seconds=721,.volume=35};
    static const char *names[] = {"home", "launcher", "player", "radio", "weather", "focus", "stopwatch", "settings", "xiaozhi"};
    for(unsigned cycle=0;cycle<12;cycle++) for(unsigned page=PF_HOME; page<=PF_XIAOZHI; page++) {
        pf_ui_show(page,&config,&profile,0,"Pocket-Focus-1234","12345678");
        pf_ui_update(&play,true,85,1200);
        if (page==PF_PLAYER) assert(contains(lv_screen_active(),"列表循环播放"));
        pf_ui_details(&profile,&weather,1792597800,1200,&focus,25,&stopwatch,cycle%PF_APP_COUNT);
        pf_ui_github(&github);
        if(!cycle) { char name[80]; snprintf(name,sizeof(name),"build/%s",names[page]); save(name); }
    }
    pf_xz_snapshot_t xz={.state=PF_XZ_READY};
    for(unsigned frame=0;frame<9;frame++) {
        xz.state=frame==2?PF_XZ_LISTENING:frame==3?PF_XZ_THINKING:frame==4||frame==5?PF_XZ_SPEAKING:PF_XZ_READY;
        xz.emotion=frame==6?PF_XZ_HAPPY:frame==7?PF_XZ_SAD:frame==8?PF_XZ_SURPRISED:PF_XZ_NEUTRAL;
        strcpy(xz.message,frame==2?"正在听，请直接说话":frame==3?"让我想一想":frame==4||frame==5?"正在回答":"");
        strcpy(xz.text,frame==4||frame==5?"今天想聊点什么？":"");
        unsigned tick=frame==1?3900:frame==5?1260:1200;xz.pcm_at=tick;
        assert(pf_xz_portrait(&xz,tick)==frame);
        pf_ui_xiaozhi(&xz,tick,frame%2?100:0);
        assert(contains(lv_screen_active(),"小智"));
        assert(!contains(lv_screen_active(),"芙宁娜") && !contains(lv_screen_active(),"确定键开始语音对话"));
        char name[80];snprintf(name,sizeof(name),"build/furina-%u",frame);save(name);
    }
    xz.state=PF_XZ_ERROR;strcpy(xz.text,"今天想聊点什么？");strcpy(xz.message,"网络连接失败，请重试！多余");
    pf_ui_xiaozhi(&xz,1200,35);assert(contains(lv_screen_active(),"网络连接失败，请重试！多"));
    lv_obj_update_layout(lv_screen_active());
    assert(lv_obj_get_y(find_label(lv_screen_active(),"网络连接失败，请重试！多"))>=230);
    assert(!contains(lv_screen_active(),"今天想聊点什么？"));
    save("build/xiaozhi-short-error");
    strcpy(xz.message,"录音启动失败，请重试");pf_ui_xiaozhi(&xz,1200,35);save("build/xiaozhi-error-below");
    xz.state=PF_XZ_ACTIVATING;strcpy(xz.code,"123456");strcpy(xz.message,"到 xiaozhi.me 添加设备");
    pf_ui_xiaozhi(&xz,1200,35);assert(contains(lv_screen_active(),"123456"));save("build/xiaozhi-activation");
    pf_ui_show(PF_HOME,&config,&profile,0,"test","12345678");
    memset(&weather,0,sizeof(weather));
    pf_ui_update(&play,false,-1,1200); pf_ui_details(&profile,&weather,0,1200,&focus,25,&stopwatch,0);
    save("build/home-offline");
    assert(contains(lv_screen_active(),"--:--") && contains(lv_screen_active(),"等待天气"));
    pf_github_t no_data={0};strcpy(no_data.username,"other-user");pf_ui_github(&no_data);
    assert(!contains(lv_screen_active(),"等待联网读取贡献图") && !contains(lv_screen_active(),"GitHub @ibuler"));
    save("build/home-default");
    static const int codes[]={0,2,3,45,61,71,95,-1};
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);i++) {
        weather.valid=true;strcpy(weather.city,profile.city);weather.code=codes[i];
        pf_ui_show(PF_HOME,&config,&profile,0,"test","12345678");
        pf_ui_details(&profile,&weather,1792597800,1200,&focus,25,&stopwatch,0);pf_ui_github(&github);
        char name[80];snprintf(name,sizeof(name),"build/weather-icon-%u",i);save(name);
    }
    weather.valid=true; weather.temperature_tenths=-5; strcpy(weather.city,profile.city);
    pf_ui_show(PF_WEATHER,&config,&profile,0,"test","12345678");
    pf_ui_details(&profile,&weather,1792597800,1200,&focus,25,&stopwatch,0);
    assert(contains(lv_screen_active(),"-0.5 C"));
    config.count=0; pf_ui_show(PF_RADIO,&config,&profile,0,"test","12345678");
    pf_ui_message("电台目录暂不可用，请重试"); pf_ui_update(&play,false,-1,1200);
    assert(contains(lv_screen_active(),"电台目录暂不可用，请重试"));
    for (unsigned page=PF_PLAYER;page<=PF_RADIO;page++) {
        pf_ui_show(page,&config,&profile,0,"test","12345678");
        play.volume=0; pf_ui_update(&play,true,85,1200);
        assert(contains(lv_screen_active(),"音量 0%"));
        play.volume=100; pf_ui_update(&play,true,85,1200);
        assert(contains(lv_screen_active(),"音量 100%"));
        assert(contains(lv_screen_active(),"长按上/下:音量+/-"));
        lv_refr_now(NULL);
        save(page==PF_PLAYER ? "build/player-volume-max" : "build/radio-volume-max");
    }
    /* Exercise the smaller production pool with maximum-length saved fields. */
    memset(config.name, 'N', sizeof(config.name)-1); config.name[sizeof(config.name)-1]=0;
    memset(profile.company, 'C', sizeof(profile.company)-1); profile.company[sizeof(profile.company)-1]=0;
    memset(profile.city, 'X', sizeof(profile.city)-1); profile.city[sizeof(profile.city)-1]=0;
    config.count=PF_TRACKS;
    for(unsigned i=0;i<PF_TRACKS;i++) {
        memset(config.tracks[i].title,'T',PF_TITLE-1); config.tracks[i].title[PF_TITLE-1]=0;
    }
    play.state=PF_ERROR; strcpy(play.error,"音乐解码失败，请重试");
    for(unsigned page=PF_HOME;page<=PF_SETTINGS;page++) {
        pf_ui_show(page,&config,&profile,PF_TRACKS-1,"Pocket-Focus-1234","12345678");
        pf_ui_update(&play,true,85,1200);
        pf_ui_details(&profile,&weather,1792597800,1200,&focus,25,&stopwatch,0);
        pf_ui_github(&github); lv_refr_now(NULL);
    }
    lv_mem_monitor_t mem; lv_mem_monitor(&mem);
    assert(mem.total_size <= LV_MEM_SIZE);
    assert(mem.max_used + 4096 <= mem.total_size);
    printf("UI render: PASS; pool peak=%lu / %lu usable bytes (%lu reserved); 12 navigation cycles + max fields\n",
           (unsigned long)mem.max_used, (unsigned long)mem.total_size, (unsigned long)LV_MEM_SIZE);
    return 0;
}
