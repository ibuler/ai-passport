#pragma once
#include "model.h"
/* Incremental public-calendar parser: bounded memory independent of HTML size. */
typedef struct {
    pf_github_t calendar;
    int64_t today;
    char tag[512];
    size_t used;
    bool in_tag, overflow, failed;
} pf_github_parser_t;
void pf_github_parser_init(pf_github_parser_t *parser, int64_t today, const char *username);
void pf_github_parser_feed(pf_github_parser_t *parser, const char *data, size_t bytes);
bool pf_github_parser_finish(pf_github_parser_t *parser, pf_github_t *out);
void pf_github_default(pf_github_t *out);
