#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { PF_XZ_OFF, PF_XZ_CONNECTING, PF_XZ_ACTIVATING, PF_XZ_READY,
               PF_XZ_LISTENING, PF_XZ_THINKING, PF_XZ_SPEAKING, PF_XZ_ERROR } pf_xz_state_t;
typedef enum { PF_XZ_NEUTRAL, PF_XZ_HAPPY, PF_XZ_SAD, PF_XZ_SURPRISED } pf_xz_emotion_t;
typedef struct {
    pf_xz_state_t state;
    pf_xz_emotion_t emotion;
    uint32_t pcm_at;
    char message[96], text[256], code[32];
} pf_xz_snapshot_t;
typedef enum { PF_XZ_IGNORE, PF_XZ_HELLO, PF_XZ_TTS_START, PF_XZ_TTS_STOP,
               PF_XZ_TEXT, PF_XZ_STT, PF_XZ_EMOTION, PF_XZ_GOODBYE } pf_xz_event_type_t;
typedef struct {
    pf_xz_event_type_t type;
    unsigned rate, duration;
    bool udp;
    char session[80], text[256];
    pf_xz_emotion_t emotion;
} pf_xz_event_t;

/* Protocol parsers are bounded and independent of ESP-IDF/LVGL. */
bool pf_xz_parse(const char *data, size_t bytes, pf_xz_event_t *out);
bool pf_xz_binary(unsigned version, const uint8_t *data, size_t bytes,
                  const uint8_t **payload, size_t *length);
bool pf_xz_udp(const uint8_t *data, size_t bytes, uint32_t previous, uint32_t *sequence);
bool pf_xz_hex16(const char *text, uint8_t out[16]);
unsigned pf_xz_portrait(const pf_xz_snapshot_t *state, uint32_t now);
