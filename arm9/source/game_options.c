/* Sparse per-title options and defaults; existing game files are never edited. */
#include "game_options.h"
#include "paths.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *keys[] = {"mode",     "cpu",  "vram",      "dma",   "async",
                             "language", "slot", "save_size", "hotkey"};
static const int minimum[] = {-1, -1, -1, -1, -1, -1, 0, -1, 0};
static const int maximum[] = {1, 1, 1, 1, 1, 7, 9, 10, 5};
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
static bool read_record(const char *file_path, const char *key, GameOptions *options)
{
    FILE *file = fopen(file_path, "r");
    if (!file)
        return false;
    bool valid = true, version = false, identity = false;
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
            identity = !strcmp(value, key);
        else
            for (int i = 0; i < GAME_OPTION_COUNT; i++) {
                if (strcmp(line, keys[i]))
                    continue;
                char *end;
                errno = 0;
                long number = strtol(value, &end, 10);
                if (errno || end == value || *end ||
                    (number != GAME_INHERIT && (number < minimum[i] || number > maximum[i])))
                    valid = false;
                else
                    options->values[i] = (int)number;
            }
    }
    valid = valid && !ferror(file) && version && identity;
    fclose(file);
    return valid;
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
            read_record(record, key, &candidate)) {
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
    if (!stat(current, &status) && !read_record(current, key, &existing))
        return false;
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
const char *game_option_label(int option)
{
    static const char *labels[] = {"Run mode",  "CPU boost",   "VRAM boost",
                                   "Card DMA",  "Async reads", "Language",
                                   "Save slot", "Save size",   "In-game menu"};
    return option >= 0 && option < GAME_OPTION_COUNT ? labels[option] : "";
}
void game_option_value(const GameOptions *options, int option, char output[48])
{
    int value = options->values[option];
    const char *text = NULL;
    if (value == GAME_INHERIT)
        text = "Use default";
    else if (value == -1)
        text = "Auto";
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
