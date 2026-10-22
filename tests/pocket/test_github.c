#include "pocket/github.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char html[65536];
static const int64_t today = 20748; /* 2026-10-22 UTC, Thursday. */
static size_t fixture(void)
{
    pf_github_parser_t p; pf_github_parser_init(&p,today,"ibuler");
    size_t used = 0;
    /* Real calendars are in weekday/column order, not chronological order. */
    for (unsigned row=0;row<7;row++) for (unsigned col=0;col<PF_GITHUB_WEEKS;col++) {
        unsigned i = col*7+row; int64_t day = p.calendar.first_day+i;
        if (day>today) continue;
        time_t epoch = day*86400; struct tm *tm = gmtime(&epoch);
        char date[16]; assert(strftime(date,sizeof(date),"%Y-%m-%d",tm));
        used += snprintf(html+used,sizeof(html)-used,"<td\n data-level='%u' data-date=\"%s\" class='ContributionCalendar-day'></td>\n",i%5,date);
    }
    return used;
}
static void parse_fixture(size_t bytes, size_t chunk)
{
    pf_github_parser_t p; pf_github_t g; pf_github_parser_init(&p,today,"ibuler");
    for (size_t at=0;at<bytes;at+=chunk) pf_github_parser_feed(&p,html+at,bytes-at<chunk ? bytes-at : chunk);
    assert(pf_github_parser_finish(&p,&g) && g.valid);
    assert((g.first_day+4)%7==0 && !strcmp(g.username,"ibuler"));
    for (unsigned i=0;i<PF_GITHUB_DAYS;i++) assert(g.levels[i]==(g.first_day+i<=today ? i%5 : 255));
}
static void rejected(const char *body)
{
    pf_github_parser_t p; pf_github_t g; pf_github_parser_init(&p,today,"ibuler");
    pf_github_parser_feed(&p,body,strlen(body)); assert(!pf_github_parser_finish(&p,&g));
}
int main(int argc, char **argv)
{
    pf_github_t fallback;pf_github_default(&fallback);unsigned filled=0;
    assert(fallback.valid && fallback.stale);
    for(unsigned i=0;i<PF_GITHUB_DAYS;i++) {
        assert(fallback.levels[i]<=4 || fallback.levels[i]==255);
        if(fallback.levels[i]>0 && fallback.levels[i]<=4)filled++;
    }
    assert(filled>0); /* No-network home must still have a nonempty default graphic. */
    assert(pf_github_username_valid("ibuler") && pf_github_username_valid("A-b-12") && pf_github_username_valid(""));
    const char *bad[]={NULL,"-a","a-","a--b","a_b","a/b","a?b","中文","a\n","https://github.com/a"};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) assert(!pf_github_username_valid(bad[i]));
    char max[41]; memset(max,'a',40);max[40]=0;assert(!pf_github_username_valid(max));max[39]=0;assert(pf_github_username_valid(max));
    size_t bytes=fixture(); parse_fixture(bytes,1);parse_fixture(bytes,7);parse_fixture(bytes,1024);
    rejected("<html>Sign in</html>");
    rejected("<td data-date='2026-02-30' data-level='1'>");
    rejected("<td data-date='2026-10-22' data-level='9'>");
    rejected("<td data-date='2026-10-22' data-level='3'>"); /* Incomplete calendar. */
    pf_github_parser_t p;pf_github_t g;pf_github_parser_init(&p,today,"ibuler");
    pf_github_parser_feed(&p,html,bytes);const char *conflict="<td data-date='2026-10-22' data-level='0'>";
    pf_github_parser_feed(&p,conflict,strlen(conflict));assert(!pf_github_parser_finish(&p,&g));
    pf_github_parser_init(&p,today,"ibuler");pf_github_parser_feed(&p,html,bytes);
    pf_github_parser_feed(&p,"<td",3);assert(!pf_github_parser_finish(&p,&g));
    pf_github_parser_init(&p,today,"bad/id");assert(!pf_github_parser_finish(&p,&g));
    pf_github_parser_init(&p,0,"ibuler");assert(!pf_github_parser_finish(&p,&g));
    if (argc==3) {
        FILE *f=fopen(argv[1],"rb");assert(f);char chunk[1024];size_t n;
        pf_github_parser_init(&p,strtoll(argv[2],NULL,10),"ibuler");
        while((n=fread(chunk,1,sizeof(chunk),f))) pf_github_parser_feed(&p,chunk,n);
        fclose(f);assert(pf_github_parser_finish(&p,&g));
        unsigned active=0;for(unsigned i=0;i<PF_GITHUB_DAYS;i++) if(g.levels[i]>0&&g.levels[i]<5)active++;
        printf("Live public calendar parsed: %u active days in displayed window\n",active);
    }
    puts("GitHub parser: PASS (chunk boundaries, weekday order, identity, invalid/truncated calendars)");
}
