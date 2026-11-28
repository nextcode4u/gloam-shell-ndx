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
static bool record_path(char path[PATH_CAP], const char *root, const char *key, const char *suffix,
                        bool bucketed)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    char name[48];
    if (bucketed)
        snprintf(name, sizeof name, "items/%02x/%016" PRIx64 ".%s", (unsigned)(hash >> 56), hash,
                 suffix);
    else
        snprintf(name, sizeof name, "items/%016" PRIx64 ".%s", hash, suffix);
    return path_preferences_file(path, root, name);
}
/* Opt-in layout marker lives outside the large items directory. Old cards
 * remain flat; optimized cards can still contain readable legacy records. */
static int storage_layout(const char *root)
{
    char path[PATH_CAP], data[65], normalized[65];
    if (!path_preferences_file(path, root, "item-storage.ini"))
        return -1;
    FILE *file = fopen(path, "rb");
    if (!file)
        return errno == ENOENT ? 0 : -1;
    struct stat info;
    bool valid = !fstat(fileno(file), &info) && S_ISREG(info.st_mode) && info.st_size > 0 &&
                 info.st_size <= 64;
    size_t size = valid ? (size_t)info.st_size : 0;
    valid = valid && fread(data, 1, size, file) == size && !ferror(file) && !memchr(data, 0, size);
    if (fclose(file))
        valid = false;
    if (!valid)
        return -1;
    size_t out = 0;
    for (size_t i = 0; i < size; i++)
        if (data[i] != '\r')
            normalized[out++] = data[i];
    if (out && normalized[out - 1] == '\n')
        out--;
    normalized[out] = 0;
    if (!strcmp(normalized, "version=1\nbuckets=1"))
        return 1;
    return !strcmp(normalized, "version=1\nbuckets=0") ? 0 : -1;
}

/* Only completely inspected damaged records may be replaced using a matching backup. */
typedef enum {
    RECORD_MISSING,
    RECORD_UNREADABLE,
    RECORD_INVALID,
    RECORD_PROTECTED,
    RECORD_VALID
} RecordStatus;
static RecordStatus read_record(const char *path, const char *root, const char *key,
                                ItemPreferences *item)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return errno == ENOENT ? RECORD_MISSING : RECORD_UNREADABLE;
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

static RecordStatus load_family(const char *root, const char *key, ItemPreferences *item,
                                bool for_edit, bool bucketed)
{
    char path[PATH_CAP];
    RecordStatus result = RECORD_MISSING;
    for (unsigned backup = 0; backup < 2; backup++) {
        ItemPreferences candidate = {0};
        RecordStatus status = record_path(path, root, key, backup ? "bak" : "ini", bucketed)
                                  ? read_record(path, root, key, &candidate)
                                  : RECORD_UNREADABLE;
        /* A readable older backup cannot establish an unreadable primary's fields. */
        if (for_edit && !backup && (status == RECORD_UNREADABLE || status == RECORD_PROTECTED))
            return status;
        if (status == RECORD_VALID) {
            *item = candidate;
            return RECORD_VALID;
        }
        if (status != RECORD_MISSING)
            result = status;
    }
    return result;
}
static RecordStatus load_item(const char *root, const char *content, ItemPreferences *item,
                              bool for_edit)
{
    char key[PATH_CAP];
    int bucketed = storage_layout(root);
    if (bucketed < 0 || !identity(key, root, content))
        return RECORD_UNREADABLE;
    if (bucketed) {
        RecordStatus status = load_family(root, key, item, for_edit, true);
        if (status != RECORD_MISSING)
            return status;
    }
    return load_family(root, key, item, for_edit, false);
}

bool item_preferences_load(const char *root, const char *content, ItemPreferences *item)
{
    memset(item, 0, sizeof *item);
    return load_item(root, content, item, false) == RECORD_VALID;
}
typedef struct {
    char content[PATH_CAP];
    ItemPreferences item;
    bool occupied, valid;
} DisplayRecord;
static DisplayRecord display_records[4];
static char display_root[PATH_CAP];
static unsigned display_next;

void item_preferences_display_clear(void)
{
    memset(display_records, 0, sizeof display_records);
    display_root[0] = 0;
    display_next = 0;
}

bool item_preferences_display(const char *root, const char *content, ItemPreferences *item)
{
    if (strlen(root) >= PATH_CAP || strlen(content) >= PATH_CAP)
        return item_preferences_load(root, content, item);
    if (strcmp(display_root, root)) {
        item_preferences_display_clear();
        path_copy(display_root, root, sizeof display_root);
    }
    for (unsigned i = 0; i < 4; i++) {
        const DisplayRecord *record = &display_records[i];
        if (record->occupied && !strcmp(record->content, content)) {
            *item = record->item;
            return record->valid;
        }
    }
    memset(item, 0, sizeof *item);
    RecordStatus status = load_item(root, content, item, false);
    /* Do not hide a transient read failure behind a negative cache entry. */
    if (status == RECORD_VALID || status == RECORD_MISSING) {
        DisplayRecord *record = &display_records[display_next++ % 4];
        path_copy(record->content, content, sizeof record->content);
        record->item = *item;
        record->valid = status == RECORD_VALID;
        record->occupied = true;
    }
    return status == RECORD_VALID;
}
bool item_preferences_resolve(const char *root, const char *content, ItemPreferences *item)
{
    RecordStatus status = load_item(root, content, item, false);
    if (status == RECORD_MISSING)
        memset(item, 0, sizeof *item);
    return status == RECORD_VALID || status == RECORD_MISSING;
}
bool item_preferences_read_for_edit(const char *root, const char *content, ItemPreferences *item)
{
    RecordStatus status = load_item(root, content, item, true);
    if (status == RECORD_MISSING)
        memset(item, 0, sizeof *item);
    return status == RECORD_VALID || status == RECORD_MISSING;
}
bool item_preferences_save(const char *root, const char *content, const ItemPreferences *item)
{
    item_preferences_display_clear();
    int bucketed = storage_layout(root);
    ItemPreferences previous;
    RecordStatus inspected = load_item(root, content, &previous, true);
    if (bucketed < 0 || (inspected != RECORD_VALID && inspected != RECORD_MISSING))
        return false;
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
    if (!record_path(path, root, key, "ini", bucketed != 0) ||
        !record_path(temporary, root, key, "tmp", bucketed != 0) ||
        !record_path(backup, root, key, "bak", bucketed != 0))
        return false;
    if (bucketed) {
        path_copy(directory, path, sizeof directory);
        char *slash = strrchr(directory, '/');
        if (!slash)
            return false;
        *slash = 0;
        if (mkdir(directory, 0777) &&
            (errno != EEXIST || stat(directory, &status) || !S_ISDIR(status.st_mode)))
            return false;
    }
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
