/* Exercise production worker and transport callbacks with bounded fake I/O.
 * Codec/driver/TLS acceptance remains a physical-device check. */
#include "voice_runtime.h"
#include "pocket/services.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned allocations,tasks,queues,ticks,reads,writes;
static bool http_live,ws_live,mqtt_live,encoder_live,decoder_live,hardware,http_lock,transport_udp;
static int server_socket=-1;
static unsigned server_port;
static unsigned bootstrap_calls, activation_calls, listen_starts, encoded_frames, reply_turns, abort_commands;
#define CONTINUOUS_REPLIES 20
static bool activation_request, awaiting_start, start_notification;
static unsigned startup_order, allocation_policy, task_attempts, player_stops;
static bool reject_player_stop;
static int failure;
static bool failed_release, feeds_paused;
static jmp_buf completed;
static void *checked_malloc(size_t bytes) {
    if(((failure==6 || (failure==30 && listen_starts)) && bytes==1920) || failure==18 || (failure==31 && tasks) || (failure==32 && bytes==25600))return NULL;
    if(allocation_policy==2 && bytes==25600 && tasks)return NULL;
    void *p=malloc(bytes);assert(p);
    if(bytes==25600) { startup_order=tasks?2:1; }
    allocations++;return p;
}
static void *checked_calloc(size_t n,size_t bytes) { void *p=checked_malloc(n*bytes);if(p)memset(p,0,n*bytes);return p; }
static void checked_free(void *p) { if(p) { assert(allocations);allocations--;free(p);if(!allocations && !tasks)startup_order=0; } }
#define malloc checked_malloc
#define calloc checked_calloc
#define free checked_free
#include "../../main/pocket/xiaozhi.c"
#undef malloc
#undef calloc
#undef free

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
int xSemaphoreTake(SemaphoreHandle_t h,uint32_t t) { (void)h;(void)t;return 1; }
int xSemaphoreGive(SemaphoreHandle_t h) { (void)h;return 1; }
static unsigned events;
EventGroupHandle_t xEventGroupCreate(void) { return (void *)1; }
unsigned xEventGroupSetBits(EventGroupHandle_t e,unsigned b) { (void)e;return events|=b; }
unsigned xEventGroupClearBits(EventGroupHandle_t e,unsigned b) { (void)e;events&=~b;return events; }
unsigned xEventGroupGetBits(EventGroupHandle_t e) { (void)e;return events; }
unsigned xEventGroupWaitBits(EventGroupHandle_t e,unsigned b,bool c,bool a,uint32_t t) { (void)e;(void)c;(void)a;(void)t;return events&b; }
typedef struct { unsigned begin,count;packet_t p[4]; } fake_queue_t;
QueueHandle_t xQueueCreate(unsigned n,unsigned bytes) { assert(n==4 && bytes==sizeof(packet_t));queues++;return calloc(1,sizeof(fake_queue_t)); }
int xQueueSend(QueueHandle_t q,const void *p,uint32_t t) { (void)t;fake_queue_t *f=q;if(f->count==4)return 0;f->p[(f->begin+f->count++)%4]=*(const packet_t *)p;return 1; }
int xQueueReceive(QueueHandle_t q,void *p,uint32_t t) { (void)t;fake_queue_t *f=q;if(!f->count)return 0;*(packet_t *)p=f->p[f->begin];f->begin=(f->begin+1)%4;f->count--;return 1; }
void vQueueDelete(QueueHandle_t q) { assert(queues);queues--;free(q); }
int xTaskCreate(void (*f)(void *),const char *n,unsigned z,void *a,unsigned p,TaskHandle_t *out) {
    (void)f;(void)n;(void)p;assert(z==28672 && !a && !tasks && feeds_paused);
    assert((startup_order==0 && allocations==0) || (startup_order==1 && allocations==1));
    task_attempts++;
    if(failure==19 || (allocation_policy==1 && allocations))return 0;tasks++;awaiting_start=true;startup_order=2;*out=(void *)1;return pdPASS;
}
void xTaskNotifyGive(TaskHandle_t t) { assert(t && awaiting_start && s_session && allocations==2);awaiting_start=false;start_notification=true; }
unsigned ulTaskNotifyTake(bool clear,uint32_t wait) { assert(clear && wait==portMAX_DELAY && start_notification && s_session);start_notification=false;return 1; }
void vTaskDelete(TaskHandle_t h) { assert(h && tasks==1 && !ws_live && !mqtt_live && !http_live && !queues && !encoder_live && !decoder_live && !hardware && !http_lock);tasks--;awaiting_start=start_notification=false;startup_order=0; }
void vTaskSuspend(TaskHandle_t h) { (void)h;assert(events&DONE);longjmp(completed,1); }
void vTaskDelay(uint32_t t) { ticks+=t;if((failure==22 && s_view.state==PF_XZ_ACTIVATING))atomic_store(&s_stop,true); }
int64_t esp_timer_get_time(void) { return (int64_t)ticks*1000; }
unsigned esp_get_free_heap_size(void) { return 80000; }
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t h) { (void)h;return 4096; }
unsigned esp_get_minimum_free_heap_size(void) { return 60000; }
size_t heap_caps_get_largest_free_block(uint32_t c) { (void)c;return 40000; }
void runtime_log(const char *t,const char *f,...) { (void)t;(void)f; }
esp_err_t pf_feeds_pause(void) { assert(!tasks);feeds_paused=true;return ESP_OK; }
esp_err_t pf_feeds_resume(void) { assert(!tasks && !allocations);feeds_paused=false;return ESP_OK; }
esp_err_t pf_player_stop(void) { player_stops++;return reject_player_stop?ESP_ERR_TIMEOUT:ESP_OK; }
bool pf_network_ready(void) { return true; }
bool pf_http_take(uint32_t t) { (void)t;assert(!http_lock);http_lock=true;return true; }
void pf_http_give(void) { assert(http_lock && !ws_live && !mqtt_live && !encoder_live && !decoder_live && (!hardware || failure==21) && !http_live);http_lock=false; }
uint8_t pf_volume_get(void) { return 35; }
esp_err_t esp_read_mac(uint8_t *m,int t) { (void)t;memset(m,1,6);return ESP_OK; }
void esp_fill_random(void *p,size_t n) { memset(p,1,n); }
esp_err_t nvs_open(const char *n,int mode,nvs_handle_t *h) { (void)n;(void)mode;*h=1;return ESP_OK; }
esp_err_t nvs_get_str(nvs_handle_t h,const char *k,char *v,size_t *n) { (void)h;(void)k;(void)n;strcpy(v,"00000000-0000-4000-8000-000000000001");return ESP_OK; }
esp_err_t nvs_set_str(nvs_handle_t h,const char *k,const char *v) { (void)h;(void)k;(void)v;return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t h) { (void)h;return ESP_OK; }
void nvs_close(nvs_handle_t h) { (void)h; }
void esp_crt_bundle_attach(void) {}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c) { assert(c->crt_bundle_attach);if(failure==1)return NULL;activation_request=strstr(c->url,"/activate")!=NULL;if(activation_request)activation_calls++;else bootstrap_calls++;reads=0;http_live=true;return (void *)1; }
int esp_http_client_set_method(esp_http_client_handle_t h,int m) { (void)h;(void)m;return ESP_OK; }
int esp_http_client_set_header(esp_http_client_handle_t h,const char *k,const char *v) { (void)h;(void)k;(void)v;return ESP_OK; }
int esp_http_client_open(esp_http_client_handle_t h,int size) { (void)h;(void)size;if(failure==16)atomic_store(&s_stop,true);return failure==2 || (failure==24 && bootstrap_calls==1)?ESP_FAIL:ESP_OK; }
int esp_http_client_write(esp_http_client_handle_t h,const char *p,int n) { (void)h;(void)p;return n; }
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h) { (void)h;return 0; }
int esp_http_client_get_status_code(esp_http_client_handle_t h) { (void)h;return failure==25?403:200; }
int esp_http_client_read(esp_http_client_handle_t h,char *p,int n) {
    (void)h;const char *j=transport_udp?"{\"mqtt\":{\"endpoint\":\"broker.invalid:8883\",\"client_id\":\"test\",\"publish_topic\":\"test\"}}":"{\"websocket\":{\"url\":\"wss://example.invalid/voice\",\"token\":\"\",\"version\":1}}";
    if((failure==22 || failure==23) && bootstrap_calls==1)j="{\"activation\":{\"code\":\"123456\",\"challenge\":\"test\"}}";
    if(activation_request)j="{}";
    if(reads++)return 0;assert(n>(int)strlen(j));strcpy(p,j);return strlen(j);
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h) { (void)h;return failure!=26; }
int esp_http_client_cleanup(esp_http_client_handle_t h) { (void)h;assert(http_live);http_live=false;return ESP_OK; }
esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t *c) { assert(c->crt_bundle_attach);if(failure==3)return NULL;ws_live=true;return (void *)1; }
int esp_websocket_register_events(esp_websocket_client_handle_t h,int id,event_cb_t cb,void *s) { (void)h;(void)id;(void)cb;(void)s;return ESP_OK; }
int esp_websocket_client_start(esp_websocket_client_handle_t h) { (void)h;if(failure==4)return ESP_FAIL;events|=CONNECTED;return ESP_OK; }
int esp_websocket_client_stop(esp_websocket_client_handle_t h) { (void)h;return ESP_OK; }
int esp_websocket_client_destroy(esp_websocket_client_handle_t h) { (void)h;assert(ws_live);ws_live=false;return ESP_OK; }
static void deliver(session_t *s,const char *text) { incoming(s,text,strlen(text)); }
static void auto_command(const char *p)
{
    if(strstr(p,"\"type\":\"listen\"")) {
        assert(strstr(p,"\"state\":\"start\"") && strstr(p,"\"mode\":\"auto\""));
        if(++listen_starts==CONTINUOUS_REPLIES+1)atomic_store(&s_stop,true);
    }
    if(strstr(p,"\"type\":\"abort\""))abort_commands++;
}
int esp_websocket_client_send_text(esp_websocket_client_handle_t h,const char *p,int n,uint32_t t) {
    (void)h;(void)t;session_t *s=s_session;
    if(strstr(p,"\"type\":\"hello\""))deliver(s,"{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"test\",\"audio_params\":{\"format\":\"opus\",\"sample_rate\":24000,\"channels\":1,\"frame_duration\":60}}");
    auto_command(p);
    return n;
}
int esp_websocket_client_send_bin(esp_websocket_client_handle_t h,const char *p,int n,uint32_t t) { (void)h;(void)p;(void)t;return n; }
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *c) { assert(c->broker.verification.crt_bundle_attach);if(failure==3)return NULL;mqtt_live=true;return (void *)1; }
int esp_mqtt_client_register_event(esp_mqtt_client_handle_t h,int e,event_cb_t c,void *s) { (void)h;(void)e;(void)c;(void)s;return ESP_OK; }
int esp_mqtt_client_start(esp_mqtt_client_handle_t h) { (void)h;if(failure==4)return ESP_FAIL;events|=CONNECTED;return ESP_OK; }
int esp_mqtt_client_stop(esp_mqtt_client_handle_t h) { (void)h;return ESP_OK; }
int esp_mqtt_client_destroy(esp_mqtt_client_handle_t h) { (void)h;assert(mqtt_live);mqtt_live=false;return ESP_OK; }
int esp_mqtt_client_publish(esp_mqtt_client_handle_t h,const char *t,const char *p,int n,int q,int r) {
    (void)h;(void)t;(void)q;(void)r;
    if(strstr(p,"\"type\":\"hello\"")) {
        char hello[512];snprintf(hello,sizeof(hello),"{\"type\":\"hello\",\"transport\":\"udp\",\"session_id\":\"test\",\"audio_params\":{\"format\":\"opus\",\"sample_rate\":24000,\"channels\":1,\"frame_duration\":60},\"udp\":{\"server\":\"127.0.0.1\",\"port\":%u,\"key\":\"00000000000000000000000000000000\",\"nonce\":\"01000000000000000000000000000000\"}}",server_port);
        size_t len=strlen(hello);
        for(size_t at=0;at<len;at+=17) {
            esp_mqtt_event_t event={.current_data_offset=at,.total_data_len=len,.data_len=len-at>17?17:len-at,.data=hello+at};
            mqtt_event(s_session,NULL,MQTT_EVENT_DATA,&event);
        }
    }
    auto_command(p);
    return n;
}
static void fake_reply(session_t *s)
{
    reply_turns++;
    if(failure!=28) {
        deliver(s,"{\"type\":\"stt\",\"text\":\"test utterance\"}");
        assert(!atomic_load(&s->listening));
    }
    if(failure==27)return;
    deliver(s,"{\"type\":\"tts\",\"state\":\"start\"}");
    if(transport_udp) {
        uint8_t audio[19]={1,0,0,3};audio[15]=reply_turns;memcpy(audio+16,"abc",3);
        struct sockaddr_in address; socklen_t size=sizeof(address);
        assert(getsockname(s_session->socket,(struct sockaddr *)&address,&size)==0);
        assert(sendto(server_socket,audio,sizeof(audio),0,(struct sockaddr *)&address,size)==sizeof(audio));
        fd_set ready;FD_ZERO(&ready);FD_SET(s_session->socket,&ready);
        struct timeval timeout={.tv_sec=1};
        assert(select(s_session->socket+1,&ready,NULL,NULL,&timeout)==1);
    } else {
        esp_websocket_event_data_t e={.data_ptr="abc",.data_len=3,.fin=true,.op_code=2,.payload_len=3};
        websocket_event(s,NULL,WEBSOCKET_EVENT_DATA,&e);
    }
    deliver(s,"{\"type\":\"tts\",\"state\":\"stop\"}");
}
void mbedtls_aes_init(mbedtls_aes_context *a) { (void)a; }
void mbedtls_aes_free(mbedtls_aes_context *a) { (void)a; }
int mbedtls_aes_setkey_enc(mbedtls_aes_context *a,const uint8_t *k,unsigned b) { (void)a;(void)k;(void)b;return 0; }
int mbedtls_aes_crypt_ctr(mbedtls_aes_context *a,size_t n,size_t *o,uint8_t *iv,uint8_t *stream,const uint8_t *in,uint8_t *out) { (void)a;(void)o;(void)iv;(void)stream;memcpy(out,in,n);return 0; }
void mbedtls_platform_zeroize(void *p,size_t n) { memset(p,0,n); }
int esp_opus_enc_open(void *c,uint32_t z,void **h) { (void)c;(void)z;assert(!decoder_live && !s_session->encoder_reserve);
    if(!listen_starts)assert(!http_live && !ws_live && !mqtt_live);if(failure==5 || (failure==29 && listen_starts))return ESP_FAIL;encoder_live=true;*h=(void *)1;return 0; }
int esp_opus_enc_get_frame_size(void *h,int *in,int *out) { (void)h;*in=1920;*out=256;return 0; }
int esp_opus_enc_process(void *h,esp_audio_enc_in_frame_t *i,esp_audio_enc_out_frame_t *o) { (void)h;(void)i;o->encoded_bytes=3;if(failure==8)return ESP_FAIL;
    ticks+=60;if(++encoded_frames%3==0)fake_reply(s_session);return 0; }
void esp_opus_enc_close(void *h) { (void)h;assert(encoder_live);encoder_live=false; }
int esp_opus_dec_open(void *c,uint32_t z,void **h) { (void)c;(void)z;assert(!encoder_live);if(failure==9)return ESP_FAIL;decoder_live=true;*h=(void *)1;return 0; }
int esp_opus_dec_decode(void *h,esp_audio_dec_in_raw_t *r,esp_audio_dec_out_frame_t *f,esp_audio_dec_info_t *i) { (void)h;r->consumed=r->len;f->decoded_size=2880;*i=(esp_audio_dec_info_t){24000,1,16};return failure==11?ESP_FAIL:0; }
void esp_opus_dec_close(void *h) { (void)h;assert(decoder_live);decoder_live=false; }
esp_err_t bsp_audio_init(void) { if(failure==7)return ESP_FAIL;hardware=true;return ESP_OK; }
esp_err_t bsp_audio_init_playback(void) { if(failure==10)return ESP_FAIL;hardware=true;return ESP_OK; }
esp_err_t bsp_audio_deinit(void) { if(hardware && ((failure==14 && !failed_release) || failure==21)) { failed_release=true;return ESP_FAIL; }hardware=false;return ESP_OK; }
esp_err_t bsp_audio_set_format(uint32_t r,uint8_t b,uint8_t c) { (void)r;(void)b;(void)c;return ESP_OK; }
void bsp_audio_set_volume(uint8_t v) { assert(v==0 || v==35); }
esp_err_t bsp_audio_read(void *p,size_t n) { memset(p,0,n);return ESP_OK; }
esp_err_t bsp_audio_write(const void *p,size_t n) { (void)p;assert(n==2880);writes++;if(failure==17)atomic_store(&s_stop,true);return failure==12?ESP_FAIL:ESP_OK; }
int main(void)
{
    assert(pf_xz_init()==ESP_OK);
    unsigned paths=0;
    for(unsigned round=0;round<10;round++)for(unsigned mode=0;mode<2;mode++)for(int kind=0;kind<=32;kind++) {
        failure=kind;transport_udp=mode;paths++;
        if(transport_udp) {
            server_socket=socket(AF_INET,SOCK_DGRAM,0);assert(server_socket>=0);
            struct sockaddr_in address={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
            assert(bind(server_socket,(struct sockaddr *)&address,sizeof(address))==0);
            socklen_t size=sizeof(address);assert(getsockname(server_socket,(struct sockaddr *)&address,&size)==0);
            server_port=ntohs(address.sin_port);
        }
        ticks=reads=writes=bootstrap_calls=activation_calls=listen_starts=encoded_frames=reply_turns=abort_commands=0;failed_release=false;
        pf_xz_enter();assert(pf_xz_active());
        esp_err_t result=pf_xz_toggle();
        if(failure==18 || failure==19 || failure==31 || failure==32) { assert(result==ESP_ERR_NO_MEM && !s_task && !allocations);assert(pf_xz_stop()==ESP_OK);if(server_socket>=0)close(server_socket);server_socket=-1;continue; }
        assert(result==ESP_OK && tasks==1);
        if(failure==15)atomic_store(&s_stop,true);
        if(failure==20) { assert(pf_xz_toggle()==ESP_ERR_TIMEOUT);assert(s_task && s_session && tasks==1 && pf_xz_active()); }
        if(setjmp(completed)==0)worker(s_session);
        assert(events&DONE);
        assert(!ws_live && !mqtt_live && !http_live && !encoder_live && !decoder_live && !http_lock && !queues);
        if(failure==21) { assert(hardware);assert(pf_xz_stop()==ESP_FAIL && s_task && tasks==1);failure=0; }
        else assert(!hardware);
        if(kind==22)assert(s_view.state==PF_XZ_ACTIVATING && !strcmp(s_view.code,"123456"));
        if(kind==23)assert(bootstrap_calls==2 && activation_calls==1 && !s_view.code[0]);
        if(kind==2 || kind==26)assert(bootstrap_calls==2 && !strcmp(s_view.message,"网络连接失败，请重试"));
        if(kind==24)assert(bootstrap_calls==2);
        if(kind==25)assert(bootstrap_calls==1 && !strcmp(s_view.message,"服务返回错误，请重试"));
        if(kind==27)assert(reply_turns==1 && writes==0 && !strcmp(s_view.message,"回答超时，请重试"));
        if(kind==29 || kind==30)assert(writes==1 && !strcmp(s_view.message,"录音启动失败，请重试"));
        if(kind==0 || kind==13 || kind==23 || kind==24 || kind==28) {
            if(writes!=CONTINUOUS_REPLIES || listen_starts!=CONTINUOUS_REPLIES+1 || s_view.state!=PF_XZ_LISTENING)fprintf(stderr,"mode=%u kind=%d writes=%u state=%u error=%s\n",mode,kind,writes,s_view.state,s_view.message);
            assert(writes==CONTINUOUS_REPLIES && listen_starts==CONTINUOUS_REPLIES+1 && encoded_frames==CONTINUOUS_REPLIES*3 && abort_commands==1 && s_view.state==PF_XZ_LISTENING);
        }
        assert(pf_xz_stop()==ESP_OK);
        assert(!s_task && !s_session && !tasks && !allocations && !pf_xz_active() && !feeds_paused);
        if(server_socket>=0)close(server_socket);server_socket=-1;
    }
    /* Simulate two fragmented-region placements: stack-first succeeds without
     * retry; reserve-first is a bounded fallback with no abandoned task. */
    failure=0;
    for(allocation_policy=1;allocation_policy<=2;allocation_policy++) {
        task_attempts=0;pf_xz_enter();
        assert(pf_xz_toggle()==ESP_OK && tasks==1 && allocations==2);
        assert(task_attempts==allocation_policy && startup_order==2);
        atomic_store(&s_stop,true);
        if(setjmp(completed)==0)worker(s_session);
        assert(pf_xz_stop()==ESP_OK && !tasks && !allocations);
    }
    allocation_policy=0;reject_player_stop=true;pf_xz_enter();
    assert(pf_xz_toggle()==ESP_FAIL && !tasks && !allocations && !feeds_paused);
    reject_player_stop=false;assert(pf_xz_stop()==ESP_OK && player_stops);
    /* A second OK closes all active phases; late cleanup ACK must not restart. */
    const pf_xz_state_t phases[]={PF_XZ_CONNECTING,PF_XZ_LISTENING,PF_XZ_THINKING,PF_XZ_SPEAKING};
    failure=0;transport_udp=false;
    for(unsigned i=0;i<4;i++) {
        pf_xz_enter();assert(pf_xz_toggle()==ESP_OK);view(phases[i],"");
        assert(pf_xz_toggle()==ESP_ERR_TIMEOUT && stopping() && s_task);
        if(setjmp(completed)==0)worker(s_session);
        assert(pf_xz_toggle()==ESP_OK && !tasks && !s_task && !allocations && !feeds_paused);
        assert(pf_xz_active() && s_view.state==PF_XZ_READY);
        assert(pf_xz_stop()==ESP_OK);
    }
    /* RFC6455 fragments and MQTT chunks must never overflow or consume an
     * unrelated session's events. These are the real transport callbacks. */
    session_t *s=calloc(1,sizeof(*s));assert(s);s->version=1;s->audio=xQueueCreate(4,sizeof(packet_t));
    const char *hello="{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"current\",\"audio_params\":{\"format\":\"opus\",\"sample_rate\":24000,\"channels\":1,\"frame_duration\":60}}";
    size_t length=strlen(hello);events=0;
    esp_websocket_event_data_t e={.data_ptr=hello,.data_len=13,.fin=false,.op_code=1,.payload_len=13};
    websocket_event(s,NULL,WEBSOCKET_EVENT_DATA,&e);assert(!(events&HELLO));
    e=(esp_websocket_event_data_t){.data_ptr=hello+13,.data_len=length-13,.fin=true,.op_code=0,.payload_len=length-13};
    websocket_event(s,NULL,WEBSOCKET_EVENT_DATA,&e);assert((events&HELLO) && !(events&FAULT));
    deliver(s,"{\"type\":\"tts\",\"state\":\"start\",\"session_id\":\"old\"}");assert(!atomic_load(&s->tts_started));
    e=(esp_websocket_event_data_t){.data_ptr="abc",.data_len=3,.fin=true,.op_code=2,.payload_len=3};
    websocket_event(s,NULL,WEBSOCKET_EVENT_DATA,&e);assert(((fake_queue_t *)s->audio)->count==1);
    atomic_store(&s->listening,true);websocket_event(s,NULL,WEBSOCKET_EVENT_DATA,&e);assert(((fake_queue_t *)s->audio)->count==1);
    e.payload_offset=1;websocket_event(s,NULL,WEBSOCKET_EVENT_DATA,&e);assert(events&FAULT);
    events=0;esp_mqtt_event_t m={.current_data_offset=0,.total_data_len=2,.data_len=3,.data="abc"};
    mqtt_event(s,NULL,MQTT_EVENT_DATA,&m);assert(events&FAULT);
    release_json(s);vQueueDelete(s->audio);free(s);assert(!queues && !allocations);
    printf("XiaoZhi production-worker lifecycle: PASS (%u WebSocket and MQTT/UDP continuous-turn/failure/cancel/retry paths; four OK-close phases)\n",paths);
}
