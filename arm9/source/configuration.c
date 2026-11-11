/* Frontend-owned configuration only. Validate an entire archive before writes;
 * journal old/new checksums so an interrupted import can be rolled back. */
#include "configuration.h"
#include "paths.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define RECORD_LIMIT 1024
#define RECORD_BYTES 65536u
#define ARCHIVE_BYTES (4u * 1024u * 1024u)
#define NAME_CAP 96

typedef struct {
    char name[NAME_CAP];
    uint32_t bytes, crc, offset;
} Record;
typedef struct {
    Record records[RECORD_LIMIT];
    unsigned count;
} RecordList;

static uint32_t crc_step(uint32_t crc, const unsigned char *data, size_t bytes)
{
    while (bytes--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
    }
    return crc;
}
static uint32_t number(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put_number(unsigned char *p, uint32_t value)
{
    for (int i = 0; i < 4; i++)
        p[i] = (unsigned char)(value >> (i * 8));
}
static bool allowed(const char *name)
{
    static const char *root_records[] = {"state.ini",        "tabs.ini",        "dashboard.ini",
                                         "recent.ini",       "language.ini",    "appearance-1.ini",
                                         "appearance-2.ini", "appearance-3.ini"};
    for (unsigned i = 0; i < sizeof root_records / sizeof *root_records; i++)
        if (!strcmp(name, root_records[i]))
            return true;
    const char *file = NULL;
    if (!strncmp(name, "games/", 6))
        file = name + 6;
    else if (!strncmp(name, "items/", 6))
        file = name + 6;
    if (file) {
        for (int i = 0; i < 16; i++)
            if (!file[i] || !strchr("0123456789abcdef", file[i]))
                return false;
        return !strcmp(file + 16, ".ini") ||
               (!strncmp(name, "games/", 6) && !strcmp(file + 16, ".cheats.ini"));
    }
    if (!strncmp(name, "plugins/user/", 13)) {
        file = name + 13;
        unsigned length = 0;
        while (file[length] && file[length] != '.') {
            if (!(islower((unsigned char)file[length]) || isdigit((unsigned char)file[length]) ||
                  file[length] == '_' || file[length] == '-') ||
                ++length > 15)
                return false;
        }
        return length && !strcmp(file + length, ".ini");
    }
    return false;
}
/* Reject redirected host paths as well; FAT itself has no symlink support. */
static bool owned_path(char out[PATH_CAP], const char *base, const char *name)
{
    if (!allowed(name) || !path_join(out, base, name))
        return false;
    char check[PATH_CAP];
    path_copy(check, out, sizeof check);
    for (char *p = check + strlen(base);; p++) {
        if (*p != '/' && *p)
            continue;
        char saved = *p;
        *p = 0;
        struct stat status;
        bool redirected = !lstat(check, &status) && S_ISLNK(status.st_mode);
        *p = saved;
        if (redirected)
            return false;
        if (!saved)
            return true;
    }
}
static bool make_directory(const char *path)
{
    struct stat status;
    return !mkdir(path, 0777) ||
           (errno == EEXIST && !lstat(path, &status) && S_ISDIR(status.st_mode));
}
static bool prepare_parent(const char *path)
{
    char parent[PATH_CAP];
    path_copy(parent, path, sizeof parent);
    for (char *p = parent; *p; p++) {
        if (*p != '/' || p == parent || p[-1] == ':')
            continue;
        *p = 0;
        bool ready = make_directory(parent);
        *p = '/';
        if (!ready)
            return false;
    }
    return true;
}
static bool checksum(const char *path, uint32_t *bytes, uint32_t *crc)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char buffer[512];
    uint32_t count = 0, hash = UINT32_MAX;
    size_t read;
    bool valid = true;
    while ((read = fread(buffer, 1, sizeof buffer, file))) {
        count += read;
        if (count > RECORD_BYTES || memchr(buffer, 0, read)) {
            valid = false;
            break;
        }
        hash = crc_step(hash, buffer, read);
    }
    valid &= !ferror(file);
    fclose(file);
    *bytes = count;
    *crc = hash ^ UINT32_MAX;
    return valid;
}
static bool add_record(RecordList *list, const char *base, const char *name)
{
    char path[PATH_CAP];
    if (!allowed(name))
        return true;
    if (list->count == RECORD_LIMIT || strlen(name) >= NAME_CAP || !owned_path(path, base, name))
        return false;
    struct stat status;
    if (stat(path, &status))
        return errno == ENOENT;
    if (!S_ISREG(status.st_mode))
        return false;
    Record *record = &list->records[list->count];
    if (!checksum(path, &record->bytes, &record->crc))
        return false;
    path_copy(record->name, name, sizeof record->name);
    list->count++;
    return true;
}
static bool collect(RecordList *list, const char *base)
{
    static const char *directories[] = {"", "games", "items", "plugins/user"};
    memset(list, 0, sizeof *list);
    unsigned scanned = 0;
    for (unsigned i = 0; i < sizeof directories / sizeof *directories; i++) {
        char path[PATH_CAP];
        if (!path_join(path, base, directories[i]))
            return false;
        struct stat status;
        if (!lstat(path, &status) && S_ISLNK(status.st_mode))
            return false;
        DIR *directory = opendir(path);
        if (!directory) {
            if (errno == ENOENT)
                continue;
            return false;
        }
        bool valid = true;
        struct dirent *entry;
        while ((entry = readdir(directory))) {
            if (++scanned > 8192) {
                valid = false;
                break;
            }
            char name[NAME_CAP];
            int length = snprintf(name, sizeof name, "%s%s%s", directories[i],
                                  *directories[i] ? "/" : "", entry->d_name);
            if (length >= (int)sizeof name)
                continue;
            if (!add_record(list, base, name)) {
                valid = false;
                break;
            }
        }
        closedir(directory);
        if (!valid)
            return false;
    }
    return true;
}
static bool read_archive(const char *path, RecordList *list)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[12];
    bool valid =
        fread(header, 1, sizeof header, file) == sizeof header && !memcmp(header, "DSCFG001", 8);
    list->count = valid ? number(header + 8) : 0;
    valid &= list->count <= RECORD_LIMIT;
    uint32_t total = 0;
    for (unsigned i = 0; valid && i < list->count; i++) {
        Record *record = &list->records[i];
        valid = fread(header, 1, sizeof header, file) == sizeof header;
        unsigned length = header[0] | (unsigned)header[1] << 8;
        record->bytes = number(header + 4);
        record->crc = number(header + 8);
        total += record->bytes;
        valid &= !header[2] && !header[3] && length && length < NAME_CAP &&
                 record->bytes <= RECORD_BYTES && total <= ARCHIVE_BYTES;
        if (!valid)
            break;
        valid = fread(record->name, 1, length, file) == length && !memchr(record->name, 0, length);
        record->name[length] = 0;
        valid &= allowed(record->name);
        for (unsigned previous = 0; valid && previous < i; previous++)
            valid &= strcmp(list->records[previous].name, record->name) != 0;
        long offset = ftell(file);
        valid &= offset >= 0 && (uint64_t)offset <= ARCHIVE_BYTES + RECORD_LIMIT * 108u;
        record->offset = (uint32_t)offset;
        unsigned char buffer[512];
        uint32_t left = record->bytes, crc = UINT32_MAX;
        while (valid && left) {
            size_t bytes = left < sizeof buffer ? left : sizeof buffer;
            valid = fread(buffer, 1, bytes, file) == bytes && !memchr(buffer, 0, bytes);
            crc = crc_step(crc, buffer, bytes);
            left -= bytes;
        }
        valid &= (crc ^ UINT32_MAX) == record->crc;
    }
    valid &= !ferror(file) && fgetc(file) == EOF;
    fclose(file);
    return valid;
}
static bool write_record(FILE *archive, const char *path, const Record *record)
{
    unsigned char header[12] = {0}, buffer[512];
    unsigned length = strlen(record->name);
    header[0] = length;
    header[1] = length >> 8;
    put_number(header + 4, record->bytes);
    put_number(header + 8, record->crc);
    FILE *source = fopen(path, "rb");
    if (!source)
        return false;
    bool valid = fwrite(header, 1, sizeof header, archive) == sizeof header &&
                 fwrite(record->name, 1, length, archive) == length;
    uint32_t left = record->bytes, crc = UINT32_MAX;
    while (valid && left) {
        size_t bytes = left < sizeof buffer ? left : sizeof buffer;
        valid =
            fread(buffer, 1, bytes, source) == bytes && fwrite(buffer, 1, bytes, archive) == bytes;
        crc = crc_step(crc, buffer, bytes);
        left -= bytes;
    }
    valid &= !ferror(source) && (crc ^ UINT32_MAX) == record->crc && fgetc(source) == EOF;
    fclose(source);
    return valid;
}
static bool export_to(const char *base, const char *output, RecordList *list)
{
    if (!collect(list, base) || !prepare_parent(output))
        return false;
    char temporary[PATH_CAP];
    if (snprintf(temporary, sizeof temporary, "%s.tmp", output) >= (int)sizeof temporary)
        return false;
    FILE *file = fopen(temporary, "wbx");
    if (!file)
        return false;
    unsigned char header[12];
    memcpy(header, "DSCFG001", 8);
    put_number(header + 8, list->count);
    bool valid = fwrite(header, 1, sizeof header, file) == sizeof header;
    uint32_t total = 0;
    for (unsigned i = 0; valid && i < list->count; i++) {
        char path[PATH_CAP];
        total += list->records[i].bytes;
        valid = total <= ARCHIVE_BYTES && owned_path(path, base, list->records[i].name) &&
                write_record(file, path, &list->records[i]);
    }
    if (fclose(file))
        valid = false;
    if (!valid) {
        remove(temporary);
        return false;
    }
    char backup[PATH_CAP];
    if (snprintf(backup, sizeof backup, "%s.bak", output) >= (int)sizeof backup)
        return false;
    remove(backup);
    bool existed = rename(output, backup) == 0;
    if (rename(temporary, output)) {
        if (existed)
            rename(backup, output);
        return false;
    }
    return true;
}
bool configuration_inspect(const char *path, unsigned *count, char error[160])
{
    RecordList *list = malloc(sizeof *list);
    bool valid = list && read_archive(path, list);
    if (valid)
        *count = list->count;
    else
        snprintf(error, 160,
                 "Invalid configuration archive. Use DSCFG001, at most 1024 records/4 MiB, with "
                 "valid checksums.");
    free(list);
    return valid;
}
static bool private_base(const char *root, char base[PATH_CAP], bool create);

bool configuration_export(const char *root, char error[160])
{
    char base[PATH_CAP], output[PATH_CAP];
    RecordList *list = malloc(sizeof *list);
    bool valid = list && private_base(root, base, true) &&
                 path_preferences_file(output, root, "configuration/export.dsconfig") &&
                 export_to(base, output, list);
    free(list);
    snprintf(error, 160, "%s",
             valid ? "Saved configuration/export.dsconfig"
                   : "Configuration export failed; check SD access and record limits.");
    return valid;
}

static bool private_base(const char *root, char base[PATH_CAP], bool create)
{
    char parent[PATH_CAP];
    struct stat status;
    if (!path_join(parent, root, "_nds") || (!lstat(parent, &status) && S_ISLNK(status.st_mode)) ||
        !path_preferences_file(base, root, "") ||
        (!lstat(base, &status) && S_ISLNK(status.st_mode)))
        return false;
    size_t length = strlen(base);
    while (length && base[length - 1] == '/')
        base[--length] = 0;
    if (!lstat(base, &status) && S_ISLNK(status.st_mode))
        return false;
    return !create || path_prepare_preferences_directory(root);
}
static const Record *find_record(const RecordList *list, const char *name)
{
    for (unsigned i = 0; i < list->count; i++)
        if (!strcmp(list->records[i].name, name))
            return &list->records[i];
    return NULL;
}
static bool extract_record(FILE *archive, const char *base, const Record *record)
{
    char path[PATH_CAP], temporary[PATH_CAP];
    if (!owned_path(path, base, record->name) || !prepare_parent(path) ||
        snprintf(temporary, sizeof temporary, "%s.config-tmp", path) >= (int)sizeof temporary ||
        fseek(archive, record->offset, SEEK_SET))
        return false;
    struct stat status;
    if (!lstat(temporary, &status))
        return false; /* Preserve an unrecognized/interrupted temporary file. */
    FILE *file = fopen(temporary, "wbx");
    if (!file)
        return false;
    unsigned char buffer[512];
    uint32_t left = record->bytes, crc = UINT32_MAX;
    bool valid = true;
    while (left && valid) {
        size_t bytes = left < sizeof buffer ? left : sizeof buffer;
        valid =
            fread(buffer, 1, bytes, archive) == bytes && fwrite(buffer, 1, bytes, file) == bytes;
        crc = crc_step(crc, buffer, bytes);
        left -= bytes;
    }
    valid &= (crc ^ UINT32_MAX) == record->crc;
    if (fclose(file))
        valid = false;
    if (valid) {
        /* The complete original is in rollback.dsconfig before any removal. */
        if (remove(path) && errno != ENOENT)
            valid = false;
        else
            valid = !rename(temporary, path);
    }
    if (!valid)
        remove(temporary);
    return valid;
}
bool configuration_recover(const char *root, char error[160])
{
    char base[PATH_CAP], pending[PATH_CAP], rollback[PATH_CAP];
    if (!private_base(root, base, false) ||
        !path_preferences_file(pending, root, "configuration/pending.dsconfig") ||
        !path_preferences_file(rollback, root, "configuration/rollback.dsconfig")) {
        snprintf(error, 160, "Configuration recovery could not access its private directory.");
        return false;
    }
    struct stat status;
    if (stat(pending, &status))
        return errno == ENOENT;
    RecordList *incoming = malloc(sizeof *incoming), *previous = malloc(sizeof *previous);
    bool valid =
        incoming && previous && read_archive(pending, incoming) && read_archive(rollback, previous);
    /* First inspect every destination. Never overwrite unrelated edits made
     * after a crash; their old/new hashes will not match this transaction. */
    for (unsigned i = 0; valid && i < incoming->count; i++) {
        const Record *next = &incoming->records[i];
        const Record *old = find_record(previous, next->name);
        char path[PATH_CAP];
        valid = owned_path(path, base, next->name);
        if (!valid)
            break;
        if (lstat(path, &status)) {
            valid = errno == ENOENT;
            continue;
        }
        uint32_t bytes, crc;
        valid = S_ISREG(status.st_mode) && checksum(path, &bytes, &crc) &&
                ((bytes == next->bytes && crc == next->crc) ||
                 (old && bytes == old->bytes && crc == old->crc));
    }
    FILE *archive = valid ? fopen(rollback, "rb") : NULL;
    valid &= archive != NULL;
    for (unsigned i = 0; valid && i < incoming->count; i++) {
        const Record *next = &incoming->records[i];
        const Record *old = find_record(previous, next->name);
        char path[PATH_CAP];
        valid = owned_path(path, base, next->name);
        if (!valid)
            break;
        if (old)
            valid = extract_record(archive, base, old);
        else if (remove(path) && errno != ENOENT)
            valid = false;
    }
    if (archive)
        fclose(archive);
    if (valid)
        valid = !remove(pending);
    free(incoming);
    free(previous);
    if (!valid)
        snprintf(error, 160,
                 "Configuration recovery needs attention. Keep pending.dsconfig and "
                 "rollback.dsconfig; an SD error or external edit prevented safe rollback.");
    return valid;
}
static bool copy_archive(const char *source, const char *destination)
{
    FILE *input = fopen(source, "rb");
    if (!input)
        return false;
    FILE *output = fopen(destination, "wbx");
    bool valid = output != NULL;
    unsigned char buffer[512];
    size_t read, total = 0;
    while (valid && (read = fread(buffer, 1, sizeof buffer, input))) {
        total += read;
        valid =
            total <= ARCHIVE_BYTES + RECORD_LIMIT * 108u && fwrite(buffer, 1, read, output) == read;
    }
    valid &= !ferror(input);
    fclose(input);
    if (output && fclose(output))
        valid = false;
    return valid;
}
bool configuration_import(const char *root, const char *source, char error[160])
{
    char base[PATH_CAP], pending[PATH_CAP], temporary[PATH_CAP], rollback[PATH_CAP];
    RecordList *incoming = malloc(sizeof *incoming), *previous = malloc(sizeof *previous);
    bool valid = incoming && previous && private_base(root, base, true) &&
                 read_archive(source, incoming) &&
                 path_preferences_file(pending, root, "configuration/pending.dsconfig") &&
                 path_preferences_file(temporary, root, "configuration/incoming.tmp") &&
                 path_preferences_file(rollback, root, "configuration/rollback.dsconfig");
    struct stat status;
    if (valid)
        valid = lstat(pending, &status) && errno == ENOENT;
    /* Copy/validate again: the source may be on the same SD and must remain
     * independent of all destinations throughout the transaction. */
    bool staged = false;
    if (valid) {
        valid = prepare_parent(temporary) && strcmp(source, temporary) &&
                copy_archive(source, temporary);
        staged = valid;
        valid = valid && read_archive(temporary, incoming) && export_to(base, rollback, previous) &&
                !rename(temporary, pending);
    }
    if (staged && !valid)
        remove(temporary); /* This run created it exclusively; original records are untouched. */
    FILE *archive = valid ? fopen(pending, "rb") : NULL;
    bool journal_ready = valid;
    valid &= archive != NULL;
    for (unsigned i = 0; valid && i < incoming->count; i++)
        valid = extract_record(archive, base, &incoming->records[i]);
    if (archive)
        fclose(archive);
    if (valid)
        valid = !remove(pending);
    free(incoming);
    free(previous);
    if (!valid) {
        char recovery_error[160] = "";
        bool recovered = journal_ready && configuration_recover(root, recovery_error);
        snprintf(error, 160, "%s",
                 recovered         ? "Import failed; original configuration restored."
                 : *recovery_error ? recovery_error
                                   : "Import unavailable. Check archive, SD access and pending "
                                     "transaction; current configuration was not replaced.");
        return false;
    }
    snprintf(error, 160,
             "Configuration imported. Restart to load it; previous records remain in "
             "configuration/rollback.dsconfig.");
    return true;
}
