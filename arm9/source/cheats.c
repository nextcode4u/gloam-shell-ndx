/* Bounded read-only R4 usrcheat database reader. Reference format:
 * akmenu-next cheat.cpp (Normmatt, Smiths, bliss, yellow wood goblin), GPL-3.0+.
 * Database code+CRC matching, folder flags and word-block lengths follow that
 * contract. All strings, offsets, counts and code payloads are checked here. */
#include "cheats.h"
#include "paths.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static uint32_t le32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint64_t le64(const unsigned char *p)
{
    return le32(p) | (uint64_t)le32(p + 4) << 32;
}
uint32_t cheats_crc(const void *data, size_t bytes)
{
    const unsigned char *p = data;
    uint32_t crc = UINT32_MAX;
    while (bytes--) {
        crc ^= *p++;
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xEDB88320) : 0);
    }
    /* R4 indexes store this accumulator without the usual final inversion. */
    return crc;
}
static bool reference(char key[PATH_CAP], const char *root, const char *path)
{
    if (!path_is_within_root(path, root))
        return false;
    const char *relative = path + strlen(root);
    while (*relative == '/')
        relative++;
    return *relative && path_resolve_sd(key, "sd:/", relative);
}
static bool selection_path(char output[PATH_CAP], const char *root, const char *content,
                           const char *suffix)
{
    char key[PATH_CAP];
    if (!reference(key, root, content))
        return false;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    char name[64];
    snprintf(name, sizeof name, "games/%016" PRIx64 ".cheats.%s", hash, suffix);
    return path_preferences_file(output, root, name);
}
static bool string_at(const CheatState *state, size_t *offset, char *out, size_t capacity)
{
    if (*offset >= state->record_bytes)
        return false;
    const unsigned char *start = state->record + *offset;
    const unsigned char *end = memchr(start, 0, state->record_bytes - *offset);
    if (!end)
        return false;
    size_t bytes = (size_t)(end - start);
    if (out && capacity) {
        size_t copied = bytes < capacity - 1 ? bytes : capacity - 1;
        for (size_t i = 0; i < copied; i++)
            out[i] = start[i] >= 32 && start[i] <= 126 ? (char)start[i] : '?';
        out[copied] = 0;
    }
    *offset += bytes + 1;
    return true;
}
static bool parse_record(CheatState *state)
{
    size_t offset = 0;
    if (!string_at(state, &offset, NULL, 0))
        return false;
    offset = (offset + 3) & ~(size_t)3;
    if (offset > state->record_bytes || state->record_bytes - offset < 36)
        return false;
    uint32_t count = le32(state->record + offset) & 0x0FFFFFFF;
    offset += 36;
    if (count > CHEAT_ENTRY_CAP)
        return false;
    int group = -1;
    uint32_t children = 0;
    for (uint32_t index = 0; index < count; index++) {
        if (offset > state->record_bytes || state->record_bytes - offset < 4)
            return false;
        uint32_t flags = le32(state->record + offset);
        CheatEntry *entry = &state->entries[state->count++];
        entry->group = -1;
        entry->expanded = true;
        size_t strings = offset + 4;
        if (!string_at(state, &strings, entry->name, sizeof entry->name) ||
            !string_at(state, &strings, entry->note, sizeof entry->note))
            return false;
        strings = (strings + 3) & ~(size_t)3;
        if (flags & 0x10000000) {
            if (children)
                return false; /* Nested folders have no defined contract here. */
            entry->folder = true;
            entry->exclusive = (flags >> 24) == 0x11;
            group = (int)index;
            children = flags & 0x00FFFFFF;
            if (children > count - index - 1)
                return false;
            offset = strings;
            continue;
        }
        size_t words = (size_t)(flags & 0x00FFFFFF) + 1;
        if (words > (state->record_bytes - offset) / 4)
            return false;
        size_t end = offset + words * 4;
        if (strings > end || end - strings < 4)
            return false;
        uint32_t code_words = le32(state->record + strings);
        if (code_words & 1 || code_words > (end - strings - 4) / 4)
            return false;
        entry->offset = (uint32_t)strings + 4;
        entry->words = code_words;
        if (children) {
            entry->group = group;
            entry->exclusive = state->entries[group].exclusive;
            children--;
        }
        offset = end;
    }
    return children == 0;
}
static bool read_selections(CheatState *state, const char *root, const char *suffix)
{
    char path[PATH_CAP], key[PATH_CAP], database[PATH_CAP];
    if (!selection_path(path, root, state->content, suffix) ||
        !reference(key, root, state->content) || !reference(database, root, state->database))
        return false;
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    bool version = false, content = false, source = false, code = false, crc = false,
         record = false;
    bool selected[CHEAT_ENTRY_CAP] = {false}, valid = true;
    char line[PATH_CAP + 32];
    size_t total = 0;
    while (fgets(line, sizeof line, file)) {
        total += strlen(line);
        if (total > 8192 || (!strchr(line, '\n') && !feof(file))) {
            valid = false;
            break;
        }
        line[strcspn(line, "\r\n")] = 0;
        char *value = strchr(line, '=');
        if (!value)
            continue;
        *value++ = 0;
        char *end;
        unsigned long number = strtoul(value, &end, 16);
        if (!strcmp(line, "version"))
            version = !strcmp(value, "1");
        else if (!strcmp(line, "content"))
            content = !strcmp(value, key);
        else if (!strcmp(line, "database"))
            source = !strcmp(value, database);
        else if (!strcmp(line, "code"))
            code = *value && !*end && number == state->code;
        else if (!strcmp(line, "rom_crc"))
            crc = *value && !*end && number == state->rom_crc;
        else if (!strcmp(line, "record_crc"))
            record = *value && !*end && number == state->record_crc;
        else if (!strcmp(line, "enabled")) {
            if (!*value || *end || number >= (unsigned)state->count ||
                state->entries[number].folder)
                valid = false;
            else
                selected[number] = true;
        }
    }
    valid = valid && !ferror(file) && version && content && source && code && crc && record;
    fclose(file);
    if (valid) {
        for (int i = 0; i < state->count; i++)
            if (selected[i])
                cheats_toggle(state, i); /* Also enforces exclusive folders. */
    }
    return valid;
}
bool cheats_load(CheatState *state, const char *root, const char *content)
{
    memset(state, 0, sizeof *state);
    char key[PATH_CAP];
    if (!reference(key, root, content))
        return false;
    path_copy(state->content, content, PATH_CAP);
    FILE *rom = fopen(content, "rb");
    unsigned char header[512];
    bool read = rom && fread(header, 1, sizeof header, rom) == sizeof header;
    if (rom)
        fclose(rom);
    if (!read) {
        path_copy(state->status, "Cannot read the title header", sizeof state->status);
        return false;
    }
    state->code = le32(header + 12);
    state->rom_crc = cheats_crc(header, sizeof header);
    const char *sources[] = {"_nds/dsi-frontend/extras/usrcheat.dat",
                             "_nds/TWiLightMenu/extras/usrcheat.dat",
                             "_nds/akmenunext/cheats/usrcheat.dat"};
    FILE *db = NULL;
    for (unsigned i = 0; i < sizeof sources / sizeof sources[0]; i++)
        if (path_join(state->database, root, sources[i]) && (db = fopen(state->database, "rb")))
            break;
    if (!db) {
        path_copy(state->status, "No usrcheat.dat found in supported extras/cheats folders",
                  sizeof state->status);
        return false;
    }
    unsigned char signature[12], index[16], next[16];
    bool valid = fread(signature, 1, 12, db) == 12 && !memcmp(signature, "R4 CheatCode", 12) &&
                 !fseek(db, 0, SEEK_END);
    long file_bytes = valid ? ftell(db) : -1;
    valid = valid && file_bytes >= 0x120 && !fseek(db, 0x100, SEEK_SET) &&
            fread(index, 1, 16, db) == 16;
    uint64_t start = 0, bytes = 0;
    for (unsigned i = 0; valid && i < 262144 && le64(index + 8); i++) {
        uint64_t current = le64(index + 8);
        if (current < 0x120 || current >= (uint64_t)file_bytes || fread(next, 1, 16, db) != 16) {
            valid = false;
            break;
        }
        uint64_t limit = le64(next + 8);
        if (limit && (limit <= current || limit > (uint64_t)file_bytes)) {
            valid = false;
            break;
        }
        if (le32(index) == state->code && le32(index + 4) == state->rom_crc) {
            start = current;
            bytes = (limit ? limit : (uint64_t)file_bytes) - current;
            break;
        }
        memcpy(index, next, sizeof index);
    }
    valid = valid && start && bytes && bytes <= CHEAT_RECORD_CAP;
    if (valid) {
        state->record = malloc((size_t)bytes);
        state->record_bytes = (size_t)bytes;
        valid = state->record && !fseek(db, (long)start, SEEK_SET) &&
                fread(state->record, 1, (size_t)bytes, db) == bytes && parse_record(state);
    }
    fclose(db);
    if (!valid) {
        free(state->record);
        state->record = NULL;
        state->count = 0;
        path_copy(state->status,
                  start ? "Unsupported or malformed cheat record"
                        : "No matching game code and header CRC in database",
                  sizeof state->status);
        return false;
    }
    state->record_crc = cheats_crc(state->record, state->record_bytes);
    if (!read_selections(state, root, "ini"))
        read_selections(state, root, "bak");
    return true;
}
void cheats_free(CheatState *state)
{
    free(state->record);
    state->record = NULL;
    state->count = 0;
}
void cheats_toggle(CheatState *state, int index)
{
    if (index < 0 || index >= state->count)
        return;
    CheatEntry *entry = &state->entries[index];
    if (entry->folder) {
        entry->expanded = !entry->expanded;
        return;
    }
    if (!entry->words)
        return;
    bool selected = !entry->selected;
    if (selected && entry->exclusive)
        for (int i = 0; i < state->count; i++)
            if (state->entries[i].group == entry->group)
                state->entries[i].selected = false;
    entry->selected = selected;
}
bool cheats_payload(const CheatState *state, unsigned char *output, size_t *bytes)
{
    size_t written = 0;
    for (int i = 0; i < state->count; i++) {
        const CheatEntry *entry = &state->entries[i];
        if (!entry->selected || entry->folder)
            continue;
        size_t length = (size_t)entry->words * 4;
        if (entry->offset > state->record_bytes || length > state->record_bytes - entry->offset ||
            written + length > CHEAT_PAYLOAD_CAP - 4)
            return false;
        memcpy(output + written, state->record + entry->offset, length);
        written += length;
    }
    const unsigned char end[4] = {0, 0, 0, 0xCF};
    memcpy(output + written, end, sizeof end);
    *bytes = written + sizeof end;
    return true;
}
bool cheats_save(const CheatState *state, const char *root)
{
    char current[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP], key[PATH_CAP], db[PATH_CAP],
        dir[PATH_CAP];
    if (!state->record || !selection_path(current, root, state->content, "ini") ||
        !selection_path(temporary, root, state->content, "tmp") ||
        !selection_path(backup, root, state->content, "bak") ||
        !reference(key, root, state->content) || !reference(db, root, state->database) ||
        !path_prepare_preferences_directory(root) || !path_preferences_file(dir, root, "games"))
        return false;
    struct stat status;
    if (mkdir(dir, 0777) && (stat(dir, &status) || !S_ISDIR(status.st_mode)))
        return false;
    /* A hash collision or an unrecognized current record must not overwrite
     * another title's saved choices. Database revisions may change normally. */
    FILE *current_file = fopen(current, "r");
    if (current_file) {
        bool matched = false;
        char line[PATH_CAP + 32];
        size_t total = 0;
        while (fgets(line, sizeof line, current_file)) {
            total += strlen(line);
            if (total > 8192)
                break;
            line[strcspn(line, "\r\n")] = 0;
            if (!strncmp(line, "content=", 8))
                matched = !strcmp(line + 8, key);
        }
        matched = matched && !ferror(current_file) && total <= 8192;
        fclose(current_file);
        if (!matched)
            return false;
    }
    FILE *file = fopen(temporary, "w");
    if (!file)
        return false;
    fprintf(file,
            "version=1\ncontent=%s\ndatabase=%s\ncode=%08lX\nrom_crc=%08lX\nrecord_crc=%08lX\n",
            key, db, (unsigned long)state->code, (unsigned long)state->rom_crc,
            (unsigned long)state->record_crc);
    for (int i = 0; i < state->count; i++)
        if (state->entries[i].selected && !state->entries[i].folder)
            fprintf(file, "enabled=%X\n", i);
    bool written = !ferror(file);
    if (fclose(file))
        written = false;
    if (!written) {
        remove(temporary);
        return false;
    }
    if (path_commit_file(temporary, current, backup) != FILE_COMMIT_OK)
        return false;
    return true;
}
