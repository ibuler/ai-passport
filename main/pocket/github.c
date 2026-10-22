#include "github.h"
#include <string.h>

bool pf_github_username_valid(const char *name)
{
    if (!name) return false;
    size_t n = 0;
    for (; n < PF_GITHUB_USER && name[n]; n++) {
        char c = name[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || (c == '-' && n && name[n-1] != '-'))) return false;
    }
    return n < PF_GITHUB_USER && (!n || name[n-1] != '-');
}
static bool space(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }
static bool attribute(const char *tag, const char *key, char *out, size_t cap)
{
    size_t n = strlen(key);
    for (const char *p = tag; (p = strstr(p, key)); p += n) {
        if (p == tag || !space(p[-1]) || p[n] != '=') continue;
        p += n + 1;
        char quote = *p++;
        if (quote != '\'' && quote != '"') return false;
        const char *end = strchr(p, quote);
        if (!end || (size_t)(end-p) >= cap) return false;
        memcpy(out, p, end-p); out[end-p] = 0; return true;
    }
    return false;
}
static bool date_day(const char *s, int64_t *out)
{
    if (strlen(s) != 10 || s[4] != '-' || s[7] != '-') return false;
    for (unsigned i = 0; i < 10; i++) if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
    int y = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + s[3]-'0';
    int m = (s[5]-'0')*10 + s[6]-'0', d = (s[8]-'0')*10 + s[9]-'0';
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > days[m-1] + (m == 2 && (!(y%400) || (!(y%4) && y%100)))) return false;
    y -= m <= 2;
    int era = y / 400, yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    *out = (int64_t)era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    return true;
}
static void day_tag(pf_github_parser_t *p)
{
    if (strncmp(p->tag, "td", 2) || !space(p->tag[2])) return;
    char date[16], level[8];
    if (!attribute(p->tag, "data-date", date, sizeof(date))) return;
    int64_t day;
    if (!date_day(date, &day) || !attribute(p->tag, "data-level", level, sizeof(level)) ||
        strlen(level) != 1 || level[0] < '0' || level[0] > '4') { p->failed = true; return; }
    if (day < p->calendar.first_day || day > p->today) return;
    unsigned i = (unsigned)(day - p->calendar.first_day);
    if (i >= PF_GITHUB_DAYS) { p->failed = true; return; }
    uint8_t v = level[0] - '0';
    if (p->calendar.levels[i] != 255 && p->calendar.levels[i] != v) p->failed = true;
    p->calendar.levels[i] = v;
}
void pf_github_parser_init(pf_github_parser_t *p, int64_t today, const char *username)
{
    memset(p, 0, sizeof(*p));
    p->today = today;
    p->failed = today < 19723 || today > 47846 || !pf_github_username_valid(username) || !username[0];
    if (!p->failed) strcpy(p->calendar.username, username);
    p->calendar.first_day = today - (today + 4) % 7 - (PF_GITHUB_WEEKS - 1) * 7;
    memset(p->calendar.levels, 255, sizeof(p->calendar.levels));
}
void pf_github_parser_feed(pf_github_parser_t *p, const char *data, size_t bytes)
{
    for (size_t i = 0; i < bytes && !p->failed; i++) {
        char c = data[i];
        if (c == '<') { p->in_tag = true; p->overflow = false; p->used = 0; }
        else if (p->in_tag && c == '>') {
            p->tag[p->used] = 0;
            if (!p->overflow) day_tag(p);
            p->in_tag = false;
        } else if (p->in_tag) {
            if (p->used + 1 < sizeof(p->tag)) p->tag[p->used++] = c;
            else p->overflow = true;
        }
    }
}
bool pf_github_parser_finish(pf_github_parser_t *p, pf_github_t *out)
{
    if (p->failed || p->in_tag) return false;
    for (int64_t d = p->calendar.first_day; d <= p->today; d++)
        if (p->calendar.levels[d-p->calendar.first_day] > 4) return false;
    p->calendar.valid = true; *out = p->calendar; return true;
}

/* Public ibuler calendar snapshot, 2026-10-22; display-only default, not live statistics. */
void pf_github_default(pf_github_t *out)
{
    static const uint8_t levels[PF_GITHUB_DAYS] = {
        0,2,1,2,1,0,0,0,0,0,1,1,1,1,0,2,0,1,1,0,0,0,1,2,1,1,
        0,0,0,0,1,1,0,1,0,0,1,1,1,1,1,0,0,1,1,1,1,1,0,0,2,1,
        1,1,0,0,0,1,1,1,1,1,1,0,1,1,2,1,1,0,0,1,1,4,2,3,0,0,
        4,3,2,3,2,0,0,0,2,1,3,1,0,0,1,2,3,1,0,0,0,1,3,1,2,2,
        1,0,2,1,2,2,1,0,0,1,2,1,2,1,0,0,1,2,2,2,2,0,0,1,2,3,
        2,2,0,1,2,4,4,4,1,1,0,3,3,3,1,3,0,4,1,3,2,0,0,1,1,1,
        1,0,1,0,0,0,0,0,0,3,1,3,0,1,1,2,2,1,0,0,2,2,1,0,255,255,
    };
    memset(out,0,sizeof(*out)); strcpy(out->username,"ibuler");
    out->valid=true; out->stale=true; out->first_day=20569;
    memcpy(out->levels,levels,sizeof(levels));
}
