#include "pocket/model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv)
{
    char notice[49];
    pf_notice_text(notice,sizeof(notice),"网络连接失败，请重试！多余");
    assert(!strcmp(notice,"网络连接失败，请重试！多"));
    pf_notice_text(notice,sizeof(notice),"12345678901!extra");assert(!strcmp(notice,"12345678901!"));
    pf_notice_text(notice,7,"中文字符");assert(!strcmp(notice,"中文"));
    pf_notice_text(notice,sizeof(notice),"😀12345678901tail");assert(!strcmp(notice,"😀12345678901"));
    pf_notice_text(notice,sizeof(notice),"bad\xff");assert(!notice[0]);
    pf_notice_text(notice,sizeof(notice),"line\nbreak");assert(!notice[0]);
    pf_notice_text(notice,sizeof(notice),NULL);assert(!notice[0]);
    pf_profile_t p; pf_profile_defaults(&p); assert(pf_profile_valid(&p));
    p.latitude_e4=900001; assert(!pf_profile_valid(&p)); p.latitude_e4=0;
    p.longitude_e4=-1800001; assert(!pf_profile_valid(&p)); p.longitude_e4=0;
    p.utc_minutes=841; assert(!pf_profile_valid(&p)); p.utc_minutes=480;
    strcpy(p.city,"珠海"); p.radio_count=2;
    strcpy(p.radios[0].city,"珠海"); strcpy(p.radios[1].city,"广州");
    for(unsigned i=0;i<2;i++) { strcpy(p.radios[i].track.title,"城市电台"); strcpy(p.radios[i].track.url,"https://example.com/live.mp3"); }
    assert(pf_profile_valid(&p) && pf_radio_count(&p)==1);
    assert(pf_radio_step(&p,PF_RADIOS,1)==0 && pf_radio_step(&p,0,-1)==0);
    strcpy(p.city,"广州"); assert(pf_radio_step(&p,PF_RADIOS,1)==1);
    strcpy(p.city,"北京"); assert(pf_radio_step(&p,0,1)==PF_RADIOS && pf_radio_count(&p)==0);
    p.radios[1].track.url[0]=0; assert(!pf_profile_valid(&p));
    pf_timer_t timer={0}; pf_timer_toggle(&timer,1000);
    assert(pf_timer_elapsed(&timer,3100)==2100);
    pf_timer_toggle(&timer,3100); assert(!timer.running && pf_timer_elapsed(&timer,9000)==2100);
    pf_timer_toggle(&timer,9000); assert(pf_timer_elapsed(&timer,10000)==3100);
    pf_timer_reset(&timer); assert(!timer.running && pf_timer_elapsed(&timer,10000)==0);
    pf_timer_toggle(&timer,UINT64_C(4294967296)); assert(pf_timer_elapsed(&timer,UINT64_C(4294968296))==1000);
    assert(!strcmp(pf_weather_description(0),"晴") && !strcmp(pf_weather_description(95),"雷雨"));
    assert(!strcmp(pf_weather_description(7),"天气未知"));
    assert(pf_weather_icon(0)==PF_ICON_SUN && pf_weather_icon(2)==PF_ICON_PARTLY);
    assert(pf_weather_icon(3)==PF_ICON_CLOUD && pf_weather_icon(45)==PF_ICON_FOG);
    assert(pf_weather_icon(61)==PF_ICON_RAIN && pf_weather_icon(85)==PF_ICON_SNOW);
    assert(pf_weather_icon(95)==PF_ICON_THUNDER && pf_weather_icon(7)==PF_ICON_UNKNOWN);
    char deep[70]; memset(deep,'[',33); memset(deep+33,']',33);
    assert(!pf_json_depth_valid(deep,66)); assert(pf_json_depth_valid("{\"x\":[1]}",9));
    assert(!pf_json_depth_valid("{",1) && !pf_json_depth_valid("]",1));
    assert(pf_json_depth_valid("\"[text]\"",8));
    pf_config_t c;
    pf_defaults(&c); assert(pf_config_valid(&c));
    c.volume = 101; assert(!pf_config_valid(&c)); c.volume = 35;
    assert(pf_volume_step(35, 1)==40 && pf_volume_step(35, -1)==30);
    assert(pf_volume_step(98, 1)==100 && pf_volume_step(2, -1)==0);
    assert(pf_volume_step(100, 1)==100 && pf_volume_step(0, -1)==0);
    assert(pf_volume_step(37, 0)==37 && pf_volume_step(37, -1)==32);
    c.count = PF_TRACKS + 1; assert(!pf_config_valid(&c)); c.count = 1;
    assert(pf_text_valid("中文名字睿", 32, false));
    assert(!pf_text_valid("\xe0\x80\x80", 4, false));
    assert(!pf_text_valid("\xed\xa0\x80", 4, false));
    assert(!pf_text_valid("\xf4\x90\x80\x80", 5, false));
    assert(!pf_text_valid("\xe4\xb8", 3, false));
    assert(!pf_text_valid("abc\n", 6, false));
    assert(!pf_text_valid("abc", 3, false));
    assert(!pf_text_valid("", 2, false));
    assert(pf_text_valid("", 2, true));
    assert(pf_url_valid("https://example.com/stream?id=1"));
    assert(pf_url_valid("http://192.168.1.2:8000/music.mp3"));
    const char *bad[] = {"file:///tmp/a", "https:///file", "http://", "http://?x", "http://u:p@host/file", "http://x/\r\nHeader:hi", "http://x/中文", "http://x\\evil"};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) assert(!pf_url_valid(bad[i]));
    assert(pf_track_step(0,-1,8)==7 && pf_track_step(7,1,8)==0);
    assert(pf_track_step(9,-1,0)==0 && pf_track_step(99,1,2)==1);
    pf_playback_t play={.state=PF_FINISHED,.track=0}; unsigned next=99;
    assert(pf_playlist_advance(PF_PLAYER,&play,4,&next) && next==1);
    play.track=3; assert(pf_playlist_advance(PF_PLAYER,&play,4,&next) && next==0);
    play.track=0; assert(pf_playlist_advance(PF_PLAYER,&play,1,&next) && next==0);
    for (unsigned page=PF_HOME;page<=PF_SETTINGS;page++) if (page!=PF_PLAYER)
        assert(!pf_playlist_advance(page,&play,4,&next));
    for (unsigned state=PF_IDLE;state<=PF_ERROR;state++) if (state!=PF_FINISHED) {
        play.state=state; next=99;
        assert(!pf_playlist_advance(PF_PLAYER,&play,4,&next) && next==99);
    }
    play.state=PF_FINISHED;
    assert(!pf_playlist_advance(PF_PLAYER,&play,0,&next));
    assert(!pf_playlist_advance(PF_PLAYER,&play,PF_TRACKS+1,&next));
    play.track=4; assert(!pf_playlist_advance(PF_PLAYER,&play,4,&next));
    assert(pf_crc32(0,"123456789",9)==0xcbf43926);
    uint32_t crc=pf_crc32(0,"1234",4); assert(pf_crc32(crc,"56789",5)==0xcbf43926);
    uint8_t h[16]={'P','F','I','1',96,0,96,0,1,0,1,0,0,0,0,0};
    pf_image_info_t info;
    assert(pf_image_header(h,16+2+PF_FRAME_BYTES,&info));
    assert(!pf_image_header(h,15+2+PF_FRAME_BYTES,&info));
    h[8]=49; assert(!pf_image_header(h,16+49*(2+PF_FRAME_BYTES),&info));
    h[8]=2; assert(!pf_image_header(h,16+2*(2+PF_FRAME_BYTES),&info));
    h[10]=3; assert(pf_image_header(h,16+2*(2+PF_FRAME_BYTES),&info));
    h[10]=4; assert(!pf_image_header(h,16+2*(2+PF_FRAME_BYTES),&info));
    assert(pf_delay_valid(100) && pf_delay_valid(2000));
    assert(!pf_delay_valid(99) && !pf_delay_valid(2001));
    if (argc==2) {
        FILE *f=fopen(argv[1],"rb"); assert(f);
        assert(!fseek(f,0,SEEK_END)); long bytes=ftell(f); rewind(f);
        uint8_t *data=malloc(bytes); assert(data && fread(data,1,bytes,f)==(size_t)bytes); fclose(f);
        assert(pf_image_header(data,bytes,&info));
        assert(pf_crc32(0,data+16,bytes-16)==info.crc);
        for(unsigned i=0;i<info.frames;i++) { unsigned at=16+i*(2+PF_FRAME_BYTES); assert(pf_delay_valid(data[at] | (uint16_t)data[at+1]<<8)); }
        free(data);
    }
    puts("Pocket model: PASS (UTF-8, URLs, config, navigation, image limits, cross-language CRC)");
}
