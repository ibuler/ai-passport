#include "xiaozhi_model.h"
#include <string.h>

static uint32_t be32(const uint8_t *p)
{ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
bool pf_xz_binary(unsigned version,const uint8_t *data,size_t bytes,const uint8_t **payload,size_t *length)
{
    if(!data || !payload || !length)return false;
    size_t head=0,n=bytes;
    if(version==2) {
        head=16;if(bytes<head || data[0] || data[1]!=2 || data[2] || data[3])return false;
        n=be32(data+12);
    } else if(version==3) {
        head=4;if(bytes<head || data[0] || data[1])return false;
        n=((unsigned)data[2]<<8)|data[3];
    } else if(version!=1)return false;
    if(!n || n>1275 || bytes!=head+n)return false;
    *payload=data+head;*length=n;return true;
}
bool pf_xz_udp(const uint8_t *data,size_t bytes,uint32_t previous,uint32_t *sequence)
{
    if(!data || !sequence || bytes<17 || bytes>1291 || data[0]!=1 || data[1])return false;
    if((((unsigned)data[2]<<8)|data[3])!=bytes-16)return false;
    uint32_t seq=be32(data+12);
    if(seq<=previous)return false;
    *sequence=seq;return true;
}
bool pf_xz_hex16(const char *text,uint8_t out[16])
{
    if(!text || strlen(text)!=32)return false;
    for(unsigned i=0;i<16;i++) {
        unsigned value=0;
        for(unsigned j=0;j<2;j++) {
            unsigned char c=(unsigned char)text[i*2+j];
            int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
            if(digit<0)return false;
            value=(value<<4)|(unsigned)digit;
        }
        out[i]=(uint8_t)value;
    }
    return true;
}
unsigned pf_xz_portrait(const pf_xz_snapshot_t *s,uint32_t now)
{
    if(s->state==PF_XZ_LISTENING)return 2;
    if(s->state==PF_XZ_CONNECTING || s->state==PF_XZ_THINKING)return 3;
    if(s->state==PF_XZ_ERROR)return 7;
    if(s->state==PF_XZ_SPEAKING && s->pcm_at && (uint32_t)(now-s->pcm_at)<180) {
        /* Brief emotion accents between talking-mouth frames. */
        if(s->emotion!=PF_XZ_NEUTRAL && now%1200<400)return 5+s->emotion;
        return 4+(now/140)%2;
    }
    if(s->emotion==PF_XZ_HAPPY)return 6;
    if(s->emotion==PF_XZ_SAD)return 7;
    if(s->emotion==PF_XZ_SURPRISED)return 8;
    return now%4000>=3840 ? 1 : 0;
}
