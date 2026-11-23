#include "app.h"
#include "browser.h"
#include "cheats.h"
#include "paths.h"
#include "renderer.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static const char *watched_paths[2];
static FILE *watched_streams[2];
static size_t read_limit = (size_t)-1;
static int lines_left = -1;
static bool fail_presence, nonregular_presence, fail_close, fail_open, fail_metadata;
int __real_fstat(int, struct stat *);
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_stat(const char *, struct stat *);
size_t __real_fread(void *, size_t, size_t, FILE *);
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
#endif
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (fail_open && watched_paths[0] && !strcmp(path, watched_paths[0]) && *mode == 'r') {
        errno = EIO;
        return NULL;
    }
    FILE *file = __real_fopen(path, mode);
    for (unsigned i = 0; i < 2; i++)
        if (watched_paths[i] && !strcmp(path, watched_paths[i]) && *mode == 'r')
            watched_streams[i] = file;
    return file;
}
static bool watched(FILE *file)
{
    return file && (file == watched_streams[0] || file == watched_streams[1]);
}
char *__real_fgets(char *, int, FILE *);
char *__wrap_fgets(char *text, int capacity, FILE *file)
{
    if (watched(file) && lines_left >= 0) {
        if (!lines_left)
            return NULL;
        lines_left--;
    }
    return __real_fgets(text, capacity, file);
}
#ifdef __GLIBC__
char *__wrap___fgets_chk(char *text, size_t capacity, int count, FILE *file)
{
    assert(count > 0 && (size_t)count <= capacity);
    return __wrap_fgets(text, count, file);
}
#endif
size_t __wrap_fread(void *buffer, size_t size, size_t count, FILE *file)
{
    if (watched(file) && count > read_limit)
        count = read_limit;
    return __real_fread(buffer, size, count, file);
}
#ifdef __GLIBC__
size_t __wrap___fread_chk(void *buffer, size_t capacity, size_t size, size_t count, FILE *file)
{
    if (watched(file) && count > read_limit)
        count = read_limit;
    return __real___fread_chk(buffer, capacity, size, count, file);
}
#endif
int __wrap_fclose(FILE *file)
{
    bool deny = watched(file) && fail_close;
    for (unsigned i = 0; i < 2; i++)
        if (file == watched_streams[i])
            watched_streams[i] = NULL;
    int result = __real_fclose(file);
    if (deny) {
        errno = EIO;
        return EOF;
    }
    return result;
}
int __wrap_fstat(int descriptor, struct stat *status)
{
    for (unsigned i = 0; i < 2; i++)
        if (fail_metadata && watched_streams[i] && fileno(watched_streams[i]) == descriptor) {
            errno = EIO;
            return -1;
        }
    return __real_fstat(descriptor, status);
}
int __wrap_stat(const char *path, struct stat *status)
{
    if (fail_presence && watched_paths[0] && !strcmp(path, watched_paths[0])) {
        errno = EIO;
        return -1;
    }
    int result = __real_stat(path, status);
    if (!result && nonregular_presence && watched_paths[0] && !strcmp(path, watched_paths[0]))
        status->st_mode = (status->st_mode & ~S_IFMT) | S_IFDIR;
    return result;
}
static CheatState state, restored;
static App app;
static uint16_t top[256 * 192], bottom[256 * 192];
static void read_database(const char *path, unsigned char data[4096], size_t *bytes)
{
    FILE *f = fopen(path, "rb");
    assert(f);
    *bytes = fread(data, 1, 4096, f);
    assert(!ferror(f));
    fclose(f);
}
static void write_file(const char *path, const void *data, size_t bytes)
{
    FILE *f = fopen(path, "wb");
    assert(f && fwrite(data, 1, bytes, f) == bytes && !fclose(f));
}
static void ownership_failures(const char *root, const char *game, const char *shared,
                               const unsigned char *payload, size_t bytes)
{
    char snapshot[PATH_CAP], wide[PATH_CAP], error[160];
    assert(path_preferences_file(snapshot, root, "last-cheatData.bin"));
    assert(path_join(wide, root, "_nds/nds-bootstrap/wideCheatData.bin"));
    unsigned char foreign[4096], actual[4096];
    size_t actual_bytes;
    assert(bytes > 12 && bytes <= sizeof foreign);
    memcpy(foreign, payload, bytes);
    foreign[12] ^= 0xff; /* The first eight bytes still match our ownership snapshot. */
    write_file(shared, foreign, bytes);
    watched_paths[0] = shared;
    watched_paths[1] = snapshot;
    read_limit = 8;
    assert(!cheats_backend_available(root, error));
    assert(!cheats_backend_commit(root, game, error));
    read_limit = (size_t)-1;
    read_database(shared, actual, &actual_bytes);
    assert(actual_bytes == bytes && !memcmp(actual, foreign, bytes));
    read_database(snapshot, actual, &actual_bytes);
    assert(actual_bytes == bytes && !memcmp(actual, payload, bytes));
    write_file(shared, payload, bytes);
    read_limit = 8;
    assert(!cheats_backend_available(root, error)); /* Even our own prefix proves no ownership. */
    read_limit = (size_t)-1;
    fail_close = true;
    assert(!cheats_backend_available(root, error));
    fail_close = false;
    assert(cheats_backend_available(root, error));
    watched_paths[0] = wide;
    watched_paths[1] = NULL;
    const unsigned char empty[4] = {0, 0, 0, 0xcf};
    for (unsigned i = 0; i < 3; i++) {
        write_file(wide, i == 2 ? (const void *)"foreign wide" : empty, i == 2 ? 12 : i * 4);
        if (i < 2)
            assert(cheats_backend_available(root, error));
        if (i) {
            read_limit = 0;
            assert(!cheats_backend_available(root, error));
            assert(!cheats_backend_commit(root, game, error));
            read_limit = (size_t)-1;
            read_database(wide, actual, &actual_bytes);
            assert(actual_bytes == (i == 2 ? 12 : 4));
            assert(!memcmp(actual, i == 2 ? (const void *)"foreign wide" : empty, actual_bytes));
        }
    }
    assert(!remove(wide));
    watched_paths[0] = watched_paths[1] = NULL;
    assert(cheats_backend_available(root, error));
}
static void selection_write_guards(const char *root)
{
    char folder[PATH_CAP], current[PATH_CAP] = "", backup[PATH_CAP], key[PATH_CAP];
    assert(path_preferences_file(folder, root, "games"));
    DIR *directory = opendir(folder);
    assert(directory);
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        size_t length = strlen(entry->d_name);
        if (length > 11 && !strcmp(entry->d_name + length - 11, ".cheats.ini")) {
            assert(!*current && path_join(current, folder, entry->d_name));
        }
    }
    assert(!closedir(directory) && *current);
    path_copy(backup, current, PATH_CAP);
    strcpy(backup + strlen(backup) - 3, "bak");
    assert(path_resolve_sd(key, "sd:/", state.content + strlen(root)));
    unsigned char original[4096], actual[4096];
    size_t bytes, actual_bytes;
    read_database(current, original, &bytes);
    write_file(backup, original, bytes);
    cheats_toggle(&state, 3); /* A rejected save must not publish the new choices. */
    watched_paths[0] = current;
    for (unsigned fault = 0; fault < 4; fault++) {
        fail_open = fault == 0;
        fail_close = fault == 1;
        fail_metadata = fault == 2;
        read_limit = fault == 3 ? 8 : (size_t)-1;
        bool saved = cheats_save(&state, root);
        fail_open = fail_close = fail_metadata = false;
        read_limit = (size_t)-1;
        assert(!saved);
        read_database(current, actual, &actual_bytes);
        assert(actual_bytes == bytes && !memcmp(actual, original, bytes));
        read_database(backup, actual, &actual_bytes);
        assert(actual_bytes == bytes && !memcmp(actual, original, bytes));
    }
    const char *bad[] = {
        "version=2\ncontent=%s\n", "content=%s\n", "version=1\ncontent=sd:/other.nds\ncontent=%s\n",
        "version=2\nversion=1\ncontent=%s\n", "version=1\ncontent=%s\ncontent=sd:/other.nds\n"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        char text[2048];
        int length = snprintf(text, sizeof text, bad[i], key);
        assert(length > 0 && (size_t)length < sizeof text);
        write_file(current, text, (size_t)length);
        assert(!cheats_save(&state, root));
        read_database(current, actual, &actual_bytes);
        assert(actual_bytes == (size_t)length && !memcmp(actual, text, actual_bytes));
        read_database(backup, actual, &actual_bytes);
        assert(actual_bytes == bytes && !memcmp(actual, original, bytes));
    }
    char text[2048];
    int length = snprintf(text, sizeof text, "version=1\ncontent=%s", key);
    assert(length > 0 && (size_t)length + 8 < sizeof text);
    memcpy(text + length, "\0hidden", 8);
    write_file(current, text, (size_t)length + 8);
    assert(!cheats_save(&state, root));
    read_database(current, actual, &actual_bytes);
    assert(actual_bytes == (size_t)length + 8 && !memcmp(actual, text, actual_bytes));
    /* Loading also rejects ambiguous identity, hidden bytes and incomplete I/O.
     * Hide the recovery copy so a successful fallback cannot mask this test. */
    assert(!remove(backup));
    write_file(current, original, bytes);
    for (int fault = 0; fault < 3; fault++) {
        fail_close = fault == 0;
        fail_metadata = fault == 1;
        lines_left = fault == 2 ? 6 : -1;
        assert(cheats_load(&restored, root, state.content));
        for (int i = 0; i < restored.count; i++)
            assert(!restored.entries[i].selected);
        cheats_free(&restored);
    }
    fail_close = fail_metadata = false;
    lines_left = -1;
    const char *extra[] = {"version=1\n", "content=sd:/other.nds\n", "enabled=+1\n",
                           "enabled=100000000\n"};
    for (unsigned i = 0; i < sizeof extra / sizeof *extra; i++) {
        memcpy(text, original, bytes);
        strcpy(text + bytes, extra[i]);
        write_file(current, text, bytes + strlen(extra[i]));
        assert(cheats_load(&restored, root, state.content));
        for (int j = 0; j < restored.count; j++)
            assert(!restored.entries[j].selected);
        cheats_free(&restored);
    }
    memcpy(text, original, bytes);
    memcpy(text + bytes, "\0hidden", 7);
    write_file(current, text, bytes + 7);
    assert(cheats_load(&restored, root, state.content) && !restored.entries[2].selected);
    cheats_free(&restored);
    write_file(current, original, bytes);
    assert(cheats_load(&restored, root, state.content) && restored.entries[2].selected);
    cheats_free(&restored);
    /* Old database/revision fields may change when the same title is saved again. */
    length = snprintf(text, sizeof text,
                      "version=1\r\ncontent=%s\r\ndatabase=sd:/old.dat\r\ncode=0\r\nrom_crc=0\r\n"
                      "record_crc=0\r\n",
                      key);
    assert(length > 0 && (size_t)length < sizeof text);
    write_file(current, text, (size_t)length);
    cheats_toggle(&state, 3);
    assert(cheats_save(&state, root));
    watched_paths[0] = NULL;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    char game[PATH_CAP], db[PATH_CAP], shared[PATH_CAP], error[160];
    assert(path_join(game, root, "roms/Cheat parser fixture.nds"));
    assert(path_join(db, root, "_nds/dsi-frontend/extras/usrcheat.dat"));
    unsigned char original[4096], after[4096], payload[CHEAT_PAYLOAD_CAP];
    size_t db_bytes, after_bytes, bytes;
    read_database(db, original, &db_bytes);
    assert(cheats_load(&state, root, game) && state.count == 4);
    assert(state.entries[0].folder && state.entries[0].exclusive);
    assert(state.entries[1].group == 0 && !state.entries[3].selected);
    cheats_toggle(&state, 1);
    assert(state.entries[1].selected);
    cheats_toggle(&state, 2);
    assert(!state.entries[1].selected && state.entries[2].selected);
    cheats_toggle(&state, 3);
    assert(cheats_payload(&state, payload, &bytes) && bytes == 20);
    assert(!memcmp(payload + 16, "\0\0\0\xCF", 4));
    assert(cheats_save(&state, root));
    selection_write_guards(root);
    assert(cheats_load(&restored, root, game) && restored.entries[2].selected &&
           restored.entries[3].selected);
    read_database(db, after, &after_bytes);
    assert(after_bytes == db_bytes && !memcmp(original, after, db_bytes));
    assert(cheats_backend_available(root, error));
    assert(cheats_backend_commit(root, game, error));
    assert(path_join(shared, root, "_nds/nds-bootstrap/cheatData.bin"));
    read_database(shared, after, &after_bytes);
    assert(after_bytes == bytes && !memcmp(payload, after, bytes));
    ownership_failures(root, game, shared, payload, bytes);
    /* A payload changed by another owner is preserved and blocks this adapter. */
    write_file(shared, "foreign", 7);
    assert(!cheats_backend_available(root, error));
    assert(!cheats_backend_commit(root, game, error));
    read_database(shared, after, &after_bytes);
    assert(after_bytes == 7 && !memcmp(after, "foreign", 7));
    write_file(shared, payload, bytes);
    assert(cheats_backend_available(root, error));
    /* Clearing selections publishes only the terminator, with matching ownership.
     * Relaunching must not inherit codes from the preceding enabled launch. */
    cheats_toggle(&state, 2);
    cheats_toggle(&state, 3);
    assert(cheats_save(&state, root));
    watched_paths[0] = shared;
    fail_presence = true;
    assert(!cheats_backend_commit(root, game, error));
    fail_presence = false;
    nonregular_presence = true;
    assert(!cheats_backend_commit(root, game, error));
    nonregular_presence = false;
    watched_paths[0] = NULL;
    read_database(shared, after, &after_bytes);
    assert(after_bytes == bytes && !memcmp(after, payload, bytes));
    assert(cheats_backend_commit(root, game, error));
    read_database(shared, after, &after_bytes);
    assert(after_bytes == 4 && !memcmp(after, "\0\0\0\xCF", 4));
    assert(cheats_backend_available(root, error));
    cheats_toggle(&state, 2);
    cheats_toggle(&state, 3);
    assert(cheats_save(&state, root) && cheats_backend_commit(root, game, error));
    char wide[PATH_CAP], next_payload[PATH_CAP];
    assert(path_join(wide, root, "_nds/nds-bootstrap/wideCheatData.bin"));
    write_file(wide, "foreign wide", 12);
    assert(!cheats_backend_commit(root, game, error) && strstr(error, "widescreen"));
    read_database(wide, after, &after_bytes);
    assert(after_bytes == 12 && !memcmp(after, "foreign wide", 12));
    read_database(shared, after, &after_bytes);
    assert(after_bytes == bytes && !memcmp(after, payload, bytes));
    assert(!remove(wide));
    /* Interrupted staging files are never overwritten or mistaken for ownership. */
    assert(path_join(next_payload, root, "_nds/nds-bootstrap/cheatData.frontend-next"));
    write_file(next_payload, "interrupted", 11);
    assert(!cheats_backend_commit(root, game, error));
    read_database(next_payload, after, &after_bytes);
    assert(after_bytes == 11 && !memcmp(after, "interrupted", 11));
    read_database(shared, after, &after_bytes);
    assert(after_bytes == bytes && !memcmp(after, payload, bytes));
    assert(!remove(next_payload));
    assert(cheats_backend_commit(root, game, error));
    /* AR commands come in word pairs; include the terminator in the hook limit. */
    static CheatState bounded;
    bounded.record = malloc(CHEAT_PAYLOAD_CAP);
    assert(bounded.record);
    memset(bounded.record, 0x5a, CHEAT_PAYLOAD_CAP);
    bounded.record_bytes = CHEAT_PAYLOAD_CAP;
    bounded.count = 1;
    bounded.entries[0].selected = true;
    bounded.entries[0].words = (CHEAT_PAYLOAD_CAP - 8) / 4;
    assert(cheats_payload(&bounded, payload, &bytes) && bytes == CHEAT_PAYLOAD_CAP - 4);
    assert(!memcmp(payload + bytes - 4, "\0\0\0\xCF", 4));
    bounded.entries[0].words += 2;
    assert(!cheats_payload(&bounded, payload, &bytes));
    cheats_free(&bounded);
    cheats_free(&state);
    cheats_free(&restored);
    /* Exact header CRC matching: another revision must not inherit choices. */
    unsigned char header[512] = {0};
    memcpy(header + 12, "TST0", 4);
    header[0] = 1;
    write_file(game, header, sizeof header);
    assert(!cheats_load(&state, root, game));
    cheats_free(&state);
    header[0] = 0;
    write_file(game, header, sizeof header);
    /* Bounded malformed input and mutation coverage for offsets/counts/strings. */
    for (unsigned i = 0; i < 96; i++) {
        memcpy(after, original, db_bytes);
        size_t offset = i < 16 ? 0x100 + i : 0x120 + (i * 13) % (db_bytes - 0x120);
        after[offset] ^= (unsigned char)(0x80 | (i * 7));
        write_file(db, after, db_bytes);
        cheats_load(&state, root, game);
        cheats_free(&state);
    }
    write_file(db, original, db_bytes);
    app_init(&app, root, true, true);
    app_category(&app, CATEGORY_GAMES);
    cheats_open(&app);
    assert(app.cheats && app.cheats->record);
    for (int i = 0; i < 30; i++) {
        cheats_move(&app, 1);
        cheats_activate(&app);
        render_app(&app, top, bottom, "12:00");
    }
    assert(cheats_close(&app) && !app.cheats);
    app_panel(&app, PANEL_ACTIONS);
    app_panel_move(&app, 9);
    int actions_scroll = app.panel_scroll;
    app_panel_activate(&app);
    assert(app.cheats && app.cheats->record);
    app_back(&app);
    assert(!app.cheats && app.panel == PANEL_ACTIONS);
    assert(app.panel_selection == 9 && app.panel_scroll == actions_scroll);
    app_back(&app);
    assert(app.panel == PANEL_NONE);
    puts("PASS: revision-matched bounded cheat parsing, exclusive groups, read-only database, "
         "persisted selection, payload ownership and malformed input");
}
