#include "app.h"
#include "browser.h"
#include "cheats.h"
#include "paths.h"
#include "renderer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    /* A payload changed by another owner is preserved and blocks this adapter. */
    write_file(shared, "foreign", 7);
    assert(!cheats_backend_available(root, error));
    assert(!cheats_backend_commit(root, game, error));
    read_database(shared, after, &after_bytes);
    assert(after_bytes == 7 && !memcmp(after, "foreign", 7));
    write_file(shared, payload, bytes);
    assert(cheats_backend_available(root, error));
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
