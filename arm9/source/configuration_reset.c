/* Move only frontend-owned configuration into an independent backup. Each
 * rename is reversible until the pending directory itself is committed. */
#include "configuration_reset.h"
#include "configuration.h"
#include "paths.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
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
                                      "recent.bak",
                                      "appearance-1.ini",
                                      "appearance-1.bak",
                                      "appearance-2.ini",
                                      "appearance-2.bak",
                                      "appearance-3.ini",
                                      "appearance-3.bak",
                                      "games",
                                      "items",
                                      "plugins/user"};
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

static bool record_paths(const char *base, const char *pending, unsigned index,
                         char current[PATH_CAP], char saved[PATH_CAP])
{
    char name[12];
    snprintf(name, sizeof name, "%02u", index);
    return path_join(current, base, records[index]) && path_join(saved, pending, name);
}

static bool is_default_state(const char *path)
{
    char text[sizeof defaults];
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    size_t bytes = fread(text, 1, sizeof text, file);
    bool valid = !ferror(file) && bytes == sizeof defaults - 1 && !memcmp(text, defaults, bytes);
    fclose(file);
    return valid;
}

bool configuration_reset_recover(const char *root, char error[160])
{
    char base[PATH_CAP], pending[PATH_CAP], current[PATH_CAP], saved[PATH_CAP];
    bool present;
    bool valid = paths(root, base, pending) && exists(pending, &present);
    if (valid && !present)
        return true;
    /* Preflight every conflict before rolling back any record. The only new
     * file a reset creates is this exact minimal state, blocking legacy import. */
    for (unsigned i = 0; valid && i < sizeof records / sizeof *records; i++) {
        bool live, backup;
        valid = record_paths(base, pending, i, current, saved) && exists(current, &live) &&
                exists(saved, &backup);
        if (valid && live && backup)
            valid = i == 0 && is_default_state(current);
    }
    for (unsigned i = 0; valid && i < sizeof records / sizeof *records; i++) {
        bool backup;
        valid = record_paths(base, pending, i, current, saved) && exists(saved, &backup);
        if (valid && backup) {
            if (i == 0 && is_default_state(current))
                valid = !remove(current);
            if (valid)
                valid = !rename(saved, current);
        }
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
        valid = record_paths(base, pending, i, current, saved) && exists(current, &present);
    }
    bool started = valid && !mkdir(pending, 0777);
    valid = started;
    for (unsigned i = 0; valid && i < sizeof records / sizeof *records; i++) {
        char current[PATH_CAP], saved[PATH_CAP];
        valid = record_paths(base, pending, i, current, saved) && exists(current, &present);
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
