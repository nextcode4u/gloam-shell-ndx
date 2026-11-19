/* Exercise FAT save transactions and recovery from malformed or interrupted reads. */
#include "paths.h"
#include "preferences.h"
#include "tabs.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static App app;
static bool short_read;
static FILE *state_stream;
static int state_lines;
size_t __real_fread(void *, size_t, size_t, FILE *);
char *__real_fgets(char *, int, FILE *);
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
char *__real___fgets_chk(char *, size_t, int, FILE *);
#endif
static bool deny_open, deny_close, deny_remove, deny_rotate, deny_install, deny_restore;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_remove(const char *);
int __real_rename(const char *, const char *);
static bool named(const char *path, const char *name)
{
    const char *base = strrchr(path, '/');
    return !strcmp(base ? base + 1 : path, name);
}
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (deny_open && named(path, "state.tmp") && *mode == 'w') {
        errno = EROFS;
        return NULL;
    }
    FILE *file = __real_fopen(path, mode);
    if (short_read && named(path, "state.ini") && *mode == 'r') {
        state_stream = file;
        state_lines = 0;
    }
    return file;
}
size_t __wrap_fread(void *buffer, size_t size, size_t count, FILE *file)
{
    if (file == state_stream && count > 16)
        count = 16;
    return __real_fread(buffer, size, count, file);
}
char *__wrap_fgets(char *buffer, int size, FILE *file)
{
    if (file == state_stream && ++state_lines > 2)
        return NULL;
    return __real_fgets(buffer, size, file);
}
/* Fortified glibc builds route these reads through checked entry points. */
#ifdef __GLIBC__
size_t __wrap___fread_chk(void *buffer, size_t capacity, size_t size, size_t count, FILE *file)
{
    if (file == state_stream && count > 16)
        count = 16;
    return __real___fread_chk(buffer, capacity, size, count, file);
}
char *__wrap___fgets_chk(char *buffer, size_t capacity, int size, FILE *file)
{
    if (file == state_stream && ++state_lines > 2)
        return NULL;
    return __real___fgets_chk(buffer, capacity, size, file);
}
#endif
int __wrap_fclose(FILE *file)
{
    if (file == state_stream)
        state_stream = NULL;
    int result = __real_fclose(file);
    if (deny_close) {
        errno = ENOSPC;
        return EOF;
    }
    return result;
}
int __wrap_remove(const char *path)
{
    if (deny_remove && named(path, "state.bak")) {
        errno = EACCES;
        return -1;
    }
    return __real_remove(path);
}
int __wrap_rename(const char *from, const char *to)
{
    if ((deny_rotate && named(from, "state.ini")) || (deny_install && named(from, "state.tmp")) ||
        (deny_restore && named(from, "state.bak"))) {
        errno = EIO;
        return -1;
    }
    if (access(to, F_OK) == 0) {
        errno = EEXIST; /* FatFs does not implement POSIX destination replacement. */
        return -1;
    }
    return __real_rename(from, to);
}
static void put(const char *path, const char *text)
{
    FILE *f = __real_fopen(path, "w");
    assert(f && fputs(text, f) >= 0);
    assert(__real_fclose(f) == 0);
}
static void expect(const char *path, const char *text)
{
    char line[128];
    FILE *f = __real_fopen(path, "r");
    assert(f && fgets(line, sizeof line, f));
    assert(!strcmp(line, text));
    assert(__real_fclose(f) == 0);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    path_copy(app.root, argv[1], sizeof app.root);
    app.storage_ready = true;
    assert(path_prepare_preferences_directory(app.root));
    char current[PATH_CAP], backup[PATH_CAP];
    assert(path_preferences_file(current, app.root, "state.ini"));
    assert(path_preferences_file(backup, app.root, "state.bak"));
    put(current, "original\n");
    put(backup, "older\n");

    deny_open = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "open state.tmp"));
    expect(current, "original\n");
    deny_open = false;
    deny_close = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "write state.tmp"));
    expect(current, "original\n");
    deny_close = false;
    deny_remove = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "remove old state.bak"));
    expect(current, "original\n");
    expect(backup, "older\n");
    deny_remove = false;
    deny_rotate = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "back up state.ini"));
    expect(current, "original\n");
    deny_rotate = false;
    deny_install = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "install state.ini"));
    expect(current, "original\n"); /* Successful rollback. */
    deny_restore = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "kept in state.bak"));
    expect(backup, "original\n");
    assert(access(current, F_OK) != 0);
    deny_restore = false;
    assert(!app_save(&app)); /* Retrying must not delete the sole recovery copy. */
    expect(backup, "original\n");
    assert(app.preferences_dirty);
    deny_install = false;
    assert(app_save(&app) && !app.preferences_dirty && !*app.preferences_error);
    expect(current, "version=1\n");
    expect(backup, "original\n");
    assert(app_save(&app)); /* Repeated saves with FAT rename behavior. */
    expect(backup, "version=1\n");
    assert(path_commit_file(current, current, backup) == FILE_COMMIT_INSPECT);
    expect(current, "version=1\n");
    /* A failed primary read must not apply even its first complete setting. */
    put(current, "version=1\nmusic_volume=99\nshow_hidden=1\n");
    put(backup, "version=1\nshow_hidden=1\n");
    app.audio.music_volume = 37;
    app.show_hidden = false;
    short_read = true;
    preferences_load(&app);
    short_read = false;
    assert(app.show_hidden && app.audio.music_volume == 37);
    app.show_hidden = false;
    deny_close = true;
    preferences_load(&app);
    deny_close = false;
    assert(!app.show_hidden && app.audio.music_volume == 37);

    /* A continuation of an overlong record must never become a setting. */
    FILE *long_file = __real_fopen(current, "w");
    assert(long_file && fputs("version=1\n", long_file) >= 0);
    for (int i = 0; i < 1023; i++)
        assert(fputc('#', long_file) != EOF);
    assert(fputs("show_hidden=1\nmusic_volume=42", long_file) >= 0);
    assert(!__real_fclose(long_file));
    app.show_hidden = false;
    preferences_load(&app);
    assert(!app.show_hidden && app.audio.music_volume == 42);

    /* Binary corruption rejects the entire primary before applying values. */
    FILE *binary = __real_fopen(current, "wb");
    static const char corrupt[] = "version=1\nmusic_volume=99\n\0show_hidden=0\n";
    assert(binary && fwrite(corrupt, 1, sizeof corrupt - 1, binary) == sizeof corrupt - 1);
    assert(!__real_fclose(binary));
    app.audio.music_volume = 37;
    app.show_hidden = false;
    preferences_load(&app);
    assert(app.show_hidden && app.audio.music_volume == 37);
    put(current, "version=1\r\nmusic_volume=43\r\nshow_hidden=0");
    preferences_load(&app);
    assert(!app.show_hidden && app.audio.music_volume == 43);
    put(current, "version=1");
    app.audio.music_volume = 29;
    preferences_load(&app);
    assert(app.audio.music_volume == 29); /* Header-only files remain valid. */
    put(current, "");
    app.show_hidden = false;
    preferences_load(&app);
    assert(app.show_hidden); /* Empty primary uses the backup. */
    FILE *oversized = __real_fopen(current, "wb");
    assert(oversized && fputs("version=1\nmusic_volume=99\n", oversized) >= 0);
    assert(!fseek(oversized, 128 * 1024, SEEK_SET) && fputc('x', oversized) != EOF);
    assert(!__real_fclose(oversized));
    app.audio.music_volume = 29;
    preferences_load(&app);
    assert(app.audio.music_volume == 29);
    /* Reject numeric prefixes/overflow without discarding later valid records. */
    static const char *invalid_numbers[] = {"1junk",
                                            "4294967297",
                                            "18446744073709551617",
                                            "999999999999999999999999999999",
                                            "1.0",
                                            "1e0",
                                            "0x1",
                                            "--1",
                                            "+",
                                            "",
                                            "-4294967295"};
    char tabs_path[PATH_CAP];
    assert(path_preferences_file(tabs_path, app.root, "tabs.ini"));
    put(tabs_path, "[tabs]\nversion=1\n[tab.books]\npath=roms/Books\n");
    tabs_load(&app);
    int books = tabs_find(&app, "books");
    assert(books >= CATEGORY_COUNT);
    for (size_t i = 0; i < sizeof invalid_numbers / sizeof *invalid_numbers; i++) {
        char state[2048];
        const char *number = invalid_numbers[i];
        snprintf(state, sizeof state,
                 "version=1\nmusic_volume=%s\nshow_hidden=%s\nicon_size=%s\n"
                 "simple_settings_lock=%s\nfolder_batch=games,%s\n"
                 "tab_detail=games,%s\ntab_display=games,%s,0,1\n"
                 "location=1,%s,0,%s\ntab_location=books,%s,0,%s\neffects_volume=41\n",
                 number, number, number, number, number, number, number, number,
                 tabs_browse_root(&app, CATEGORY_GAMES), number, tabs_browse_root(&app, books));
        put(current, state);
        app.audio.music_volume = 37;
        app.show_hidden = false;
        app.icon_size = ICON_SMALL;
        app.simple_settings_lock = false;
        app.locations[CATEGORY_GAMES].batch = 7;
        app.locations[CATEGORY_GAMES].selected = 11;
        app.locations[books].selected = 13;
        app.tab_details[CATEGORY_GAMES] = 0;
        app.tab_display[CATEGORY_GAMES].saved = false;
        preferences_load(&app);
        assert(app.audio.music_volume == 37 && !app.show_hidden && app.icon_size == ICON_SMALL);
        assert(!app.simple_settings_lock && app.locations[CATEGORY_GAMES].batch == 7);
        assert(app.locations[CATEGORY_GAMES].selected == 11 && app.locations[books].selected == 13);
        assert(!app.tab_details[CATEGORY_GAMES] && !app.tab_display[CATEGORY_GAMES].saved);
        assert(app.audio.effects_volume == 41);
    }
    static const char *invalid_display[] = {"0,4294967297,1", "0,0,4294967297", "0,-1,1",
                                            "0,0,-1",         "0,0,1junk",      "0,0,1,0"};
    for (size_t i = 0; i < sizeof invalid_display / sizeof *invalid_display; i++) {
        char state[256];
        snprintf(state, sizeof state, "version=1\ntab_display=games,%s\n", invalid_display[i]);
        put(current, state);
        app.tab_display[CATEGORY_GAMES].saved = false;
        preferences_load(&app);
        assert(!app.tab_display[CATEGORY_GAMES].saved);
    }
    static const char *invalid_ids[] = {
        "", "xyz", "1234junk", "100000000", "-1", "10000000000000000", "\v-1", "0x100000000"};
    for (size_t i = 0; i < sizeof invalid_ids / sizeof *invalid_ids; i++) {
        char state[256];
        snprintf(state, sizeof state, "version=1\ninstalled_selection=%s\n", invalid_ids[i]);
        put(current, state);
        app.installed_selected_id = 0x12345678;
        preferences_load(&app);
        assert(app.installed_selected_id == 0x12345678);
    }
    put(current, "version=1\nmusic_volume= +42 \t\nwallpaper_strength=-1\n"
                 "installed_selection=ffffffff\ntab_detail=games,1 \n"
                 "folder_batch=games,8\ntab_display=games,0,1,0\n");
    preferences_load(&app);
    assert(app.audio.music_volume == 42 && app.wallpaper_strength == -1);
    assert(app.installed_selected_id == UINT32_MAX && app.tab_details[CATEGORY_GAMES] == 1);
    assert(app.locations[CATEGORY_GAMES].batch == 8 && app.tab_display[CATEGORY_GAMES].saved);
    puts("PASS: strict numeric fields, complete settings reads, corrupt-record recovery, FAT save "
         "rotation, "
         "read-only/full-disk errors, rollback and recovery-copy "
         "preservation");
}
