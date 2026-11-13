/* Sparse per-content records. Full normalized identity is checked alongside the
 * filename hash, so a collision never applies another item's settings. */
#include "item_preferences.h"
#include "paths.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool identity(char output[PATH_CAP], const char *root, const char *content)
{
    if (!path_is_within_root(content, root))
        return false;
    const char *relative = content + strlen(root);
    while (*relative == '/')
        relative++;
    return *relative && path_resolve_sd(output, "sd:/", relative);
}
static bool record_path(char path[PATH_CAP], const char *root, const char *key, const char *suffix)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    char name[48];
    snprintf(name, sizeof name, "items/%016" PRIx64 ".%s", hash, suffix);
    return path_preferences_file(path, root, name);
}
static bool read_record(const char *path, const char *root, const char *key, ItemPreferences *item)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    bool version = false, matched = false, valid = true;
    char line[PATH_CAP + 32];
    size_t total = 0;
    while (fgets(line, sizeof line, file)) {
        total += strlen(line);
        if (total > 4096 || (!strchr(line, '\n') && !feof(file))) {
            valid = false;
            break;
        }
        line[strcspn(line, "\r\n")] = 0;
        char *value = strchr(line, '=');
        if (!value)
            continue;
        *value++ = 0;
        if (!strcmp(line, "version"))
            version = !strcmp(value, "1");
        else if (!strcmp(line, "content"))
            matched = !strcmp(value, key);
        else if ((!strcmp(line, "cover") || !strcmp(line, "application") ||
                  !strcmp(line, "icon")) &&
                 *value) {
            char *destination = !strcmp(line, "cover")  ? item->cover
                                : !strcmp(line, "icon") ? item->icon
                                                        : item->application;
            if (!path_resolve_sd(destination, root, value))
                valid = false;
        }
    }
    valid = valid && !ferror(file);
    fclose(file);
    return valid && version && matched;
}
bool item_preferences_load(const char *root, const char *content, ItemPreferences *item)
{
    memset(item, 0, sizeof *item);
    char key[PATH_CAP], path[PATH_CAP];
    if (!identity(key, root, content) || !record_path(path, root, key, "ini"))
        return false;
    ItemPreferences candidate = {0};
    if (!read_record(path, root, key, &candidate)) {
        memset(&candidate, 0, sizeof candidate);
        if (!record_path(path, root, key, "bak") || !read_record(path, root, key, &candidate))
            return false;
    }
    *item = candidate;
    return true;
}
bool item_preferences_save(const char *root, const char *content, const ItemPreferences *item)
{
    char key[PATH_CAP], cover[PATH_CAP] = "", application[PATH_CAP] = "", icon[PATH_CAP] = "";
    char path[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP], directory[PATH_CAP];
    if (!identity(key, root, content) || (*item->cover && !identity(cover, root, item->cover)) ||
        (*item->application && !identity(application, root, item->application)) ||
        (*item->icon && !identity(icon, root, item->icon)) ||
        !path_prepare_preferences_directory(root) ||
        !path_preferences_file(directory, root, "items"))
        return false;
    struct stat status;
    if (mkdir(directory, 0777) &&
        (errno != EEXIST || stat(directory, &status) || !S_ISDIR(status.st_mode)))
        return false;
    if (!record_path(path, root, key, "ini") || !record_path(temporary, root, key, "tmp") ||
        !record_path(backup, root, key, "bak"))
        return false;
    /* An existing different identity must not be overwritten, even on a hash collision. */
    ItemPreferences existing;
    if (!stat(path, &status) && !read_record(path, root, key, &existing))
        return false;
    FILE *file = fopen(temporary, "w");
    if (!file)
        return false;
    fprintf(file, "version=1\ncontent=%s\ncover=%s\napplication=%s\nicon=%s\n", key, cover,
            application, icon);
    bool written = !ferror(file);
    if (fclose(file))
        written = false;
    if (!written) {
        remove(temporary);
        return false;
    }
    if (path_commit_file(temporary, path, backup) != FILE_COMMIT_OK)
        return false;
    return true;
}
