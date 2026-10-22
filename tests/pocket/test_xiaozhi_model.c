#include "pocket/xiaozhi_model.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    pf_xz_snapshot_t s={.state=PF_XZ_READY};
    assert(pf_xz_portrait(&s,1000)==0 && pf_xz_portrait(&s,3900)==1);
    s.state=PF_XZ_LISTENING;assert(pf_xz_portrait(&s,3900)==2);
    s.state=PF_XZ_THINKING;assert(pf_xz_portrait(&s,1000)==3);
    s.state=PF_XZ_SPEAKING;s.pcm_at=1200;
    assert(pf_xz_portrait(&s,1200)==4 && pf_xz_portrait(&s,1260)==5);
    assert(pf_xz_portrait(&s,1400)==0); // No talking mouth after PCM stops.
    s.emotion=PF_XZ_SAD;assert(pf_xz_portrait(&s,1400)==7);
    s.emotion=PF_XZ_SURPRISED;assert(pf_xz_portrait(&s,1400)==8);
    s.emotion=PF_XZ_HAPPY;assert(pf_xz_portrait(&s,1400)==6);
    s.emotion=PF_XZ_SAD;assert(pf_xz_portrait(&s,1200)==7);
    s.pcm_at=1700;assert(pf_xz_portrait(&s,1700)==4);
    const uint8_t *payload;size_t bytes;
    uint8_t packet[1300]={0};packet[1]=2;packet[15]=3;
    assert(pf_xz_binary(2,packet,19,&payload,&bytes) && bytes==3 && payload==packet+16);
    for(size_t i=0;i<19;i++)assert(!pf_xz_binary(2,packet,i,&payload,&bytes));
    packet[12]=255;assert(!pf_xz_binary(2,packet,19,&payload,&bytes));
    memset(packet,0,sizeof(packet));packet[3]=3;
    assert(pf_xz_binary(3,packet,7,&payload,&bytes) && bytes==3);
    assert(!pf_xz_binary(3,packet,8,&payload,&bytes));
    packet[0]=1;assert(!pf_xz_binary(3,packet,7,&payload,&bytes));
    assert(pf_xz_binary(1,packet,1275,&payload,&bytes));
    assert(!pf_xz_binary(1,packet,1276,&payload,&bytes));
    uint32_t seq;
    memset(packet,0,sizeof(packet));packet[0]=1;packet[3]=3;packet[15]=1;
    assert(pf_xz_udp(packet,19,0,&seq) && seq==1);
    assert(!pf_xz_udp(packet,19,1,&seq));
    for(size_t i=0;i<19;i++)assert(!pf_xz_udp(packet,i,0,&seq));
    packet[3]=4;assert(!pf_xz_udp(packet,19,0,&seq));
    uint8_t key[16];
    assert(pf_xz_hex16("0123456789ABCDEF0123456789abcdef",key) && key[0]==1 && key[15]==239);
    assert(!pf_xz_hex16("0123456789ABCDEF0123456789abcdeg",key));
    assert(!pf_xz_hex16("01",key));
    puts("XiaoZhi state/portrait/framing/UDP validation: PASS");
}
