/* Sparse per-title options and defaults; existing game files are never edited. */
#include "game_options.h"
#include "paths.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *keys[] = {"mode",        "cpu",   "vram",      "dma",    "async",
                             "language",    "slot",  "save_size", "hotkey", "overlay_language",
                             "screen_swap", "manual"};
static const int minimum[] = {-1, -1, -1, -1, -1, -1, 0, -1, 0, -1, -1, -1};
static const int maximum[] = {1, 1, 1, 1, 1, 7, 9, 10, 5, 30, 2, 1};
void game_options_defaults(GameOptions *options, bool inherit)
{
    for (int i = 0; i < GAME_OPTION_COUNT; i++)
        options->values[i] = inherit ? GAME_INHERIT : minimum[i];
}
static bool record_identity(const char *root, const char *path, char key[PATH_CAP])
{
    if (!*path) {
        path_copy(key, "defaults", PATH_CAP);
        return true;
    }
    if (!path_is_within_root(path, root))
        return false;
    const char *relative = path + strlen(root);
    while (*relative == '/')
        relative++;
    return *relative && path_resolve_sd(key, "sd:/", relative);
}
static bool record_path(char output[PATH_CAP], const char *root, const char *key,
                        const char *suffix)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *c = (const unsigned char *)key; *c; c++) {
        hash ^= *c;
        hash *= UINT64_C(1099511628211);
    }
    char relative[64];
    snprintf(relative, sizeof relative, "games/%016" PRIx64 ".%s", hash, suffix);
    return path_preferences_file(output, root, relative);
}
/* Only completely inspected damaged records may be replaced using a matching backup. */
typedef enum { RECORD_UNREADABLE, RECORD_INVALID, RECORD_PROTECTED, RECORD_VALID } RecordStatus;
static RecordStatus read_record(const char *file_path, const char *key, GameOptions *options)
{
    FILE *file = fopen(file_path, "rb");
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
    bool version = false, identity = false, foreign = false, unsupported = false;
    unsigned seen = 0;
    char *cursor = contents;
    while (*cursor) {
        char *line = cursor, *next = strchr(cursor, '\n');
        cursor = next ? next + 1 : cursor + strlen(cursor);
        if (next)
            *next = 0;
        size_t length = strlen(line);
        if (length >= PATH_CAP + 31) {
            free(contents);
            return RECORD_UNREADABLE;
        }
        if (length && line[length - 1] == '\r')
            line[length - 1] = 0;
        char *value = strchr(line, '=');
        if (!value)
            continue;
        *value++ = 0;
        unsigned field = 0;
        if (!strcmp(line, "version")) {
            version = !strcmp(value, "1");
            unsupported |= *value && !version;
            valid &= version;
        } else if (!strcmp(line, "content")) {
            field = 1;
            identity = !strcmp(value, key);
            foreign |= *value && !identity;
        } else {
            int i;
            for (i = 0; i < GAME_OPTION_COUNT && strcmp(line, keys[i]); i++) {
            }
            if (i == GAME_OPTION_COUNT)
                continue;
            field = (unsigned)i + 2;
            char *end;
            errno = 0;
            long number = strtol(value, &end, 10);
            if (errno || end == value || *end ||
                (number != GAME_INHERIT && (number < minimum[i] || number > maximum[i])))
                valid = false;
            else
                options->values[i] = (int)number;
        }
        valid &= !(seen & (1u << field));
        seen |= 1u << field;
    }
    free(contents);
    if (foreign || unsupported)
        return RECORD_PROTECTED;
    return valid && version && identity ? RECORD_VALID : RECORD_INVALID;
}

bool game_options_read(const char *root, const char *path, GameOptions *options)
{
    game_options_defaults(options, true);
    char key[PATH_CAP], record[PATH_CAP];
    if (!record_identity(root, path, key))
        return false;
    for (int backup = 0; backup < 2; backup++) {
        GameOptions candidate;
        game_options_defaults(&candidate, true);
        if (record_path(record, root, key, backup ? "bak" : "ini") &&
            read_record(record, key, &candidate) == RECORD_VALID) {
            *options = candidate;
            return true;
        }
    }
    return false;
}
bool game_options_write(const char *root, const char *path, const GameOptions *options)
{
    for (int i = 0; i < GAME_OPTION_COUNT; i++)
        if (options->values[i] != GAME_INHERIT &&
            (options->values[i] < minimum[i] || options->values[i] > maximum[i]))
            return false;
    char key[PATH_CAP], current[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP], dir[PATH_CAP];
    if (!record_identity(root, path, key) || !path_prepare_preferences_directory(root) ||
        !path_preferences_file(dir, root, "games"))
        return false;
    struct stat status;
    if (mkdir(dir, 0777) && (errno != EEXIST || stat(dir, &status) || !S_ISDIR(status.st_mode)))
        return false;
    if (!record_path(current, root, key, "ini") || !record_path(temporary, root, key, "tmp") ||
        !record_path(backup, root, key, "bak"))
        return false;
    GameOptions existing;
    bool exists = !stat(current, &status), recovered = false;
    if (!exists && errno != ENOENT)
        return false;
    if (exists) {
        RecordStatus record = read_record(current, key, &existing);
        if (record != RECORD_VALID) {
            if (record != RECORD_INVALID || read_record(backup, key, &existing) != RECORD_VALID)
                return false;
            recovered = true;
        }
    }
    FILE *file = fopen(temporary, "w");
    if (!file)
        return false;
    fprintf(file, "version=1\ncontent=%s\n", key);
    for (int i = 0; i < GAME_OPTION_COUNT; i++)
        fprintf(file, "%s=%d\n", keys[i], options->values[i]);
    bool written = !ferror(file);
    if (fclose(file))
        written = false;
    if (!written) {
        remove(temporary);
        return false;
    }
    /* Preserve the readable recovery copy rather than rotating damaged data over it. */
    if (recovered && remove(current) && errno != ENOENT) {
        remove(temporary);
        return false;
    }
    if (path_commit_file(temporary, current, backup) != FILE_COMMIT_OK)
        return false;
    return true;
}
void game_options_effective(const char *root, const char *path, GameOptions *options)
{
    game_options_defaults(options, false);
    GameOptions record;
    game_options_read(root, "", &record);
    for (int i = 0; i < GAME_OPTION_COUNT; i++)
        if (record.values[i] != GAME_INHERIT)
            options->values[i] = record.values[i];
    if (!*path)
        return;
    game_options_read(root, path, &record);
    for (int i = 0; i < GAME_OPTION_COUNT; i++)
        if (record.values[i] != GAME_INHERIT)
            options->values[i] = record.values[i];
}
unsigned game_option_hotkey(int choice)
{
    static const unsigned masks[] = {0x284, 0x308, 0x304, 0x383, 0xB82, 0xF03};
    return masks[choice >= 0 && choice < 6 ? choice : 0];
}
unsigned game_option_screen_swap(int choice)
{
    static const unsigned masks[] = {0x740, 0x340, 0x700};
    return masks[choice >= 0 && choice < 3 ? choice : 0];
}
const char *game_option_overlay_language(int choice)
{
    static const char *codes[] = {"en",    "ja", "fr",    "de", "it", "es",  "zh-CN", "ko",
                                  "zh-TW", "pt", "pt-BR", "ar", "ca", "cs",  "da",    "el",
                                  "fi",    "he", "hu",    "id", "nl", "no",  "pl",    "ro",
                                  "ru",    "ry", "sv",    "tr", "uk", "val", "vi"};
    return choice >= 0 && choice < 31 ? codes[choice] : "";
}
const char *game_option_label(int option)
{
    static const char *labels[] = {"Run mode",     "CPU boost",     "VRAM boost",  "Card DMA",
                                   "Async reads",  "Language",      "Save slot",   "Save size",
                                   "In-game menu", "Menu language", "Screen swap", "Game manual"};
    return option >= 0 && option < GAME_OPTION_COUNT ? labels[option] : "";
}
void game_option_value(const GameOptions *options, int option, char output[48])
{
    int value = options->values[option];
    const char *text = NULL;
    if (value == GAME_INHERIT)
        text = "Use default";
    else if (value == -1)
        text = option >= GAME_OVERLAY_LANGUAGE ? "Keep INI" : "Auto";
    else if (option == GAME_MODE)
        text = value ? "DSi" : "DS";
    else if (option <= GAME_ASYNC)
        text = value ? "On" : "Off";
    else if (option == GAME_LANGUAGE) {
        static const char *languages[] = {"Japanese", "English", "French",  "German",
                                          "Italian",  "Spanish", "Chinese", "Korean"};
        text = languages[value];
    } else if (option == GAME_SAVE_SIZE) {
        if (!value)
            text = "No save";
        else {
            uint32_t bytes = game_option_save_bytes(value);
            snprintf(output, 48, bytes < 1024 ? "%lu bytes" : "%lu KiB",
                     (unsigned long)(bytes < 1024 ? bytes : bytes / 1024));
            return;
        }
    } else if (option == GAME_HOTKEY) {
        static const char *hotkeys[] = {"L+Down+Select", "L+R+Start",    "L+R+Select",
                                        "L+R+A+B+Down",  "L+R+B+Y+Down", "L+R+A+B+X+Y"};
        text = hotkeys[value];
    } else if (option == GAME_OVERLAY_LANGUAGE) {
        static const char *names[] = {
            "English",     "Japanese",   "French",      "German",     "Italian",      "Spanish",
            "Chinese (S)", "Korean",     "Chinese (T)", "Portuguese", "Brazilian PT", "Arabic",
            "Catalan",     "Czech",      "Danish",      "Greek",      "Finnish",      "Hebrew",
            "Hungarian",   "Indonesian", "Dutch",       "Norwegian",  "Polish",       "Romanian",
            "Russian",     "Ryukyuan",   "Swedish",     "Turkish",    "Ukrainian",    "Valencian",
            "Vietnamese"};
        text = names[value];
    } else if (option == GAME_SCREEN_SWAP) {
        static const char *hotkeys[] = {"L+R+Up+X", "L+R+Up", "L+R+X"};
        text = hotkeys[value];
    } else if (option == GAME_MANUAL) {
        text = value ? "Sidecar .txt" : "Off";
    } else {
        snprintf(output, 48, "%d", value);
        return;
    }
    path_copy(output, text, 48);
}
/* Keep adjustment bounds in one place alongside the persisted field contract. */
void game_editor_adjust(App *app, int direction)
{
    GameEditor *editor = &app->game_editor;
    int option = editor->selected;
    int low = editor->defaults ? minimum[option] : GAME_INHERIT;
    int value = editor->options.values[option] + (direction < 0 ? -1 : 1);
    /* -1 is reserved for Auto only on options that support it. */
    if (value == -1 && minimum[option] == 0)
        value += direction < 0 ? -1 : 1;
    if (value < low)
        value = maximum[option];
    if (value > maximum[option])
        value = low;
    editor->options.values[option] = value;
    editor->status[0] = 0;
}
