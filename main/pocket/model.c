#include "model.h"
#include <string.h>

void pf_defaults(pf_config_t *c)
{
    memset(c, 0, sizeof(*c));
    c->version = 1;
    strcpy(c->name, "你的名字");
    c->volume = 35;
    c->count = 1;
    strcpy(c->tracks[0].title, "79: Corticyte");
    strcpy(c->tracks[0].url, "https://datashat.net/music_for_programming_79-corticyte.mp3");
}

bool pf_text_valid(const char *text, size_t capacity, bool allow_empty)
{
    if (!text || !capacity) return false;
    size_t n = 0;
    while (n < capacity && text[n]) n++;
    if (n == capacity || (!n && !allow_empty)) return false;
    for (size_t i = 0; i < n;) {
        uint8_t c = (uint8_t)text[i++];
        if (c < 0x20 || c == 0x7f) return false;
        if (c < 0x80) continue;
        unsigned extra;
        uint32_t point, minimum;
        if (c >= 0xc2 && c <= 0xdf) { extra = 1; point = c & 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { extra = 2; point = c & 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { extra = 3; point = c & 7; minimum = 0x10000; }
        else return false;
        if (i + extra > n) return false;
        while (extra--) {
            c = (uint8_t)text[i++];
            if ((c & 0xc0) != 0x80) return false;
            point = (point << 6) | (c & 63);
        }
        if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) return false;
    }
    return true;
}

bool pf_url_valid(const char *url)
{
    if (!pf_text_valid(url, PF_URL, false)) return false;
    const char *host = NULL;
    if (!strncmp(url, "https://", 8)) host = url + 8;
    else if (!strncmp(url, "http://", 7)) host = url + 7;
    else return false;
    if (!*host || *host == '/' || *host == ':' || *host == '?' || *host == '#') return false;
    const char *end = host + strcspn(host, "/?#");
    for (const char *p = url; *p; p++) if ((unsigned char)*p <= 0x20 || (unsigned char)*p >= 0x7f || *p == '\\') return false;
    for (const char *p = host; p < end; p++) if (*p == '@') return false;
    return true;
}

bool pf_config_valid(const pf_config_t *c)
{
    if (c->version != 1 || c->volume > 100 || c->count > PF_TRACKS || !pf_text_valid(c->name, PF_TITLE, false)) return false;
    for (unsigned i = 0; i < c->count; i++)
        if (!pf_text_valid(c->tracks[i].title, PF_TITLE, false) || !pf_url_valid(c->tracks[i].url)) return false;
    return true;
}

unsigned pf_track_step(unsigned current, int direction, unsigned count)
{
    if (!count) return 0;
    if (current >= count) current = 0;
    if (direction < 0) return (current + count - 1) % count;
    return (current + 1) % count;
}

uint8_t pf_volume_step(uint8_t current, int direction)
{
    int next = (current > 100 ? 100 : current) + (direction > 0 ? 5 : direction < 0 ? -5 : 0);
    return next < 0 ? 0 : next > 100 ? 100 : next;
}

uint32_t pf_crc32(uint32_t crc, const void *data, size_t length)
{
    const uint8_t *p = data;
    crc = ~crc;
    while (length--) {
        crc ^= *p++;
        for (unsigned i = 0; i < 8; i++) crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xedb88320u);
    }
    return ~crc;
}

static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
bool pf_image_header(const uint8_t h[16], size_t bytes, pf_image_info_t *info)
{
    if (memcmp(h, "PFI1", 4) || le16(h + 4) != PF_IMAGE_W || le16(h + 6) != PF_IMAGE_H) return false;
    unsigned frames = le16(h + 8), flags = le16(h + 10);
    if (!frames || frames > PF_IMAGE_FRAMES || flags > 3 || (!(flags & 2) && frames != 1)) return false;
    if (bytes != 16 + frames * (2 + PF_FRAME_BYTES)) return false;
    info->frames = frames;
    info->flags = flags;
    info->crc = (uint32_t)h[12] | (uint32_t)h[13] << 8 | (uint32_t)h[14] << 16 | (uint32_t)h[15] << 24;
    return true;
}
bool pf_delay_valid(uint16_t milliseconds) { return milliseconds >= 100 && milliseconds <= 2000; }

void pf_profile_defaults(pf_profile_t *p)
{
    memset(p, 0, sizeof(*p)); p->version = 1; p->utc_minutes = 480;
}
bool pf_profile_valid(const pf_profile_t *p)
{
    if (!p || p->version != 1 || !pf_text_valid(p->company, PF_TITLE, true) ||
        !pf_text_valid(p->city, PF_CITY, true) || p->latitude_e4 < -900000 || p->latitude_e4 > 900000 ||
        p->longitude_e4 < -1800000 || p->longitude_e4 > 1800000 ||
        p->utc_minutes < -720 || p->utc_minutes > 840 || p->radio_count > PF_RADIOS) return false;
    for (unsigned i = 0; i < p->radio_count; i++)
        if (!pf_text_valid(p->radios[i].city, PF_CITY, false) || !pf_text_valid(p->radios[i].track.title, PF_TITLE, false) || !pf_url_valid(p->radios[i].track.url)) return false;
    return true;
}
unsigned pf_radio_count(const pf_profile_t *p)
{
    unsigned count = 0;
    for (unsigned i = 0; i < p->radio_count; i++) if (p->city[0] && !strcmp(p->city, p->radios[i].city)) count++;
    return count;
}
unsigned pf_radio_step(const pf_profile_t *p, unsigned current, int direction)
{
    if (!p->radio_count || !p->city[0]) return PF_RADIOS;
    if (current >= p->radio_count) current = direction < 0 ? 0 : p->radio_count - 1;
    for (unsigned n = 0; n < p->radio_count; n++) {
        current = (current + p->radio_count + (direction < 0 ? -1 : 1)) % p->radio_count;
        if (!strcmp(p->city, p->radios[current].city)) return current;
    }
    return PF_RADIOS;
}
uint64_t pf_timer_elapsed(const pf_timer_t *t, uint64_t now)
{
    return t->elapsed_ms + (t->running && now >= t->started_ms ? now - t->started_ms : 0);
}
void pf_timer_toggle(pf_timer_t *t, uint64_t now)
{
    t->elapsed_ms = pf_timer_elapsed(t, now); t->started_ms = now; t->running = !t->running;
}
void pf_timer_reset(pf_timer_t *t) { memset(t, 0, sizeof(*t)); }
const char *pf_weather_description(int code)
{
    if (code == 0) return "晴";
    if (code == 1 || code == 2) return "多云";
    if (code == 3) return "阴";
    if (code == 45 || code == 48) return "雾";
    if (code >= 51 && code <= 57) return "毛毛雨";
    if (code >= 61 && code <= 67) return "雨";
    if (code >= 71 && code <= 77) return "雪";
    if (code >= 80 && code <= 82) return "阵雨";
    if (code == 85 || code == 86) return "阵雪";
    if (code >= 95 && code <= 99) return "雷雨";
    return "天气未知";
}

pf_weather_icon_t pf_weather_icon(int code)
{
    if (code == 0) return PF_ICON_SUN;
    if (code == 1 || code == 2) return PF_ICON_PARTLY;
    if (code == 3) return PF_ICON_CLOUD;
    if (code == 45 || code == 48) return PF_ICON_FOG;
    if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return PF_ICON_RAIN;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return PF_ICON_SNOW;
    if (code == 95 || code == 96 || code == 99) return PF_ICON_THUNDER;
    return PF_ICON_UNKNOWN;
}

bool pf_json_depth_valid(const char *body, size_t bytes)
{
    unsigned depth = 0; bool quoted = false, escaped = false;
    for (size_t i = 0; i < bytes; i++) {
        char c = body[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') { if (++depth > 16) return false; }
        else if (c == '}' || c == ']') { if (!depth) return false; depth--; }
    }
    return !quoted && !depth;
}
