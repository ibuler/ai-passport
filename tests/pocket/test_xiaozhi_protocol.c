#include "pocket/xiaozhi_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool parse(const char *text,pf_xz_event_t *e) { return pf_xz_parse(text,strlen(text),e); }
int main(void)
{
    const char *hello="{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"session\\\"withquote\",\"audio_params\":{\"format\":\"opus\",\"sample_rate\":24000,\"channels\":1,\"frame_duration\":60}}";
    pf_xz_event_t e;
    assert(parse(hello,&e) && e.type==PF_XZ_HELLO && e.rate==24000 && !e.udp);
    for(size_t i=0;i<strlen(hello);i++)assert(!pf_xz_parse(hello,i,&e));
    char bad[512];strcpy(bad,hello);strstr(bad,"24000")[0]='9';assert(!parse(bad,&e));
    strcpy(bad,hello);strcat(bad,"{}");assert(!parse(bad,&e));
    assert(!parse("[]",&e) && !parse("{}",&e));
    assert(parse("{\"type\":\"tts\",\"state\":\"start\"}",&e) && e.type==PF_XZ_TTS_START);
    assert(parse("{\"type\":\"tts\",\"state\":\"stop\"}",&e) && e.type==PF_XZ_TTS_STOP);
    assert(parse("{\"type\":\"llm\",\"emotion\":\"happy\"}",&e) && e.type==PF_XZ_EMOTION && e.emotion==PF_XZ_HAPPY);
    assert(parse("{\"type\":\"llm\",\"emotion\":\"surprised\"}",&e) && e.emotion==PF_XZ_SURPRISED);
    assert(parse("{\"type\":\"system\",\"command\":\"reboot\"}",&e) && e.type==PF_XZ_IGNORE);
    assert(parse("{\"type\":\"stt\",\"text\":\"line1\\nline2\"}",&e) && !strcmp(e.text,"line1 line2"));
    assert(parse("{\"type\":\"stt\",\"text\":\"\xc0\xaf\"}",&e) && !e.text[0]);
    memset(bad,'a',sizeof(bad));strcpy(bad,"{\"type\":\"stt\",\"text\":\"");
    size_t prefix=strlen(bad);memset(bad+prefix,'x',400);strcpy(bad+prefix+400,"\"}");
    assert(parse(bad,&e) && e.type==PF_XZ_STT && strlen(e.text)==255);
    puts("XiaoZhi JSON/handshake/emotion/bounds validation: PASS");
}
