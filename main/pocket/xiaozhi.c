/* XiaoZhi protocol interoperability, based on the FoloToy reference's
 * docs/websocket.md and docs/mqtt-udp.md. No upstream UI or application shell. */
#include "xiaozhi.h"
#include "services.h"
#include "bsp_audio.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_websocket_client.h"
#include "mqtt_client.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "mbedtls/aes.h"
#include "mbedtls/platform_util.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#define OTA_URL "https://api.tenclass.net/xiaozhi/ota/"
#define DONE BIT0
#define CONNECTED BIT1
#define HELLO BIT2
#define FAULT BIT3
#define PACKET_MAX 1275
#define JSON_MAX 4096
#define VOICE_STACK_BYTES 28672
#define ENCODER_RESERVE_BYTES 25600
typedef enum { CONFIG_INVALID, CONFIG_NETWORK, CONFIG_MEMORY, CONFIG_SERVICE, CONFIG_ACTIVATION } config_failure_t;
typedef struct { uint16_t bytes; uint8_t data[PACKET_MAX]; } packet_t;
typedef struct {
    esp_websocket_client_handle_t ws;
    esp_mqtt_client_handle_t mqtt;
    QueueHandle_t audio;
    int socket;
    bool http_owned, udp, transport_started, encoded_logged, played_logged;
    unsigned version, rate, duration, port;
    config_failure_t config_failure;
    char mac[18], uuid[37], session[80], host[160];
    char url[512], token[768], client_id[160], username[160], password[384], topic[256];
    uint8_t key[16], nonce[16];
    uint32_t tx_sequence, rx_sequence;
    char *json;
    size_t json_used, json_total, json_capacity;
    size_t frame_offset, frame_size;
    int opcode;
    packet_t packet;
    uint8_t wire[PACKET_MAX+17];
    void *encoder, *decoder, *encoder_reserve;
    uint8_t *pcm;
    int input_bytes;
    atomic_bool listening, tts_started, tts_stopped;
    atomic_uint tts_stopped_at;
} session_t;
static const char *TAG="pocket_xiaozhi";
static SemaphoreHandle_t s_lock;
static EventGroupHandle_t s_events;
static TaskHandle_t s_task;
static session_t *s_session;
static atomic_bool s_active, s_stop;
static pf_xz_snapshot_t s_view;

static uint32_t now_ms(void) { return esp_timer_get_time()/1000; }
static void heap_log(const char *stage)
{
    ESP_LOGI(TAG,"%s heap=%u largest=%u stack=%u",stage,(unsigned)esp_get_free_heap_size(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),(unsigned)uxTaskGetStackHighWaterMark(NULL));
}
static bool stopping(void) { return atomic_load(&s_stop); }
static void release_json(session_t *s)
{
    if(s->json) { mbedtls_platform_zeroize(s->json,s->json_capacity);free(s->json); }
    s->json=NULL;s->json_used=s->json_total=s->json_capacity=0;
    s->frame_offset=s->frame_size=0;s->opcode=0;
}
static void view(pf_xz_state_t state,const char *message)
{
    xSemaphoreTake(s_lock,portMAX_DELAY);
    s_view.state=state;
    if(state==PF_XZ_ERROR)pf_notice_text(s_view.message,sizeof(s_view.message),message);
    else snprintf(s_view.message,sizeof(s_view.message),"%s",message);
    xSemaphoreGive(s_lock);
}
void pf_xz_snapshot(pf_xz_snapshot_t *out)
{
    xSemaphoreTake(s_lock,portMAX_DELAY);*out=s_view;xSemaphoreGive(s_lock);
}
bool pf_xz_active(void) { return atomic_load(&s_active); }
static const char *str(const cJSON *j,const char *key)
{
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);
    return cJSON_IsString(v)?v->valuestring:"";
}
static bool field(const cJSON *j,const char *key,char *out,size_t cap,bool required)
{
    const char *v=str(j,key);
    if((required && !*v) || strlen(v)>=cap)return false;
    for(const unsigned char *p=(const unsigned char *)v;*p;p++)if(*p<32 || *p==127)return false;
    strcpy(out,v);return true;
}
static bool wait_bits(unsigned bits,uint32_t ms)
{
    uint32_t at=now_ms();
    while(!stopping() && now_ms()-at<ms) {
        EventBits_t got=xEventGroupGetBits(s_events);
        if(got&FAULT)return false;
        if(got&bits)return true;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return false;
}
static bool identity(session_t *s)
{
    uint8_t mac[6],random[16];
    if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK)return false;
    snprintf(s->mac,sizeof(s->mac),"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    nvs_handle_t nvs;
    if(nvs_open("pocket_xiaozhi",NVS_READWRITE,&nvs)!=ESP_OK)return false;
    size_t bytes=sizeof(s->uuid);
    bool ok=true;
    if(nvs_get_str(nvs,"uuid",s->uuid,&bytes)!=ESP_OK || strlen(s->uuid)!=36) {
        esp_fill_random(random,sizeof(random));random[6]=(random[6]&15)|64;random[8]=(random[8]&63)|128;
        snprintf(s->uuid,sizeof(s->uuid),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            random[0],random[1],random[2],random[3],random[4],random[5],random[6],random[7],random[8],random[9],random[10],random[11],random[12],random[13],random[14],random[15]);
        ok=nvs_set_str(nvs,"uuid",s->uuid)==ESP_OK && nvs_commit(nvs)==ESP_OK;
    }
    nvs_close(nvs);return ok;
}
/* Bootstrap obtains credentials/activation only. Never apply remote firmware,
 * device-control, time-zone or image updates to the user's application. */
static cJSON *bootstrap(session_t *s,bool activate)
{
    s->config_failure=CONFIG_MEMORY;
    s->json=malloc(JSON_MAX+1);s->json_capacity=JSON_MAX+1;
    if(!s->json) { release_json(s);return NULL; }
    char body[640];
    snprintf(body,sizeof(body),"{\"version\":2,\"language\":\"zh-CN\",\"flash_size\":8388608,\"minimum_free_heap_size\":%u,\"mac_address\":\"%s\",\"uuid\":\"%s\",\"chip_model_name\":\"esp32c3\",\"application\":{\"name\":\"pocket-passport\",\"version\":\"2.5.0\"},\"board\":{\"type\":\"folotoy-ai-passport\",\"name\":\"ai-passport\",\"mac\":\"%s\"}}",
        (unsigned)esp_get_minimum_free_heap_size(),s->mac,s->uuid,s->mac);
    esp_http_client_config_t cfg={.url=activate?OTA_URL "activate":OTA_URL,.timeout_ms=8000,
        .buffer_size=1024,.buffer_size_tx=512,.crt_bundle_attach=esp_crt_bundle_attach,
        .disable_auto_redirect=true,.user_agent="folotoy-ai-passport/2.5.0 PocketPassport"};
    esp_http_client_handle_t h=esp_http_client_init(&cfg);
    if(!h) { release_json(s);return NULL; }
    const char *payload=activate?"{}":body;
    esp_http_client_set_method(h,HTTP_METHOD_POST);
    esp_http_client_set_header(h,"Activation-Version","1");
    esp_http_client_set_header(h,"Device-Id",s->mac);
    esp_http_client_set_header(h,"Client-Id",s->uuid);
    esp_http_client_set_header(h,"Accept-Language","zh-CN");
    esp_http_client_set_header(h,"Content-Type","application/json");
    cJSON *j=NULL;
    size_t size=strlen(payload);
    s->config_failure=CONFIG_NETWORK;
    if(!stopping() && esp_http_client_open(h,size)==ESP_OK &&
       esp_http_client_write(h,payload,size)==(int)size && esp_http_client_fetch_headers(h)>=0) {
        int code=esp_http_client_get_status_code(h);
        s->config_failure=code==200?CONFIG_INVALID:CONFIG_SERVICE;
        if(code==200) {
            size_t used=0;uint32_t start=now_ms();
            while(used<JSON_MAX && !stopping() && now_ms()-start<8000) {
                int got=esp_http_client_read(h,s->json+used,JSON_MAX-used);
                if(got<=0)break;
                used+=got;
            }
            s->json[used]=0;
            if(activate)j=cJSON_CreateObject();
            else if(!esp_http_client_is_complete_data_received(h))s->config_failure=CONFIG_NETWORK;
            else if(pf_json_depth_valid(s->json,used)) {
                const char *end;
                j=cJSON_ParseWithLengthOpts(s->json,used+1,&end,true);
            }
        }
    }
    esp_http_client_cleanup(h);
    release_json(s);
    return j;
}
static bool configure(session_t *s)
{
    uint32_t start=now_ms();
    do {
        cJSON *j=NULL;
        for(unsigned attempt=0;attempt<2 && !stopping();attempt++) {
            j=bootstrap(s,false);
            if(j || s->config_failure!=CONFIG_NETWORK)break;
            if(!attempt) {
                view(PF_XZ_CONNECTING,"网络连接失败，正在重试");
                for(unsigned i=0;i<5 && !stopping();i++)vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        if(!j)return false;
        const cJSON *activation=cJSON_GetObjectItemCaseSensitive(j,"activation");
        const char *code=str(activation,"code");
        if(*code) {
            s->config_failure=CONFIG_ACTIVATION;
            bool challenge=*str(activation,"challenge")!=0;
            xSemaphoreTake(s_lock,portMAX_DELAY);
            snprintf(s_view.code,sizeof(s_view.code),"%.31s",code);
            xSemaphoreGive(s_lock);
            view(PF_XZ_ACTIVATING,"到 xiaozhi.me 添加设备");
            cJSON_Delete(j);
            for(unsigned i=0;i<50 && !stopping();i++)vTaskDelay(pdMS_TO_TICKS(100));
            if(challenge && !stopping()) { cJSON *reply=bootstrap(s,true);cJSON_Delete(reply); }
            continue;
        }
        xSemaphoreTake(s_lock,portMAX_DELAY);s_view.code[0]=0;xSemaphoreGive(s_lock);
        const cJSON *mqtt=cJSON_GetObjectItemCaseSensitive(j,"mqtt"), *ws=cJSON_GetObjectItemCaseSensitive(j,"websocket");
        bool ok=false;
        if(cJSON_IsObject(mqtt)) {
            s->udp=true;s->version=3;
            ok=field(mqtt,"endpoint",s->host,sizeof(s->host),true) &&
                field(mqtt,"client_id",s->client_id,sizeof(s->client_id),true) &&
                field(mqtt,"username",s->username,sizeof(s->username),false) &&
                field(mqtt,"password",s->password,sizeof(s->password),false) &&
                field(mqtt,"publish_topic",s->topic,sizeof(s->topic),true);
        } else if(cJSON_IsObject(ws)) {
            s->udp=false;s->version=1;
            cJSON *v=cJSON_GetObjectItemCaseSensitive(ws,"version");
            if(cJSON_IsNumber(v) && v->valueint)s->version=v->valueint;
            ok=field(ws,"url",s->url,sizeof(s->url),true) && !strncmp(s->url,"wss://",6) &&
                field(ws,"token",s->token,sizeof(s->token),false) && s->version>=1 && s->version<=3 &&
                (!cJSON_IsNumber(v) || v->valuedouble==v->valueint);
        }
        cJSON_Delete(j);return ok;
    } while(!stopping() && now_ms()-start<180000);
    return false;
}
static bool send_text(session_t *s,const char *text)
{
    size_t bytes=strlen(text);
    return s->udp ? esp_mqtt_client_publish(s->mqtt,s->topic,text,bytes,0,0)>=0 :
        esp_websocket_client_send_text(s->ws,text,bytes,pdMS_TO_TICKS(1500))==(int)bytes;
}
static bool command(session_t *s,const char *type,const char *state)
{
    if(stopping() && strcmp(type,"abort"))return false;
    char text[320];
    cJSON *j=cJSON_CreateObject();
    if(!j)return false;
    bool ok=cJSON_AddStringToObject(j,"session_id",s->session) && cJSON_AddStringToObject(j,"type",type);
    if(state)ok=ok && cJSON_AddStringToObject(j,"state",state) && cJSON_AddStringToObject(j,"mode","auto");
    ok=ok && cJSON_PrintPreallocated(j,text,sizeof(text),false) && send_text(s,text);
    cJSON_Delete(j);return ok;
}
static void incoming(session_t *s,const char *text,size_t bytes)
{
    pf_xz_event_t e;
    if(!pf_xz_parse(text,bytes,&e)) { xEventGroupSetBits(s_events,FAULT);return; }
    if(e.type!=PF_XZ_HELLO && e.session[0] && strcmp(e.session,s->session))return;
    if(e.type==PF_XZ_HELLO) {
        if(e.udp!=s->udp || xEventGroupGetBits(s_events)&HELLO) { xEventGroupSetBits(s_events,FAULT);return; }
        if(s->udp) {
            cJSON *j=cJSON_ParseWithLength(text,bytes), *udp=cJSON_GetObjectItemCaseSensitive(j,"udp");
            const cJSON *port=cJSON_GetObjectItemCaseSensitive(udp,"port");
            bool ok=field(udp,"server",s->host,sizeof(s->host),true) && cJSON_IsNumber(port) &&
                port->valueint>0 && port->valueint<=65535 && port->valuedouble==port->valueint &&
                pf_xz_hex16(str(udp,"key"),s->key) && pf_xz_hex16(str(udp,"nonce"),s->nonce) &&
                s->nonce[0]==1 && s->nonce[1]==0;
            if(ok)s->port=port->valueint;
            cJSON_Delete(j);
            if(!ok) { xEventGroupSetBits(s_events,FAULT);return; }
        }
        s->rate=e.rate;s->duration=e.duration;strcpy(s->session,e.session);
        xEventGroupSetBits(s_events,HELLO);
    } else if(e.type==PF_XZ_TTS_START) {
        atomic_store(&s->tts_stopped,false);atomic_store(&s->tts_started,true);atomic_store(&s->listening,false);
    } else if(e.type==PF_XZ_TTS_STOP) {
        atomic_store(&s->tts_stopped_at,now_ms());atomic_store(&s->tts_stopped,true);
    }
    else if(e.type==PF_XZ_GOODBYE)xEventGroupSetBits(s_events,FAULT);
    else if(e.type==PF_XZ_TEXT || e.type==PF_XZ_STT || e.type==PF_XZ_EMOTION) {
        xSemaphoreTake(s_lock,portMAX_DELAY);
        if(e.type==PF_XZ_EMOTION)s_view.emotion=e.emotion;else strcpy(s_view.text,e.text);
        xSemaphoreGive(s_lock);
        if(e.type==PF_XZ_STT)atomic_store(&s->listening,false);
    }
}
static void mqtt_event(void *arg,esp_event_base_t base,int32_t id,void *data)
{
    (void)base;session_t *s=arg;esp_mqtt_event_handle_t e=data;
    if(stopping())return;
    if(id==MQTT_EVENT_CONNECTED)xEventGroupSetBits(s_events,CONNECTED);
    else if(id==MQTT_EVENT_ERROR || id==MQTT_EVENT_DISCONNECTED)xEventGroupSetBits(s_events,FAULT);
    else if(id==MQTT_EVENT_DATA) {
        if(e->current_data_offset==0) {
            release_json(s);
            if(e->total_data_len<=0 || e->total_data_len>JSON_MAX) { xEventGroupSetBits(s_events,FAULT);return; }
            s->json_total=e->total_data_len;s->json_capacity=s->json_total+1;
            s->json=malloc(s->json_capacity);
        }
        if(e->data_len<0 || e->current_data_offset!=(int)s->json_used || s->json_total>JSON_MAX ||
            !s->json || s->json_used+(size_t)e->data_len>s->json_total) { release_json(s);xEventGroupSetBits(s_events,FAULT);return; }
        memcpy(s->json+s->json_used,e->data,e->data_len);s->json_used+=e->data_len;
        if(s->json_used==s->json_total) { s->json[s->json_used]=0;incoming(s,s->json,s->json_used);release_json(s); }
    }
}
static void websocket_event(void *arg,esp_event_base_t base,int32_t id,void *data)
{
    (void)base;session_t *s=arg;esp_websocket_event_data_t *e=data;
    if(stopping())return;
    if(id==WEBSOCKET_EVENT_CONNECTED)xEventGroupSetBits(s_events,CONNECTED);
    else if(id==WEBSOCKET_EVENT_ERROR || id==WEBSOCKET_EVENT_DISCONNECTED)xEventGroupSetBits(s_events,FAULT);
    else if(id==WEBSOCKET_EVENT_DATA) {
        if(e->op_code==9 || e->op_code==10)return;
        if(e->op_code==8) { xEventGroupSetBits(s_events,FAULT);return; }
        /* Assemble complete messages, including RFC6455 continuation frames. */
        if(e->payload_offset==0) {
            if(e->op_code==1 || e->op_code==2) {
                if(s->opcode) { xEventGroupSetBits(s_events,FAULT);return; }
                s->json_used=0;s->opcode=e->op_code;
                s->json_capacity=s->opcode==1?JSON_MAX+1:PACKET_MAX+17;
                s->json=malloc(s->json_capacity);
                if(!s->json) { release_json(s);xEventGroupSetBits(s_events,FAULT);return; }
            } else if(e->op_code!=0 || !s->opcode) { xEventGroupSetBits(s_events,FAULT);return; }
            s->frame_offset=0;s->frame_size=e->payload_len;
        }
        if(e->data_len<0 || e->payload_offset<0 || e->payload_len<0 ||
            (size_t)e->payload_offset!=s->frame_offset || (size_t)e->payload_len!=s->frame_size ||
            s->frame_offset+(size_t)e->data_len>s->frame_size ||
            !s->json || s->json_used+(size_t)e->data_len>=s->json_capacity) { release_json(s);xEventGroupSetBits(s_events,FAULT);return; }
        memcpy(s->json+s->json_used,e->data_ptr,e->data_len);s->json_used+=e->data_len;
        s->frame_offset+=e->data_len;
        if(e->fin && e->payload_offset+e->data_len==e->payload_len) {
            if(s->opcode==1) { s->json[s->json_used]=0;incoming(s,s->json,s->json_used); }
            else if(s->opcode==2) {
                const uint8_t *payload;size_t length;
                if(!pf_xz_binary(s->version,(uint8_t *)s->json,s->json_used,&payload,&length)) { release_json(s);xEventGroupSetBits(s_events,FAULT);return; }
                if(!atomic_load(&s->listening)) {
                    packet_t p={.bytes=length};memcpy(p.data,payload,length);
                    if(xQueueSend(s->audio,&p,pdMS_TO_TICKS(150))!=pdTRUE)xEventGroupSetBits(s_events,FAULT);
                }
            } else xEventGroupSetBits(s_events,FAULT);
            release_json(s);
        }
    }
}
static bool connect_transport(session_t *s)
{
    if(s->udp) {
        char broker[192];
        if(strchr(s->host,'/') || strchr(s->host,'@'))return false;
        snprintf(broker,sizeof(broker),"mqtts://%s%s",s->host,strchr(s->host,':')?"":":8883");
        esp_mqtt_client_config_t cfg={
            .broker.address.uri=broker,.broker.verification.crt_bundle_attach=esp_crt_bundle_attach,
            .credentials.client_id=s->client_id,.credentials.username=s->username,.credentials.authentication.password=s->password,
            .session.keepalive=60,.network.disable_auto_reconnect=true,.network.timeout_ms=8000,
            .task.stack_size=6144,.task.priority=4,.buffer.size=1024,.buffer.out_size=1024,
        };
        s->mqtt=esp_mqtt_client_init(&cfg);
        if(!s->mqtt || esp_mqtt_client_register_event(s->mqtt,ESP_EVENT_ANY_ID,mqtt_event,s)!=ESP_OK ||
           esp_mqtt_client_start(s->mqtt)!=ESP_OK)return false;
    } else {
        s->audio=xQueueCreate(4,sizeof(packet_t));
        if(!s->audio)return false;
        char headers[1024];
        int n=snprintf(headers,sizeof(headers),"Device-Id: %s\r\nClient-Id: %s\r\nProtocol-Version: %u\r\n",s->mac,s->uuid,s->version);
        if(s->token[0])snprintf(headers+n,sizeof(headers)-n,"Authorization: %s%s\r\n",strchr(s->token,' ')?"":"Bearer ",s->token);
        esp_websocket_client_config_t cfg={.uri=s->url,.headers=headers,.crt_bundle_attach=esp_crt_bundle_attach,
            .disable_auto_reconnect=true,.network_timeout_ms=8000,.task_stack=6144,.task_prio=4,.buffer_size=1024};
        s->ws=esp_websocket_client_init(&cfg);
        if(!s->ws || esp_websocket_register_events(s->ws,WEBSOCKET_EVENT_ANY,websocket_event,s)!=ESP_OK ||
            esp_websocket_client_start(s->ws)!=ESP_OK)return false;
    }
    s->transport_started=true;
    if(!wait_bits(CONNECTED,20000))return false;
    char hello[300];
    snprintf(hello,sizeof(hello),"{\"type\":\"hello\",\"version\":%u,\"transport\":\"%s\",\"features\":{\"mcp\":false},\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}",s->version,s->udp?"udp":"websocket");
    if(!send_text(s,hello) || !wait_bits(HELLO,10000))return false;
    if(s->udp) {
        struct addrinfo hint={.ai_family=AF_INET,.ai_socktype=SOCK_DGRAM}, *address=NULL;
        char port[8];snprintf(port,sizeof(port),"%u",s->port);
        if(getaddrinfo(s->host,port,&hint,&address)!=0)return false;
        s->socket=socket(address->ai_family,SOCK_DGRAM,0);
        bool ok=s->socket>=0 && connect(s->socket,address->ai_addr,address->ai_addrlen)==0;
        freeaddrinfo(address);
        if(!ok)return false;
        int flags=fcntl(s->socket,F_GETFL,0);
        if(flags<0 || fcntl(s->socket,F_SETFL,flags|O_NONBLOCK)<0)return false;
    }
    return true;
}
static bool crypt_audio(session_t *s,const uint8_t nonce[16],const uint8_t *in,size_t size,uint8_t *out)
{
    mbedtls_aes_context aes;mbedtls_aes_init(&aes);
    uint8_t iv[16],stream[16]={0};size_t offset=0;memcpy(iv,nonce,16);
    int ret=mbedtls_aes_setkey_enc(&aes,s->key,128);
    if(!ret)ret=mbedtls_aes_crypt_ctr(&aes,size,&offset,iv,stream,in,out);
    mbedtls_aes_free(&aes);return ret==0;
}
static void be32(uint8_t *p,uint32_t value)
{ p[0]=value>>24;p[1]=value>>16;p[2]=value>>8;p[3]=value; }
static bool send_audio(session_t *s,size_t bytes)
{
    if(s->udp) {
        memcpy(s->wire,s->nonce,16);s->wire[2]=bytes>>8;s->wire[3]=bytes;
        be32(s->wire+8,0);be32(s->wire+12,++s->tx_sequence);
        return crypt_audio(s,s->wire,s->packet.data,bytes,s->wire+16) &&
            send(s->socket,s->wire,bytes+16,0)==(int)(bytes+16);
    }
    size_t header=s->version==1?0:s->version==2?16:4;
    memset(s->wire,0,header);
    if(s->version==2) { s->wire[1]=2;be32(s->wire+12,bytes); }
    else if(s->version==3) { s->wire[2]=bytes>>8;s->wire[3]=bytes; }
    memcpy(s->wire+header,s->packet.data,bytes);
    return esp_websocket_client_send_bin(s->ws,(char *)s->wire,header+bytes,pdMS_TO_TICKS(1500))==(int)(header+bytes);
}
static bool receive_audio(session_t *s)
{
    if(!s->udp)return xQueueReceive(s->audio,&s->packet,0)==pdTRUE;
    int got=recv(s->socket,s->wire,sizeof(s->wire),0);
    if(got<0) { if(errno!=EAGAIN && errno!=EWOULDBLOCK)xEventGroupSetBits(s_events,FAULT);return false; }
    uint32_t sequence;
    if(!pf_xz_udp(s->wire,got,s->rx_sequence,&sequence))return false;
    s->packet.bytes=got-16;
    if(!crypt_audio(s,s->wire,s->wire+16,s->packet.bytes,s->packet.data)) { xEventGroupSetBits(s_events,FAULT);return false; }
    s->rx_sequence=sequence;return true;
}
static esp_err_t close_audio(session_t *s)
{
    free(s->encoder_reserve);s->encoder_reserve=NULL;
    if(s->encoder) { esp_opus_enc_close(s->encoder);s->encoder=NULL; }
    if(s->decoder) { esp_opus_dec_close(s->decoder);s->decoder=NULL; }
    free(s->pcm);s->pcm=NULL;
    return bsp_audio_deinit();
}
static bool open_encoder(session_t *s)
{
    esp_opus_enc_config_t cfg={.sample_rate=16000,.channel=1,.bits_per_sample=16,.bitrate=16000,
        .frame_duration=ESP_OPUS_ENC_FRAME_DURATION_60_MS,.application_mode=ESP_OPUS_ENC_APPLICATION_VOIP,
        .complexity=0,.enable_vbr=true};
    int output_bytes;
    if(esp_opus_enc_open(&cfg,sizeof(cfg),&s->encoder)!=ESP_AUDIO_ERR_OK ||
        esp_opus_enc_get_frame_size(s->encoder,&s->input_bytes,&output_bytes)!=ESP_AUDIO_ERR_OK ||
        s->input_bytes!=1920 || output_bytes>PACKET_MAX) { heap_log("Opus encoder initialization failed");return false; }
    heap_log("Opus encoder ready");
    return true;
}
static bool start_capture(session_t *s)
{
    if(!s->encoder && !open_encoder(s))return false;
    s->pcm=malloc(s->input_bytes);
    if(!s->pcm || bsp_audio_init()!=ESP_OK || bsp_audio_set_format(16000,16,1)!=ESP_OK)return false;
    bsp_audio_set_volume(0);
    heap_log("Opus capture ready");
    atomic_store(&s->tts_started,false);atomic_store(&s->tts_stopped,false);
    return command(s,"listen","start");
}
static bool capture(session_t *s)
{
    if(stopping() || !atomic_load(&s->listening))return true;
    esp_audio_enc_in_frame_t in={.buffer=s->pcm,.len=s->input_bytes};
    esp_audio_enc_out_frame_t out={.buffer=s->packet.data,.len=sizeof(s->packet.data)};
    bool ok=bsp_audio_read(s->pcm,s->input_bytes)==ESP_OK &&
        esp_opus_enc_process(s->encoder,&in,&out)==ESP_AUDIO_ERR_OK && out.encoded_bytes &&
        out.encoded_bytes<=PACKET_MAX && send_audio(s,out.encoded_bytes);
    if(ok && !s->encoded_logged) { s->encoded_logged=true;heap_log("first Opus encoded"); }
    return ok;
}
static bool playback(session_t *s)
{
    if(!s->decoder) {
        esp_opus_dec_cfg_t cfg={.sample_rate=s->rate,.channel=1,.frame_duration=ESP_OPUS_DEC_FRAME_DURATION_60_MS,.self_delimited=false};
        if(esp_opus_dec_open(&cfg,sizeof(cfg),&s->decoder)!=ESP_AUDIO_ERR_OK)return false;
        s->pcm=malloc(2880);
        if(!s->pcm || bsp_audio_init_playback()!=ESP_OK || bsp_audio_set_format(s->rate,16,1)!=ESP_OK)return false;
        heap_log("Opus playback ready");
    }
    esp_audio_dec_in_raw_t raw={.buffer=s->packet.data,.len=s->packet.bytes};
    esp_audio_dec_out_frame_t frame={.buffer=s->pcm,.len=2880};esp_audio_dec_info_t info;
    if(esp_opus_dec_decode(s->decoder,&raw,&frame,&info)!=ESP_AUDIO_ERR_OK || raw.consumed!=raw.len ||
        !frame.decoded_size || frame.decoded_size>2880 || frame.decoded_size%2 || info.sample_rate!=s->rate ||
        info.channel!=1 || info.bits_per_sample!=16)return false;
    bsp_audio_set_volume(pf_volume_get());
    if(bsp_audio_write(s->pcm,frame.decoded_size)!=ESP_OK)return false;
    if(!s->played_logged) { s->played_logged=true;heap_log("first Opus PCM output"); }
    xSemaphoreTake(s_lock,portMAX_DELAY);s_view.pcm_at=now_ms();xSemaphoreGive(s_lock);
    return true;
}
static void worker(void *arg)
{
    (void)arg;
    /* Creation reserves the large blocks before allocating the context. */
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    session_t *s=s_session;
    const char *error=NULL;
    uint32_t at=now_ms(),last_audio=0,reply_at=0;
    bool recording=false;
    view(PF_XZ_CONNECTING,"正在连接小智");
    if(!pf_network_ready()) { error="未联网，请进入手机后台";goto done; }
    while(!stopping() && time(NULL)<1704067200 && now_ms()-at<10000)vTaskDelay(pdMS_TO_TICKS(100));
    if(time(NULL)<1704067200) { error="网络校时失败，请重试";goto done; }
    while(!stopping() && !(s->http_owned=pf_http_take(100))) {}
    if(stopping())goto done;
    free(s->encoder_reserve);s->encoder_reserve=NULL;
    if(!open_encoder(s)) { error="录音启动失败，请重试";goto done; }
    heap_log("before bootstrap");
    if(!identity(s)) { error="小智设备标识读取失败";goto done; }
    if(!configure(s)) {
        error=s->config_failure==CONFIG_NETWORK?"网络连接失败，请重试":
            s->config_failure==CONFIG_MEMORY?"配置内存不足，请重试":
            s->config_failure==CONFIG_SERVICE?"服务返回错误，请重试":
            s->config_failure==CONFIG_ACTIVATION?"激活尚未完成，请重试":"配置内容无效，请重试";
        goto done;
    }
    if(stopping())goto done;
    view(PF_XZ_CONNECTING,"正在建立语音连接");
    if(!connect_transport(s)) { error="小智连接失败，请重试";goto done; }
    heap_log("transport connected");
    atomic_store(&s->listening,true);
    while(!stopping()) {
        if(xEventGroupGetBits(s_events)&FAULT || !pf_network_ready()) { error="语音连接中断，请重试";break; }
        if(atomic_load(&s->listening)) {
            if(!recording) {
                for(unsigned i=0;i<16 && !stopping() && receive_audio(s);i++) {}
                if((!s->encoder && close_audio(s)!=ESP_OK) || !start_capture(s)) { error="录音启动失败，请重试";break; }
                recording=true;reply_at=0;
                xSemaphoreTake(s_lock,portMAX_DELAY);s_view.text[0]=0;s_view.emotion=PF_XZ_NEUTRAL;xSemaphoreGive(s_lock);
                view(PF_XZ_LISTENING,"正在听，请直接说话");
            }
            if(!capture(s)) { error="录音发送失败，请重试";break; }
        } else {
            if(recording) {
                if(close_audio(s)!=ESP_OK) { error="结束录音失败，请重试";break; }
                recording=false;reply_at=now_ms();last_audio=reply_at;
                view(PF_XZ_THINKING,"让我想一想");
            }
            if(atomic_load(&s->tts_started)) {
                view(PF_XZ_SPEAKING,"正在回答");
                if(receive_audio(s)) {
                    if(!playback(s)) { error="语音解码或输出失败";break; }
                    last_audio=now_ms();
                } else {
                    if(atomic_load(&s->tts_stopped) && now_ms()-last_audio>=500 &&
                       now_ms()-atomic_load(&s->tts_stopped_at)>=500) {
                        if(close_audio(s)!=ESP_OK) { error="音频资源释放失败";break; }
                        atomic_store(&s->tts_started,false);atomic_store(&s->tts_stopped,false);reply_at=0;
                        atomic_store(&s->listening,true);
                        view(PF_XZ_LISTENING,"正在听，请直接说话");
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            } else vTaskDelay(pdMS_TO_TICKS(20));
            if(!atomic_load(&s->listening) && now_ms()-reply_at>=90000) { error="回答超时，请重试";break; }
        }
    }
done:
    atomic_store(&s->listening,false);
    /* Silence hardware before bounded network shutdown/abort can block. */
    if(close_audio(s)!=ESP_OK)error="音频资源释放失败";
    if(stopping() && (xEventGroupGetBits(s_events)&HELLO)) (void)command(s,"abort",NULL);
    if(s->ws) { if(s->transport_started)esp_websocket_client_stop(s->ws);esp_websocket_client_destroy(s->ws);s->ws=NULL; }
    if(s->mqtt) { if(s->transport_started)esp_mqtt_client_stop(s->mqtt);esp_mqtt_client_destroy(s->mqtt);s->mqtt=NULL; }
    if(s->socket>=0) { close(s->socket);s->socket=-1; }
    if(s->audio) { vQueueDelete(s->audio);s->audio=NULL; }
    release_json(s);
    if(s->http_owned) { pf_http_give();s->http_owned=false; }
    if(!stopping())view(error?PF_XZ_ERROR:PF_XZ_READY,error?error:"");
    ESP_LOGI(TAG,"Session cleaned: heap=%u largest=%u",(unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    xEventGroupSetBits(s_events,DONE);
    vTaskSuspend(NULL);
}
esp_err_t pf_xz_init(void)
{
    s_lock=xSemaphoreCreateMutex();s_events=xEventGroupCreate();
    return s_lock && s_events ? ESP_OK : ESP_ERR_NO_MEM;
}
void pf_xz_enter(void)
{
    atomic_store(&s_active,true);
    xSemaphoreTake(s_lock,portMAX_DELAY);s_view=(pf_xz_snapshot_t){.state=PF_XZ_READY};
    xSemaphoreGive(s_lock);
}
esp_err_t pf_xz_stop(void)
{
    if(s_task) {
        atomic_store(&s_stop,true);
        if(!(xEventGroupWaitBits(s_events,DONE,false,true,pdMS_TO_TICKS(15000))&DONE)) {
            view(PF_XZ_ERROR,"停止小智超时，请重试");return ESP_ERR_TIMEOUT;
        }
        if(bsp_audio_deinit()!=ESP_OK) { view(PF_XZ_ERROR,"音频资源释放失败");return ESP_FAIL; }
        vTaskDelete(s_task);s_task=NULL;
        mbedtls_platform_zeroize(s_session,sizeof(*s_session));free(s_session);s_session=NULL;
    }
    atomic_store(&s_active,false);atomic_store(&s_stop,false);
    xEventGroupClearBits(s_events,DONE|CONNECTED|HELLO|FAULT);
    view(PF_XZ_OFF,"");
    return pf_feeds_resume();
}
esp_err_t pf_xz_toggle(void)
{
    if(s_task && (stopping() || !(xEventGroupGetBits(s_events)&DONE))) {
        esp_err_t result=pf_xz_stop();
        if(result==ESP_OK)pf_xz_enter();
        return result;
    }
    if(s_task && xEventGroupGetBits(s_events)&DONE) {
        esp_err_t result=pf_xz_stop();if(result!=ESP_OK)return result;
        pf_xz_enter();
    }
    if(!s_task) {
        if(pf_feeds_pause()!=ESP_OK) { view(PF_XZ_ERROR,"后台停止超时，请重试");return ESP_ERR_TIMEOUT; }
        atomic_store(&s_stop,false);xEventGroupClearBits(s_events,DONE|CONNECTED|HELLO|FAULT);
        /* Keep both large allocations contiguous before small context/TLS
         * allocations. The worker waits for context ownership before starting. */
        heap_log("before session allocation");
        void *reserve=malloc(ENCODER_RESERVE_BYTES);
        if(!reserve) { view(PF_XZ_ERROR,"语音任务内存不足");return ESP_ERR_NO_MEM; }
        if(xTaskCreate(worker,"pocket_xiaozhi",VOICE_STACK_BYTES,NULL,4,&s_task)!=pdPASS) {
            free(reserve);view(PF_XZ_ERROR,"语音任务内存不足");return ESP_ERR_NO_MEM;
        }
        s_session=calloc(1,sizeof(*s_session));
        if(!s_session) {
            /* Not notified: this task owns no context, transport or hardware. */
            vTaskDelete(s_task);s_task=NULL;free(reserve);
            view(PF_XZ_ERROR,"语音任务内存不足");return ESP_ERR_NO_MEM;
        }
        s_session->socket=-1;s_session->encoder_reserve=reserve;
        xTaskNotifyGive(s_task);
    }
    return ESP_OK;
}
