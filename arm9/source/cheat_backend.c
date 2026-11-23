/* Shared backend payload ownership. Foreign active cheats are never removed. */
#include "cheats.h"
#include "paths.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Matching prefixes after short reads do not prove payload ownership. */
static bool file_equal(const char *left, const char *right)
{
    FILE *a = fopen(left, "rb"), *b = fopen(right, "rb");
    struct stat first_info, second_info;
    bool equal = a && b && !fstat(fileno(a), &first_info) && !fstat(fileno(b), &second_info) &&
                 S_ISREG(first_info.st_mode) && S_ISREG(second_info.st_mode) &&
                 first_info.st_size >= 0 && first_info.st_size <= CHEAT_PAYLOAD_CAP &&
                 first_info.st_size == second_info.st_size;
    size_t remaining = equal ? (size_t)first_info.st_size : 0;
    unsigned char first[1024], second[1024];
    while (equal && remaining) {
        size_t count = remaining < sizeof first ? remaining : sizeof first;
        equal = fread(first, 1, count, a) == count && fread(second, 1, count, b) == count &&
                !memcmp(first, second, count);
        remaining -= count;
    }
    equal = equal && fgetc(a) == EOF && fgetc(b) == EOF && !ferror(a) && !ferror(b);
    if (a && fclose(a))
        equal = false;
    if (b && fclose(b))
        equal = false;
    return equal;
}
static bool empty_payload(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return errno == ENOENT;
    struct stat info;
    bool valid = !fstat(fileno(file), &info) && S_ISREG(info.st_mode) &&
                 (info.st_size == 0 || info.st_size == 4);
    unsigned char bytes[5];
    if (valid) {
        size_t count = fread(bytes, 1, sizeof bytes, file);
        valid = count == (size_t)info.st_size && !ferror(file) &&
                (!count || !memcmp(bytes, "\0\0\0\xCF", 4));
    }
    if (fclose(file))
        valid = false;
    return valid;
}

bool cheats_backend_available(const char *root, char error[160])
{
    char shared[PATH_CAP], snapshot[PATH_CAP];
    if (!path_join(shared, root, "_nds/nds-bootstrap/wideCheatData.bin") ||
        !empty_payload(shared)) {
        snprintf(error, 160,
                 "Cannot verify the widescreen cheat payload. Check SD access or disable it in "
                 "its owning launcher; no shared payload was changed.");
        return false;
    }
    if (!path_join(shared, root, "_nds/nds-bootstrap/cheatData.bin") ||
        !path_preferences_file(snapshot, root, "last-cheatData.bin"))
        return false;
    if (!empty_payload(shared) && !file_equal(shared, snapshot)) {
        snprintf(error, 160,
                 "Cannot verify the active cheat payload. Check SD access or disable it in its "
                 "owning launcher; no shared payload was changed.");
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
    bool had_current = false;
    if (valid) {
        had_current = !stat(shared, &status);
        if ((!had_current && errno != ENOENT) || (had_current && !S_ISREG(status.st_mode)))
            valid = false;
    }
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
