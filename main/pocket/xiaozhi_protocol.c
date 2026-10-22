#include "xiaozhi_model.h"
#include "model.h"
#include "cJSON.h"
#include <string.h>

static const char *string(const cJSON *j, const char *key)
{
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);
    return cJSON_IsString(v) ? v->valuestring : "";
}
static void copy(char *out,size_t size,const char *text)
{
    size_t n=strlen(text);
    if(n>=size) { n=size-1; while(n && ((unsigned char)text[n]&0xc0)==0x80) n--; }
    memcpy(out,text,n);out[n]=0;
    for(size_t i=0;i<n;i++)if((unsigned char)out[i]<32 || out[i]==127)out[i]=' ';
    if(!pf_text_valid(out,size,true))out[0]=0;
}
static bool number(const cJSON *j,const char *key,unsigned *out)
{
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);
    if(!cJSON_IsNumber(v) || v->valuedouble!=v->valueint || v->valueint<0)return false;
    *out=(unsigned)v->valueint;return true;
}
bool pf_xz_parse(const char *data,size_t bytes,pf_xz_event_t *out)
{
    if(!data || !out || !bytes || bytes>4096 || !pf_json_depth_valid(data,bytes))return false;
    memset(out,0,sizeof(*out));
    const char *end=NULL;
    cJSON *j=cJSON_ParseWithLengthOpts(data,bytes,&end,false);
    if(!j)return false;
    while(end<data+bytes && (*end==' ' || *end=='\r' || *end=='\n' || *end=='\t'))end++;
    bool ok=cJSON_IsObject(j) && end==data+bytes;
    const char *type=string(j,"type");
    if(!type[0])ok=false;
    const char *session=string(j,"session_id");
    if(*session && !pf_text_valid(session,sizeof(out->session),false))ok=false;
    if(ok)copy(out->session,sizeof(out->session),session);
    if(ok && !strcmp(type,"hello")) {
        const char *transport=string(j,"transport"), *session=string(j,"session_id");
        const cJSON *a=cJSON_GetObjectItemCaseSensitive(j,"audio_params");
        unsigned channels=0;
        ok=(!strcmp(transport,"websocket") || !strcmp(transport,"udp")) &&
            session[0] && strlen(session)<sizeof(out->session) &&
            pf_text_valid(session,sizeof(out->session),false) &&
            !strcmp(string(a,"format"),"opus") && number(a,"sample_rate",&out->rate) &&
            (out->rate==16000 || out->rate==24000) && number(a,"channels",&channels) && channels==1 &&
            number(a,"frame_duration",&out->duration) &&
            (out->duration==20 || out->duration==40 || out->duration==60);
        if(ok) { out->type=PF_XZ_HELLO;out->udp=!strcmp(transport,"udp");copy(out->session,sizeof(out->session),session); }
    } else if(ok && !strcmp(type,"tts")) {
        const char *state=string(j,"state");
        if(!strcmp(state,"start"))out->type=PF_XZ_TTS_START;
        else if(!strcmp(state,"stop"))out->type=PF_XZ_TTS_STOP;
        else if(!strcmp(state,"sentence_start")) { out->type=PF_XZ_TEXT;copy(out->text,sizeof(out->text),string(j,"text")); }
    } else if(ok && !strcmp(type,"stt")) {
        out->type=PF_XZ_STT;copy(out->text,sizeof(out->text),string(j,"text"));
    } else if(ok && !strcmp(type,"llm")) {
        const char *e=string(j,"emotion");out->type=PF_XZ_EMOTION;
        if(!strcmp(e,"happy") || !strcmp(e,"laughing") || !strcmp(e,"funny") || !strcmp(e,"loving") || !strcmp(e,"confident"))out->emotion=PF_XZ_HAPPY;
        else if(!strcmp(e,"sad") || !strcmp(e,"crying") || !strcmp(e,"angry") || !strcmp(e,"embarrassed"))out->emotion=PF_XZ_SAD;
        else if(!strcmp(e,"surprised") || !strcmp(e,"shocked"))out->emotion=PF_XZ_SURPRISED;
    } else if(ok && !strcmp(type,"goodbye"))out->type=PF_XZ_GOODBYE;
    cJSON_Delete(j);return ok;
}
