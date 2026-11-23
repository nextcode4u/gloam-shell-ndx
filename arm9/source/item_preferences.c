/* Sparse per-content records. Full normalized identity is checked alongside the
 * filename hash, so a collision never applies another item's settings. */
#include "item_preferences.h"
#include "paths.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
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
/* Only completely inspected damaged records may be replaced using a matching backup. */
typedef enum { RECORD_UNREADABLE, RECORD_INVALID, RECORD_PROTECTED, RECORD_VALID } RecordStatus;
static RecordStatus read_record(const char *path, const char *root, const char *key,
                                ItemPreferences *item)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return RECORD_UNREADABLE;
    struct stat info;
    bool valid = !fstat(fileno(file), &info) && S_ISREG(info.st_mode) && info.st_size >= 0 &&
                 info.st_size <= 4096;
    size_t bytes = valid ? (size_t)info.st_size : 0;
    char *contents = valid ? malloc(bytes + 1) : NULL;
    valid = contents && fread(contents, 1, bytes, file) == bytes && !ferror(file);
    if (fclose(file))
        valid = false;
    if (!valid || memchr(contents, 0, bytes)) {
        free(contents);
        return RECORD_UNREADABLE;
    }
    contents[bytes] = 0;
    bool version = false, matched = false, foreign = false, unsupported = false;
    unsigned seen = 0;
    const char *keys[] = {"version", "content", "cover", "application", "icon"};
    char *cursor = contents;
    while (*cursor) {
        char *line = cursor, *next = strchr(cursor, '\n');
        cursor = next ? next + 1 : cursor + strlen(cursor);
        if (next)
            *next = 0;
        size_t length = strlen(line);
        if (length >= PATH_CAP + 31) {
            free(contents);
            return RECORD_UNREADABLE; /* Do not replace an identity we could not inspect. */
        }
        if (length && line[length - 1] == '\r')
            line[length - 1] = 0;
        char *value = strchr(line, '=');
        if (!value)
            continue;
        *value++ = 0;
        unsigned field;
        for (field = 0; field < 5 && strcmp(line, keys[field]); field++) {
        }
        if (field == 5)
            continue;
        valid &= !(seen & (1u << field));
        seen |= 1u << field;
        if (!field) {
            version = !strcmp(value, "1");
            unsupported |= *value && !version;
            valid &= version;
        } else if (field == 1) {
            matched = !strcmp(value, key);
            foreign |= *value && !matched;
        } else {
            char *destination = field == 2   ? item->cover
                                : field == 3 ? item->application
                                             : item->icon;
            destination[0] = 0;
            if (*value && !path_resolve_sd(destination, root, value))
                valid = false;
        }
    }
    free(contents);
    if (foreign || unsupported)
        return RECORD_PROTECTED;
    return valid && version && matched ? RECORD_VALID : RECORD_INVALID;
}

bool item_preferences_load(const char *root, const char *content, ItemPreferences *item)
{
    memset(item, 0, sizeof *item);
    char key[PATH_CAP], path[PATH_CAP];
    if (!identity(key, root, content) || !record_path(path, root, key, "ini"))
        return false;
    ItemPreferences candidate = {0};
    if (read_record(path, root, key, &candidate) != RECORD_VALID) {
        memset(&candidate, 0, sizeof candidate);
        if (!record_path(path, root, key, "bak") ||
            read_record(path, root, key, &candidate) != RECORD_VALID)
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
    ItemPreferences existing = {0};
    bool exists = !stat(path, &status), recovered = false;
    if (!exists && errno != ENOENT)
        return false;
    if (exists) {
        RecordStatus record = read_record(path, root, key, &existing);
        if (record != RECORD_VALID) {
            if (record != RECORD_INVALID ||
                read_record(backup, root, key, &existing) != RECORD_VALID)
                return false;
            recovered = true;
        }
    }
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
    /* Keep the verified backup when replacing a damaged primary. A failed install
     * can then recover from it instead of rotating the damaged record over it. */
    if (recovered && remove(path) && errno != ENOENT) {
        remove(temporary);
        return false;
    }
    if (path_commit_file(temporary, path, backup) != FILE_COMMIT_OK)
        return false;
    return true;
}
