#ifndef GLOAM_UPDATE_CHECKER_H
#define GLOAM_UPDATE_CHECKER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#define GLOAM_VERSION "v0.1.0-preview.23"
#define UPDATE_RESPONSE_CAP (96 * 1024)
#define UPDATE_TAG_CAP 48
#define UPDATE_TRUST_CAP (64 * 1024)
#define UPDATE_RELEASES_URL "https://github.com/nextcode4u/gloam-shell-ndx/releases"
typedef struct {
    bool enabled, requested, attempted, cache_loaded, busy, available, connected;
    char latest[UPDATE_TAG_CAP];
    char status[80];
    time_t checked_at;
} UpdateState;
/* A bounded, read-only release notice. Never downloads or installs executable data. */
int update_version_compare(const char *left, const char *right);
bool update_response_tag(char *response, size_t size, char tag[UPDATE_TAG_CAP]);
/* Cheap preflight before initializing the radio; TLS still validates the PEM. */
bool update_trust_available(const char *path);
bool update_should_start(UpdateState *state, unsigned frames, unsigned idle_frames, bool allowed,
                         time_t now);
void update_cache_load(UpdateState *state, const char *root);
void update_complete(UpdateState *state, const char *root, const char *tag, time_t now);
#endif
