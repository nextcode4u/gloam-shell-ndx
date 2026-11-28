/* Move only frontend-owned configuration into an independent backup. Each
 * rename is reversible until the pending directory itself is committed. */
#include "configuration_reset.h"
#include "configuration.h"
#include "paths.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *const records[] = {"state.ini",
                                      "state.bak",
                                      "tabs.ini",
                                      "tabs.bak",
                                      "dashboard.ini",
                                      "dashboard.bak",
                                      "recent.ini",
                                      "language.ini",
                                      "collections.ini",
                                      "recent.bak",
                                      "appearance-1.ini",
                                      "appearance-1.bak",
                                      "appearance-2.ini",
                                      "appearance-2.bak",
                                      "appearance-3.ini",
                                      "appearance-3.bak",
                                      "games",
                                      "items",
                                      "plugins/user",
                                      "item-storage.ini"};
enum { RESET_RECORD_COUNT = sizeof records / sizeof *records, RESET_LAYOUT_BYTES = 1024 };
typedef struct {
    const char *names[RESET_RECORD_COUNT];
    unsigned count;
} ResetLayout;
static const char layout_id[] = "Gloam reset layout 1\n";
static const char defaults[] = "version=1\n";

static bool exists(const char *path, bool *present)
{
    struct stat status;
    if (!lstat(path, &status)) {
        *present = true;
        return !S_ISLNK(status.st_mode);
    }
    *present = false;
    return errno == ENOENT;
}

static bool paths(const char *root, char base[PATH_CAP], char pending[PATH_CAP])
{
    char path[PATH_CAP];
    bool present;
    /* FAT has no symbolic links, but host fixtures must not redirect writes. */
    const char *const parents[] = {"_nds", FRONTEND_DATA_DIRECTORY,
                                   FRONTEND_DATA_DIRECTORY "/configuration",
                                   FRONTEND_DATA_DIRECTORY "/plugins"};
    for (unsigned i = 0; i < sizeof parents / sizeof *parents; i++)
        if (!path_join(path, root, parents[i]) || !exists(path, &present))
            return false;
    return path_preferences_file(base, root, "") &&
           path_preferences_file(pending, root, "configuration/reset-pending");
}

static bool record_paths(const char *base, const char *pending, const char *record, unsigned index,
                         char current[PATH_CAP], char saved[PATH_CAP])
{
    char name[12];
    snprintf(name, sizeof name, "%02u", index);
    return path_join(current, base, record) && path_join(saved, pending, name);
}

/* Each backup carries its own slot-to-name map. Recovery never infers it from
 * the current build's order: adding a record must not rename an older one. */
static bool layout_write(const char *pending)
{
    char text[RESET_LAYOUT_BYTES], path[PATH_CAP];
    size_t used = sizeof layout_id - 1;
    memcpy(text, layout_id, used);
    for (unsigned i = 0; i < RESET_RECORD_COUNT; i++) {
        int bytes = snprintf(text + used, sizeof text - used, "%02u=%s\n", i, records[i]);
        if (bytes < 0 || (size_t)bytes >= sizeof text - used)
            return false;
        used += (size_t)bytes;
    }
    if (!path_join(path, pending, "layout.txt"))
        return false;
    FILE *file = fopen(path, "wx");
    if (!file)
        return false;
    bool valid = fwrite(text, 1, used, file) == used && !ferror(file);
    if (fclose(file))
        valid = false;
    if (!valid)
        remove(path);
    return valid;
}
static bool layout_directory(const char *pending, const ResetLayout *layout)
{
    DIR *directory = opendir(pending);
    if (!directory)
        return false;
    bool valid = true;
    unsigned scanned = 0;
    while (valid) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (!entry) {
            valid = errno == 0;
            break;
        }
        if (++scanned > RESET_RECORD_COUNT + 3) {
            valid = false;
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") ||
            !strcasecmp(entry->d_name, "layout.txt"))
            continue;
        bool known = false;
        for (unsigned i = 0; i < layout->count; i++) {
            char name[12];
            snprintf(name, sizeof name, "%02u", i);
            if (!strcmp(entry->d_name, name)) {
                known = true;
                break;
            }
        }
        valid = known;
    }
    if (closedir(directory))
        valid = false;
    return valid;
}
static bool layout_read(const char *pending, ResetLayout *layout)
{
    char path[PATH_CAP], text[RESET_LAYOUT_BYTES + 1];
    bool present;
    layout->count = 0;
    if (!path_join(path, pending, "layout.txt") || !exists(path, &present) || !present)
        return false;
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    struct stat status;
    bool valid = !fstat(fileno(file), &status) && S_ISREG(status.st_mode) &&
                 status.st_size >= (off_t)(sizeof layout_id - 1) &&
                 status.st_size <= RESET_LAYOUT_BYTES;
    size_t bytes = valid ? (size_t)status.st_size : 0;
    valid = valid && fread(text, 1, bytes, file) == bytes && !ferror(file);
    if (fclose(file))
        valid = false;
    if (!valid || memchr(text, 0, bytes) || memcmp(text, layout_id, sizeof layout_id - 1))
        return false;
    text[bytes] = 0;
    char *cursor = text + sizeof layout_id - 1;
    while (*cursor) {
        if (layout->count == RESET_RECORD_COUNT)
            return false;
        char *end = strchr(cursor, '\n');
        if (!end)
            return false;
        *end = 0;
        char prefix[12];
        int length = snprintf(prefix, sizeof prefix, "%02u=", layout->count);
        if (length < 0 || (size_t)length >= sizeof prefix ||
            strncmp(cursor, prefix, (size_t)length))
            return false;
        const char *name = cursor + length;
        unsigned record;
        for (record = 0; record < RESET_RECORD_COUNT && strcmp(name, records[record]); record++) {
        }
        if (record == RESET_RECORD_COUNT)
            return false;
        for (unsigned previous = 0; previous < layout->count; previous++)
            if (layout->names[previous] == records[record])
                return false;
        layout->names[layout->count++] = records[record];
        cursor = end + 1;
    }
    return layout->count && layout_directory(pending, layout);
}
static bool is_default_state(const char *path)
{
    char text[sizeof defaults];
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    struct stat status;
    bool valid = !fstat(fileno(file), &status) && S_ISREG(status.st_mode) &&
                 status.st_size == (off_t)(sizeof defaults - 1);
    size_t bytes = valid ? fread(text, 1, sizeof text, file) : 0;
    valid =
        valid && !ferror(file) && bytes == sizeof defaults - 1 && !memcmp(text, defaults, bytes);
    if (fclose(file))
        valid = false;
    return valid;
}

bool configuration_reset_recover(const char *root, char error[160])
{
    char base[PATH_CAP], pending[PATH_CAP], current[PATH_CAP], saved[PATH_CAP];
    bool present;
    bool valid = paths(root, base, pending) && exists(pending, &present);
    if (valid && !present)
        return true;
    ResetLayout layout = {0};
    if (valid && !layout_read(pending, &layout)) {
        if (!rmdir(pending)) /* Empty pending folders contain nothing to restore. */
            return true;
        snprintf(error, 160,
                 "Reset layout is missing or unsupported. Keep reset-pending; restore its "
                 "records using the version that created it.");
        return false;
    }
    /* Preflight every conflict before rolling back any record. The only new
     * file a reset creates is this exact minimal state, blocking legacy import. */
    for (unsigned i = 0; valid && i < layout.count; i++) {
        bool live, backup;
        valid = record_paths(base, pending, layout.names[i], i, current, saved) &&
                exists(current, &live) && exists(saved, &backup);
        if (valid && live && backup)
            valid = !strcmp(layout.names[i], "state.ini") && is_default_state(current);
    }
    for (unsigned i = 0; valid && i < layout.count; i++) {
        bool backup;
        valid = record_paths(base, pending, layout.names[i], i, current, saved) &&
                exists(saved, &backup);
        if (valid && backup) {
            bool live;
            valid = exists(current, &live);
            if (valid && live)
                valid = !strcmp(layout.names[i], "state.ini") && is_default_state(current) &&
                        !remove(current);
            if (valid)
                valid = !rename(saved, current);
        }
    }
    if (valid) {
        char marker[PATH_CAP];
        valid = path_join(marker, pending, "layout.txt") && !remove(marker);
    }
    if (valid)
        valid = !rmdir(pending);
    if (!valid)
        snprintf(error, 160,
                 "Reset recovery needs attention. Keep configuration/reset-pending; "
                 "an SD error or external edit prevented rollback.");
    return valid;
}

bool configuration_reset(const char *root, char error[160])
{
    char base[PATH_CAP], pending[PATH_CAP], completed[PATH_CAP], directory[PATH_CAP];
    bool present;
    bool valid = paths(root, base, pending) && configuration_recover(root, error) &&
                 configuration_reset_recover(root, error) &&
                 path_preferences_file(directory, root, "configuration");
    if (valid && mkdir(directory, 0777) && errno != EEXIST)
        valid = false;
    /* A fixed limit keeps directory discovery bounded and never overwrites an
     * earlier reset backup. Folder contents stay in place through renames. */
    unsigned slot = 0;
    for (; valid && slot < 1000; slot++) {
        char name[64];
        snprintf(name, sizeof name, "configuration/reset-backup-%03u", slot);
        valid = path_preferences_file(completed, root, name) && exists(completed, &present);
        if (valid && !present)
            break;
    }
    valid &= slot < 1000;
    for (unsigned i = 0; valid && i < sizeof records / sizeof *records; i++) {
        char current[PATH_CAP], saved[PATH_CAP];
        valid =
            record_paths(base, pending, records[i], i, current, saved) && exists(current, &present);
    }
    bool started = valid && !mkdir(pending, 0777);
    valid = started;
    if (started && !layout_write(pending)) {
        rmdir(pending);
        snprintf(error, 160, "Could not record the reset layout. No settings were moved.");
        return false;
    }
    for (unsigned i = 0; valid && i < sizeof records / sizeof *records; i++) {
        char current[PATH_CAP], saved[PATH_CAP];
        valid =
            record_paths(base, pending, records[i], i, current, saved) && exists(current, &present);
        if (valid && present)
            valid = !rename(current, saved);
    }
    if (valid) {
        char state[PATH_CAP];
        valid = path_join(state, base, "state.ini");
        FILE *file = valid ? fopen(state, "wx") : NULL;
        valid = file != NULL;
        if (file) {
            valid = fputs(defaults, file) >= 0;
            if (fclose(file))
                valid = false;
            /* This call exclusively created the replacement. Remove an incomplete
             * write before rollback mistakes it for an independent external edit. */
            if (!valid)
                remove(state);
        }
    }
    if (valid)
        valid = !rename(pending, completed);
    if (!valid) {
        char recovery[160] = "";
        bool restored = started && configuration_reset_recover(root, recovery);
        snprintf(error, 160, "%s",
                 *recovery  ? recovery
                 : restored ? "Reset failed; settings restored."
                            : "Reset unavailable. Check SD access and existing transactions.");
        return false;
    }
    snprintf(error, 160, "Defaults restored. Previous settings: configuration/reset-backup-%03u",
             slot);
    return true;
}
