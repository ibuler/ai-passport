/* Real feed task: cooperative stop, late acknowledgement and deferred refresh. */
#include "voice_runtime.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define eSetBits 1
int xTaskNotify(TaskHandle_t,uint32_t,int);
int xTaskNotifyWait(uint32_t,uint32_t,uint32_t *,uint32_t);
size_t heap_caps_get_free_size(uint32_t);
const char *esp_err_to_name(esp_err_t);
#include "../../main/pocket/feeds.c"
static unsigned bits, tasks, notices;
static bool fail_create, late, stopping_during_idle;
static jmp_buf suspended;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
int xSemaphoreTake(SemaphoreHandle_t h,uint32_t t) { (void)h;(void)t;return 1; }
int xSemaphoreGive(SemaphoreHandle_t h) { (void)h;return 1; }
EventGroupHandle_t xEventGroupCreate(void) { return (void *)1; }
unsigned xEventGroupSetBits(EventGroupHandle_t e,unsigned b) { (void)e;return bits|=b; }
unsigned xEventGroupClearBits(EventGroupHandle_t e,unsigned b) { (void)e;return bits&=~b; }
unsigned xEventGroupWaitBits(EventGroupHandle_t e,unsigned b,bool c,bool a,uint32_t t) { (void)e;(void)c;(void)a;(void)t;return bits&b; }
int xTaskCreate(void (*f)(void *),const char *n,unsigned z,void *a,unsigned p,TaskHandle_t *out) { (void)n;(void)a;(void)p;assert(f==worker && z==8192 && !tasks);if(fail_create)return 0;tasks++;*out=(void *)1;return pdPASS; }
void vTaskDelete(TaskHandle_t t) { assert(t && tasks && (bits&FEEDS_DONE));tasks--; }
void vTaskSuspend(TaskHandle_t t) { (void)t;assert(bits&FEEDS_DONE);longjmp(suspended,1); }
int xTaskNotifyWait(uint32_t a,uint32_t b,uint32_t *n,uint32_t t) { (void)a;(void)b;(void)t;*n=0;if(stopping_during_idle)atomic_store(&s_stop,true);return 1; }
int xTaskNotify(TaskHandle_t t,uint32_t n,int a) { (void)t;(void)n;(void)a;notices++;if(!late && atomic_load(&s_stop) && setjmp(suspended)==0)worker(NULL);return 1; }
bool pf_settings_active(void) { return false; }
bool pf_xz_active(void) { return false; }
void pf_player_snapshot(pf_playback_t *p) { memset(p,0,sizeof(*p)); }
bool pf_http_take(uint32_t t) { (void)t;assert(false);return false; }
void pf_http_give(void) { assert(false); }
void pf_profile_get(pf_profile_t *p) { memset(p,0,sizeof(*p)); }
void pf_github_username_get(char *p) { *p=0; }
bool pf_network_ready(void) { return false; }
int64_t esp_timer_get_time(void) { return 0; }
size_t heap_caps_get_free_size(uint32_t c) { (void)c;return 80000; }
size_t heap_caps_get_largest_free_block(uint32_t c) { return heap_caps_get_free_size(c); }
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t t) { (void)t;return 4096; }
const char *esp_err_to_name(esp_err_t e) { (void)e;return "test"; }
void runtime_log(const char *t,const char *f,...) { (void)t;(void)f; }
void esp_crt_bundle_attach(void) {}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c) { (void)c;assert(false);return NULL; }
int esp_http_client_open(esp_http_client_handle_t h,int s) { (void)h;(void)s;return ESP_FAIL; }
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h) { (void)h;return 0; }
int esp_http_client_get_status_code(esp_http_client_handle_t h) { (void)h;return 0; }
int esp_http_client_read(esp_http_client_handle_t h,char *p,int n) { (void)h;(void)p;(void)n;return 0; }
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h) { (void)h;return false; }
int esp_http_client_cleanup(esp_http_client_handle_t h) { (void)h;return ESP_OK; }
int main(void)
{
 assert(pf_weather_init()==ESP_OK);
 for(unsigned i=0;i<100;i++) {
  pf_weather_refresh();pf_radio_refresh();pf_github_refresh();
  s_weather.valid=true;s_radio.count=1;s_github.valid=true;
  late=true;assert(pf_feeds_pause()==ESP_ERR_TIMEOUT && tasks==1 && s_task);
  unsigned notified=notices;
  if(setjmp(suspended)==0)worker(NULL); /* acknowledgement after caller timed out */
  assert(bits&FEEDS_DONE);
  assert(pf_feeds_pause()==ESP_OK && !tasks && !s_task && notices==notified);
  pf_weather_refresh();assert(atomic_load(&s_deferred)==(WEATHER|RADIO|GITHUB));
  fail_create=true;assert(pf_feeds_resume()==ESP_ERR_NO_MEM && !tasks);
  fail_create=false;assert(pf_feeds_resume()==ESP_OK && tasks==1);
  assert(s_weather.valid && s_radio.count==1 && s_github.valid);
  late=false;assert(pf_feeds_pause()==ESP_OK && !tasks);
  assert(pf_feeds_resume()==ESP_OK);
  late=true;assert(pf_feeds_pause()==ESP_ERR_TIMEOUT);
  if(setjmp(suspended)==0)worker(NULL);
  assert(pf_feeds_resume()==ESP_OK && tasks==1 && !atomic_load(&s_stop));
 }
 late=false;assert(pf_feeds_pause()==ESP_OK && !tasks);
 puts("Feed lifecycle: PASS (100 stop/late-ACK/resume/OOM cycles; refresh and caches retained)");
}
