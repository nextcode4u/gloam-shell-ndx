#include "update_checker.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char response[UPDATE_RESPONSE_CAP];
static bool parse(const char *json, char tag[UPDATE_TAG_CAP])
{
    int size = snprintf(response, sizeof response,
                        "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n%s", strlen(json), json);
    return update_response_tag(response, size, tag);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(update_version_compare("v0.1.0-preview.23", "v0.1.0-preview.22") > 0);
    assert(update_version_compare("v0.1.0-preview.9", "v0.1.0-preview.10") < 0);
    assert(update_version_compare("v0.1.0-rc.1", "v0.1.0-preview.999") > 0);
    assert(update_version_compare("v0.1.0", "v0.1.0-rc.999") > 0);
    assert(update_version_compare("v0.1.0-preview.-1", GLOAM_VERSION) == 0);
    assert(update_version_compare("v999999999999999999999.1.0", GLOAM_VERSION) == 0);
    assert(update_version_compare("v01.1.0", GLOAM_VERSION) == 0);
    char tag[UPDATE_TAG_CAP];
    const char *good =
        "[{\"body\":\"Example: \\\"tag_name\\\": \\\"v999.0.0\\\"\","
        "\"assets\":[{\"name\":\"BOOT.NDS\"}],\"tag_name\":\"v0.1.0-preview.23\",\"draft\":false}]";
    assert(parse(good, tag) && !strcmp(tag, "v0.1.0-preview.23"));
    const char *bad[] = {"[]",
                         "[{}]",
                         "[",
                         "[{\"tag_name\":\"v1.0.0\",\"draft\":true}]",
                         "[{\"tag_name\":\"v1.0.0\",\"tag_name\":\"v2.0.0\"}]",
                         "[{\"tag_name\":\"v1.0.0\"}] trailing",
                         "[{\"tag_name\":\"v1.0.0\",}]",
                         "[{\"tag_name\":\"../../BOOT.NDS\"}]",
                         "[{\"body\":{\"tag_name\":\"v1.0.0\"}}]"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++)
        assert(!parse(bad[i], tag));
    for (size_t i = 0; i < strlen(good); i++) {
        char partial[512];
        memcpy(partial, good, i);
        partial[i] = 0;
        assert(!parse(partial, tag));
    }
    assert(parse(good, tag));
    size_t size = strlen(response); /* parser splits headers; build a fresh response below */
    size = snprintf(response, sizeof response, "HTTP/1.1 302 Found\r\n\r\n%s", good);
    assert(!update_response_tag(response, size, tag));
    size =
        snprintf(response, sizeof response, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n%s", good);
    assert(!update_response_tag(response, size, tag));
    size = snprintf(response, sizeof response,
                    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n%s", good);
    assert(!update_response_tag(response, size, tag));
    UpdateState state = {.enabled = true};
    assert(!update_should_start(&state, 1799, 120, true, 100000));
    assert(!update_should_start(&state, 1800, 119, true, 100000));
    assert(!update_should_start(&state, 1800, 120, false, 100000));
    assert(update_should_start(&state, 1800, 120, true, 100000));
    assert(!update_should_start(&state, 1801, 120, true, 100000));
    update_complete(&state, argv[1], "v999.0.0", 100000);
    assert(state.available && !state.busy);
    UpdateState restored = {.enabled = true};
    update_cache_load(&restored, argv[1]);
    assert(restored.available && restored.checked_at == 100000);
    assert(!update_should_start(&restored, 2000, 200, true, 100001));
    restored.requested = true;
    assert(update_should_start(&restored, 2000, 200, true, 100001));
    update_complete(&restored, argv[1], NULL, 100002);
    assert(restored.available && restored.checked_at == 100000 && !restored.busy);
    update_complete(&restored, argv[1], GLOAM_VERSION, 100003);
    assert(!restored.available);
    restored.enabled = false;
    restored.requested = true;
    assert(!update_should_start(&restored, 2000, 200, true, 100004));
    char cache[1024];
    snprintf(cache, sizeof cache, "%s/_nds/dsi-frontend/update-cache.txt", argv[1]);
    FILE *file = fopen(cache, "w");
    assert(file);
    fputs("v999.0.0\n999999999999999999999999999999\n", file);
    fclose(file);
    UpdateState invalid_cache = {0};
    update_cache_load(&invalid_cache, argv[1]);
    assert(invalid_cache.cache_loaded && !invalid_cache.available && !invalid_cache.checked_at);
    puts("PASS: bounded release parsing, numeric version ordering, startup/idle gates, daily "
         "cache, cancellation state and stale notice preservation");
}
