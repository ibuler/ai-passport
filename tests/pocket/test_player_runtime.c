#include "runtime.h"
#include "pocket/services.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned allocated,attempts,fail_at,gives,reads,writes;
static bool owns_http,http_live,decoder_live,awake;
static bool fail_task_create,fail_release,idle_ack=true;
static unsigned tasks_live,task_creations,task_deletions;
static int scenario;
static esp_alloc_failed_hook_t failed_hook;
static void *checked_malloc(size_t size)
{
    if (++attempts==fail_at) { failed_hook(size,MALLOC_CAP_8BIT,"test_malloc"); return NULL; }
    void *p=malloc(size); assert(p); allocated++; return p;
}
static void checked_free(void *p) { if(p) { assert(allocated); allocated--; free(p); } }
#define malloc checked_malloc
#define free checked_free
#include "../../main/pocket/player.c"
#undef malloc
#undef free

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
int xSemaphoreTake(SemaphoreHandle_t m,uint32_t t) { (void)m;(void)t;return 1; }
int xSemaphoreGive(SemaphoreHandle_t m) { (void)m;return 1; }
EventGroupHandle_t xEventGroupCreate(void) { return (void *)1; }
unsigned xEventGroupSetBits(EventGroupHandle_t e,unsigned b) { (void)e;return b; }
unsigned xEventGroupClearBits(EventGroupHandle_t e,unsigned b) { (void)e;return b; }
unsigned xEventGroupWaitBits(EventGroupHandle_t e,unsigned b,bool c,bool a,uint32_t t) { (void)e;(void)c;(void)a;(void)t;return idle_ack ? b : 0; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (void *)1; }
int xTaskCreate(void (*f)(void *),const char *n,unsigned z,void *a,unsigned p,TaskHandle_t *h) {
    (void)f;(void)n;(void)a;(void)p;
    assert(z==10240 && !tasks_live && !owns_http && !http_live && !decoder_live && !allocated && !awake);
    if(fail_task_create) return 0;
    tasks_live++;task_creations++;*h=(void *)1;return pdPASS;
}
void vTaskDelete(TaskHandle_t h) {
    assert(h && tasks_live==1 && !owns_http && !http_live && !decoder_live && !allocated && !awake);
    tasks_live--;task_deletions++;
}
unsigned ulTaskNotifyTake(bool c,uint32_t t) { (void)c;(void)t;return 0; }
void xTaskNotifyGive(TaskHandle_t h) { (void)h; }
void vTaskDelay(uint32_t t) { (void)t; }
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t h) { (void)h;return 4096; }
unsigned esp_get_free_heap_size(void) { return 40000; }
size_t heap_caps_get_largest_free_block(uint32_t c) { (void)c;return 16000; }
esp_err_t heap_caps_register_failed_alloc_callback(esp_alloc_failed_hook_t h) { failed_hook=h;return ESP_OK; }
void runtime_log(const char *tag,const char *fmt,...) { (void)tag;(void)fmt; }
bool pf_network_ready(void) { return true; }
bool pf_music_url(unsigned track,char out[PF_URL]) { if(track)return false;strcpy(out,"https://example.com/a.mp3");return true; }
uint8_t pf_volume_get(void) { return 35; }
bool pf_radio_track(unsigned track,pf_track_t *out) { return pf_music_url(track,out->url); }
bool pf_http_take(uint32_t t) { (void)t;assert(!owns_http);owns_http=true;return true; }
void pf_http_give(void)
{
    /* A background fetch can run immediately after this handoff. */
    assert(owns_http && !http_live && !decoder_live && !allocated && !awake);
    owns_http=false;gives++;
}
void esp_crt_bundle_attach(void) {}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c) { (void)c;if(scenario==1)return NULL;http_live=true;return (void *)1; }
int esp_http_client_set_header(esp_http_client_handle_t h,const char *a,const char *b) { (void)h;(void)a;(void)b;return 0; }
int esp_http_client_open(esp_http_client_handle_t h,int n) { (void)h;(void)n;return scenario==2 ? ESP_FAIL : ESP_OK; }
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h) { (void)h;return 2048; }
int esp_http_client_get_status_code(esp_http_client_handle_t h) { (void)h;return 200; }
int esp_http_client_set_redirection(esp_http_client_handle_t h) { (void)h;return ESP_OK; }
int esp_http_client_close(esp_http_client_handle_t h) { (void)h;return ESP_OK; }
int esp_http_client_read(esp_http_client_handle_t h,char *p,int n) {
    (void)h;memset(p,0,n);if(scenario==4)return -1;
    if(scenario==6) { atomic_fetch_add(&s_generation,1);atomic_store(&s_requested,false); }
    return reads++ ? 0 : n;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h) { (void)h;return reads>1; }
int esp_http_client_cleanup(esp_http_client_handle_t h) { (void)h;assert(owns_http);http_live=false;return 0; }
int esp_audio_dec_register_default(void) { return ESP_AUDIO_ERR_OK; }
int esp_audio_simple_dec_register_default(void) { return ESP_AUDIO_ERR_OK; }
int esp_audio_simple_dec_open(const esp_audio_simple_dec_cfg_t *c,esp_audio_simple_dec_handle_t *h) { (void)c;if(scenario==3)return ESP_AUDIO_ERR_MEM_LACK;decoder_live=true;*h=(void *)1;return 0; }
int esp_audio_simple_dec_close(esp_audio_simple_dec_handle_t h) { (void)h;assert(owns_http);decoder_live=false;return 0; }
int esp_audio_simple_dec_process(esp_audio_simple_dec_handle_t h,esp_audio_simple_dec_raw_t *r,esp_audio_simple_dec_out_t *o)
{
    (void)h;
    if(scenario==5) { failed_hook(8192,MALLOC_CAP_8BIT,"calloc");return ESP_AUDIO_ERR_MEM_LACK; }
    r->consumed=r->len;o->decoded_size=8;return 0;
}
int esp_audio_simple_dec_get_info(esp_audio_simple_dec_handle_t h,esp_audio_simple_dec_info_t *i) { (void)h;*i=(esp_audio_simple_dec_info_t){16,2,48000};return 0; }
esp_err_t bsp_audio_init_playback(void) { awake=true;return ESP_OK; }
esp_err_t bsp_audio_wake(void) { awake=true;return ESP_OK; }
esp_err_t bsp_audio_sleep(void) { awake=false;return ESP_OK; }
esp_err_t bsp_audio_deinit(void) { if(fail_release)return ESP_FAIL;awake=false;return ESP_OK; }
esp_err_t bsp_audio_set_format(uint32_t r,uint8_t b,uint8_t c) { (void)r;(void)b;(void)c;return ESP_OK; }
void bsp_audio_set_volume(uint8_t v) { assert(v==35); }
esp_err_t bsp_audio_write(const void *p,size_t n) {
    (void)p;assert(n==4);writes++;
    if(scenario==7) { atomic_fetch_add(&s_generation,1);atomic_store(&s_requested,false); }
    return ESP_OK;
}
int main(void)
{
    assert(pf_player_init()==ESP_OK);
    assert(!s_worker && !tasks_live);
    for(unsigned round=0;round<20;round++) for(scenario=0;scenario<=5;scenario++) for(fail_at=0;fail_at<=2;fail_at++) {
        attempts=gives=reads=writes=0;
        assert((round%2 ? pf_player_radio(0) : pf_player_start(0))==ESP_OK);
        assert(s_worker && tasks_live==1);
        play(atomic_load(&s_generation),0,round%2);
        assert(gives==1 && !owns_http && !allocated && !http_live && !decoder_live && !awake);
        if(!scenario && !fail_at) {
            assert(s_status.state==PF_FINISHED && writes==1);
            pf_playback_t snapshot;
            pf_player_pause(); pf_player_snapshot(&snapshot); assert(snapshot.state==PF_PAUSED);
            pf_player_pause(); pf_player_snapshot(&snapshot); assert(snapshot.state==PF_FINISHED);
        }
        else assert(s_status.state==PF_ERROR);
        if(scenario==5 && !fail_at) assert(atomic_load(&s_failed_bytes)==8192);
        assert(pf_player_stop()==ESP_OK);
        assert(!s_worker && !tasks_live && s_status.state==PF_IDLE);
    }
    fail_at=0;
    for(unsigned round=0;round<20;round++) for(scenario=6;scenario<=7;scenario++) {
        attempts=gives=reads=writes=0;
        assert(pf_player_start(0)==ESP_OK);
        play(atomic_load(&s_generation),0,false);
        assert(gives==1 && !owns_http && !allocated && !http_live && !decoder_live && !awake);
        assert(s_status.state!=PF_FINISHED); // A cancelled track never advances the playlist.
        assert(pf_player_stop()==ESP_OK && !s_worker && !tasks_live);
    }
    assert(pf_player_start(0)==ESP_OK);
    atomic_store(&s_requested,false);
    gives=0;
    play(atomic_load(&s_generation),0,false); // Cancel before the worker acquires anything.
    assert(!gives && !allocated && !http_live && !decoder_live && !awake);
    assert(pf_player_stop()==ESP_OK && !tasks_live);
    assert(pf_player_stop()==ESP_OK); // Already stopped: no task to delete.
    fail_task_create=true;
    assert(pf_player_start(0)==ESP_ERR_NO_MEM && s_status.state==PF_ERROR);
    assert(!s_worker && !tasks_live);
    fail_task_create=false;
    assert(pf_player_start(0)==ESP_OK);
    idle_ack=false;
    assert(pf_player_stop()==ESP_ERR_TIMEOUT && s_status.state==PF_ERROR);
    assert(s_worker && tasks_live==1); // Never delete a producer before its acknowledgement.
    idle_ack=true;fail_release=true;
    assert(pf_player_stop()==ESP_FAIL && s_status.state==PF_ERROR);
    assert(s_worker && tasks_live==1);
    fail_release=false;
    assert(pf_player_stop()==ESP_OK && !s_worker && !tasks_live);
    assert(task_creations==task_deletions);
    puts("Player lifecycle: PASS (360 music/radio success/failure/retry paths, 41 cancellations, task reclamation, stop timeout/release recovery)");
}
