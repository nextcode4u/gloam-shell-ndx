/* Shared backend payload ownership. Foreign active cheats are never removed. */
#include "cheats.h"
#include "paths.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool file_equal(const char *left, const char *right)
{
    FILE *a = fopen(left, "rb"), *b = fopen(right, "rb");
    bool equal = a && b;
    size_t total = 0;
    unsigned char first[1024], second[1024];
    while (equal) {
        size_t x = fread(first, 1, sizeof first, a), y = fread(second, 1, sizeof second, b);
        total += x;
        if (total > CHEAT_PAYLOAD_CAP || x != y || memcmp(first, second, x)) {
            equal = false;
            break;
        }
        if (x < sizeof first) {
            equal = !ferror(a) && !ferror(b);
            break;
        }
    }
    if (a)
        fclose(a);
    if (b)
        fclose(b);
    return equal;
}
static bool empty_payload(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return errno == ENOENT;
    unsigned char bytes[5];
    size_t count = fread(bytes, 1, sizeof bytes, file);
    bool valid = !ferror(file) && (count == 0 || (count == 4 && !memcmp(bytes, "\0\0\0\xCF", 4)));
    fclose(file);
    return valid;
}
bool cheats_backend_available(const char *root, char error[160])
{
    char shared[PATH_CAP], snapshot[PATH_CAP];
    if (!path_join(shared, root, "_nds/nds-bootstrap/wideCheatData.bin") ||
        !empty_payload(shared)) {
        snprintf(error, 160,
                 "A foreign widescreen cheat payload is active. Disable it in its owning launcher; "
                 "no shared payload was changed.");
        return false;
    }
    if (!path_join(shared, root, "_nds/nds-bootstrap/cheatData.bin") ||
        !path_preferences_file(snapshot, root, "last-cheatData.bin"))
        return false;
    if (!empty_payload(shared) && !file_equal(shared, snapshot)) {
        snprintf(error, 160,
                 "A foreign cheat payload is active. Disable it in its owning launcher; no shared "
                 "payload was changed.");
        return false;
    }
    return true;
}
static bool write_bytes(const char *path, const void *data, size_t bytes, bool exclusive)
{
    FILE *file = fopen(path, exclusive ? "wbx" : "wb");
    if (!file)
        return false;
    bool written = fwrite(data, 1, bytes, file) == bytes;
    if (fclose(file))
        written = false;
    if (!written)
        remove(path);
    return written;
}
bool cheats_backend_commit(const char *root, const char *content, char error[160])
{
    if (!cheats_backend_available(root, error))
        return false;
    CheatState *state = calloc(1, sizeof *state);
    unsigned char *payload = malloc(CHEAT_PAYLOAD_CAP);
    if (!state || !payload) {
        free(state);
        free(payload);
        snprintf(error, 160, "Not enough memory to prepare cheats");
        return false;
    }
    cheats_load(state, root, content);
    size_t bytes;
    bool valid = cheats_payload(state, payload, &bytes);
    cheats_free(state);
    free(state);
    if (!valid) {
        free(payload);
        snprintf(error, 160, "Selected cheats exceed the backend's %u KiB payload limit",
                 (unsigned)(CHEAT_PAYLOAD_CAP / 1024));
        return false;
    }
    char shared[PATH_CAP], snapshot[PATH_CAP], snapshot_next[PATH_CAP], next[PATH_CAP],
        previous[PATH_CAP], directory[PATH_CAP];
    valid = path_join(shared, root, "_nds/nds-bootstrap/cheatData.bin") &&
            path_join(directory, root, "_nds/nds-bootstrap") &&
            path_join(next, root, "_nds/nds-bootstrap/cheatData.frontend-next") &&
            path_preferences_file(snapshot, root, "last-cheatData.bin") &&
            path_preferences_file(snapshot_next, root, "next-cheatData.bin") &&
            path_preferences_file(previous, root, "previous-cheatData.bin");
    struct stat status;
    bool had_current = valid && !stat(shared, &status);
    /* No selected codes and no active shared payload needs no work directory. */
    if (valid && bytes == 4 && !had_current) {
        free(payload);
        return true;
    }
    if (valid && mkdir(directory, 0777) && (stat(directory, &status) || !S_ISDIR(status.st_mode)))
        valid = false;
    if (valid)
        valid = write_bytes(snapshot_next, payload, bytes, false) &&
                write_bytes(next, payload, bytes, true);
    free(payload);
    if (!valid)
        goto failure;
    if (had_current) {
        if (remove(previous) && errno != ENOENT)
            goto failure;
        if (rename(shared, previous))
            goto failure;
    }
    if (rename(next, shared)) {
        if (had_current)
            rename(previous, shared);
        goto failure;
    }
    /* Publish ownership only after the exact shared bytes are in place. A
     * failed ownership update blocks the next launch, rather than guessing. */
    if (remove(snapshot) && errno != ENOENT)
        goto failure;
    if (rename(snapshot_next, snapshot))
        goto failure;
    return true;
failure:
    snprintf(error, 160,
             "Could not prepare cheat payload. Handoff stopped; check SD access and any "
             "interrupted payload temporary file.");
    return false;
}
