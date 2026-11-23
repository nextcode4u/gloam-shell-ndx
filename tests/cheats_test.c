#include "app.h"
#include "browser.h"
#include "cheats.h"
#include "paths.h"
#include "renderer.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static const char *watched_paths[2];
static FILE *watched_streams[2];
static size_t read_limit = (size_t)-1;
static bool fail_presence, nonregular_presence, fail_close;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_stat(const char *, struct stat *);
size_t __real_fread(void *, size_t, size_t, FILE *);
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
#endif
FILE *__wrap_fopen(const char *path, const char *mode)
{
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
