#include "update_checker.h"
#include "paths.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool update_trust_available(const char *path)
{
    struct stat info;
    if (stat(path, &info) || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
        info.st_size > UPDATE_TRUST_CAP)
        return false;
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    return fclose(file) == 0;
}

/* Only our published numeric version forms are accepted, not arbitrary URLs or text. */
static bool number(const char **text, unsigned *out)
{
    const char *start = *text;
    *out = 0;
    while (**text >= '0' && **text <= '9') {
        *out = *out * 10 + (*(*text)++ - '0');
        if (*out > 999999)
            return false;
    }
    return *text > start && (*text - start == 1 || *start != '0');
}
static bool version(const char *text, unsigned out[5])
{
    memset(out, 0, 5 * sizeof *out);
    if (!text || strlen(text) >= UPDATE_TAG_CAP || *text++ != 'v')
        return false;
    for (int i = 0; i < 3; i++) {
        if (!number(&text, &out[i]))
            return false;
        if (i < 2 && *text++ != '.')
            return false;
    }
    if (!*text) {
        out[3] = 2;
        return true;
    }
    if (!strncmp(text, "-preview.", 9))
        text += 9;
    else if (!strncmp(text, "-rc.", 4)) {
        out[3] = 1;
        text += 4;
    } else
        return false;
    return number(&text, &out[4]) && !*text;
}

int update_version_compare(const char *left, const char *right)
{
    unsigned a[5], b[5];
    if (!version(left, a) || !version(right, b))
        return 0;
    for (int i = 0; i < 5; i++)
        if (a[i] != b[i])
            return a[i] > b[i] ? 1 : -1;
    return 0;
}

/* Parse JSON structurally so a quoted release-note example cannot become a notice. */
static void spaces(const char **p)
{
    while (**p == ' ' || **p == '\r' || **p == '\n' || **p == '\t')
        (*p)++;
}
static bool string(const char **p, char *out, size_t capacity)
{
    if (*(*p)++ != '"')
        return false;
    size_t n = 0;
    bool escaped = false;
    while (**p && **p != '"') {
        unsigned char c = *(*p)++;
        if (c < 32)
            return false;
        if (c == '\\') {
            escaped = true;
            c = *(*p)++;
            if (!c || !strchr("\"\\/bfnrtu", c))
                return false;
            if (c == 'u')
                for (int i = 0; i < 4; i++) {
                    if (!isxdigit((unsigned char)**p))
                        return false;
                    (*p)++;
                }
        }
        if (out && n + 1 < capacity)
            out[n] = c;
        n++;
    }
    if (**p != '"')
        return false;
    (*p)++;
    if (out) {
        if (escaped || n >= capacity)
            out[0] = 0;
        else
            out[n] = 0;
    }
    return true;
}
static bool json_value(const char **p, unsigned depth, char tag[UPDATE_TAG_CAP], bool release)
{
    if (depth > 16)
        return false;
    spaces(p);
    if (**p == '"')
        return string(p, NULL, 0);
    if (**p == '{' || **p == '[') {
        bool object = **p == '{';
        char end = object ? '}' : ']';
        (*p)++;
        spaces(p);
        while (**p != end) {
            char key[32] = "";
            if (object) {
                if (!string(p, key, sizeof key))
                    return false;
                spaces(p);
                if (*(*p)++ != ':')
                    return false;
                spaces(p);
            }
            if (release && object && !strcmp(key, "tag_name")) {
                if (*tag || !string(p, tag, UPDATE_TAG_CAP))
                    return false;
            } else if (release && object && !strcmp(key, "draft")) {
                if (strncmp(*p, "false", 5))
                    return false;
                *p += 5;
            } else if (!json_value(p, depth + 1, tag, depth == 0 && !object))
                return false;
            spaces(p);
            if (**p == end)
                break;
            if (*(*p)++ != ',')
                return false;
            spaces(p);
            if (**p == end)
                return false;
        }
        (*p)++;
        return true;
    }
    const char *start = *p;
    while (**p && !strchr(",]} \t\r\n", **p))
        (*p)++;
    size_t length = *p - start;
    if ((length == 4 && (!memcmp(start, "true", 4) || !memcmp(start, "null", 4))) ||
        (length == 5 && !memcmp(start, "false", 5)))
        return true;
    if (!length || length > 32)
        return false;
    char number[33], *end;
    memcpy(number, start, length);
    number[length] = 0;
    strtod(number, &end);
    return end == number + length && (isdigit((unsigned char)*number) || *number == '-');
}

bool update_response_tag(char *response, size_t size, char tag[UPDATE_TAG_CAP])
{
    tag[0] = 0;
    if (!size || size >= UPDATE_RESPONSE_CAP || memchr(response, 0, size))
        return false;
    response[size] = 0;
    if (strncmp(response, "HTTP/1.1 200 ", 13) && strncmp(response, "HTTP/1.0 200 ", 13))
        return false;
    char *body = strstr(response, "\r\n\r\n");
    if (!body || body - response > 8192)
        return false;
    *body = 0;
    body += 4;
    size_t body_size = size - (body - response);
    for (char *line = strstr(response, "\r\n"); line; line = strstr(line, "\r\n")) {
        line += 2;
        if (!strncasecmp(line, "Transfer-Encoding:", 18))
            return false; /* Request HTTP/1.0; do not guess at unknown framing. */
        if (!strncasecmp(line, "Content-Encoding:", 17) && strncasecmp(line + 17, " identity", 9))
            return false;
        if (!strncasecmp(line, "Content-Length:", 15)) {
            char *end;
            unsigned long length = strtoul(line + 15, &end, 10);
            if (length != body_size || (*end && *end != '\r'))
                return false;
        }
    }
    const char *p = body;
    spaces(&p);
    if (*p != '[' || !json_value(&p, 0, tag, false))
        return false;
    spaces(&p);
    unsigned parsed[5];
    if (*p || !version(tag, parsed)) {
        tag[0] = 0;
        return false;
    }
    return true;
}

bool update_should_start(UpdateState *state, unsigned frames, unsigned idle_frames, bool allowed,
                         time_t now)
{
    if (!allowed || !state->enabled || state->busy || frames < 1800 || idle_frames < 120)
        return false;
    if (!state->requested &&
        (state->attempted ||
         (state->checked_at > 0 && now >= state->checked_at && now - state->checked_at < 86400)))
        return false;
    state->attempted = state->busy = true;
    state->requested = false;
    snprintf(state->status, sizeof state->status, "Checking releases...");
    return true;
}
void update_cache_load(UpdateState *state, const char *root)
{
    state->cache_loaded = true;
    char path[PATH_CAP];
    if (!path_join(path, root, "_nds/dsi-frontend/update-cache.txt"))
        return;
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    char tag[UPDATE_TAG_CAP], stamp[32], extra;
    int fields = fscanf(f, "%47s %31s %c", tag, stamp, &extra);
    fclose(f);
    if (fields != 2)
        return;
    char *end;
    errno = 0;
    long long timestamp = strtoll(stamp, &end, 10);
    unsigned parsed[5];
    if (!errno && !*end && version(tag, parsed) && timestamp > 0 && timestamp <= 4102444800LL) {
        snprintf(state->latest, sizeof state->latest, "%s", tag);
        state->checked_at = (time_t)timestamp;
        state->available = update_version_compare(tag, GLOAM_VERSION) > 0;
        snprintf(state->status, sizeof state->status, "%s",
                 state->available ? "Update available" : "Last check: up to date");
    }
}
void update_complete(UpdateState *state, const char *root, const char *tag, time_t now)
{
    state->busy = state->connected = false;
    unsigned parsed[5];
    if (!tag || !version(tag, parsed)) {
        snprintf(state->status, sizeof state->status, "Check unavailable; try later");
        return; /* Offline failure must not discard an existing notification. */
    }
    snprintf(state->latest, sizeof state->latest, "%s", tag);
    state->available = update_version_compare(tag, GLOAM_VERSION) > 0;
    state->checked_at = now;
    snprintf(state->status, sizeof state->status, "%s",
             state->available ? "Update available" : "Up to date");
    char path[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP];
    if (!path_join(path, root, "_nds/dsi-frontend/update-cache.txt") ||
        !path_join(temporary, root, "_nds/dsi-frontend/update-cache.tmp") ||
        !path_join(backup, root, "_nds/dsi-frontend/update-cache.bak"))
        return;
    FILE *f = fopen(temporary, "w");
    if (!f)
        return;
    bool written = fprintf(f, "%s\n%lld\n", tag, (long long)now) > 0;
    if (fclose(f))
        written = false;
    if (written)
        path_commit_file(temporary, path, backup);
}
