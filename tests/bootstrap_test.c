#include "app.h"
#include "arguments.h"
#include "bootstrap.h"
#include "browser.h"
#include "game_options.h"
#include "launch.h"
#include "paths.h"
#include "renderer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static App app;
static LaunchPlan plan;
static uint16_t top[256 * 192], bottom[256 * 192];
static void put32(unsigned char *p, uint32_t n)
{
    for (int i = 0; i < 4; i++)
        p[i] = n >> (i * 8);
}
static uint16_t crc16(const unsigned char *p, int length)
{
    uint16_t crc = 0xFFFF;
    while (length--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    return crc;
}
static void write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    assert(f && fwrite(data, 1, size, f) == size && !fclose(f));
}
static void read_text(const char *path, char output[4096])
{
    FILE *f = fopen(path, "rb");
    assert(f);
    size_t count = fread(output, 1, 4095, f);
    output[count] = 0;
    assert(!ferror(f));
    fclose(f);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    char path[PATH_CAP], game[PATH_CAP], backend[PATH_CAP], error[160], ini[PATH_CAP], text[4096];
    const char *dirs[] = {"roms", "_nds", "_nds/dsi-frontend"};
    for (unsigned i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        assert(path_join(path, root, dirs[i]));
        assert(!mkdir(path, 0777));
    }
    unsigned char data[1024] = {0};
    memcpy(data + 12, "ASME", 4);
    put32(data + 0x20, 512);
    put32(data + 0x24, 0x02000000);
    put32(data + 0x28, 0x02000000);
    put32(data + 0x2C, 64);
    put32(data + 0x30, 576);
    put32(data + 0x34, 0x037F8000);
    put32(data + 0x38, 0x037F8000);
    put32(data + 0x3C, 64);
    uint16_t crc = crc16(data, 0x15E);
    data[0x15E] = crc;
    data[0x15F] = crc >> 8;
    assert(path_join(game, root, "roms/Game with spaces.nds"));
    write_file(game, data, sizeof data);
    assert(path_join(backend, root, "_nds/nds-bootstrap-release.nds"));
    memcpy(data + 12, "KBSE", 4);
    write_file(backend, data, sizeof data);
    app_init(&app, root, true, true);
    bool known;
    assert(game_save_size(0x454D5341, &known) == 8192 && known);
    assert(!game_save_size(0x30303030, &known) && !known);
    assert(launch_prepare(&app, game, &plan, error));
    assert(plan.route == LAUNCH_BOOTSTRAP && plan.save_bytes == 8192 && plan.arguments.count == 1);
    assert(strstr(plan.save, "/saves/Game with spaces.sav"));
    struct stat status;
    assert(stat(plan.save, &status)); /* Preflight never creates a save. */
    assert(path_join(ini, root, "_nds/nds-bootstrap.ini"));
    const char *old = "; keep "
                      "comment\n[NDS-BOOTSTRAP]\nNDS_PATH=old\nSAV_PATH=old.sav\nAPP_PATH=stale."
                      "app\nDONOR5_NDS_PATH=sd:/donor.nds\nDEBUG=1\n[OTHER]\nNDS_PATH=other\n";
    write_file(ini, old, strlen(old));
    assert(bootstrap_commit(root, &plan, error));
    assert(!stat(plan.save, &status) && status.st_size == 8192);
    FILE *save = fopen(plan.save, "rb");
    assert(save);
    for (int i = 0; i < 8192; i++)
        assert(fgetc(save) == 255);
    fclose(save);
    read_text(ini, text);
    assert(strstr(text, "; keep comment") && strstr(text, "DONOR5_NDS_PATH=sd:/donor.nds") &&
           strstr(text, "DEBUG=1"));
    assert(strstr(text, "[OTHER]\nNDS_PATH=other") &&
           strstr(text, "NDS_PATH=sd:/roms/Game with spaces.nds") &&
           strstr(text, "QUIT_PATH=sd:/BOOT.NDS") && !strstr(text, "stale.app"));
    /* Relaunching must not accumulate empty sections or blank lines. */
    char stable_ini[sizeof text];
    strcpy(stable_ini, text);
    for (int launch = 0; launch < 32; launch++) {
        assert(bootstrap_commit(root, &plan, error));
        read_text(ini, text);
        assert(!strcmp(text, stable_ini));
    }
    /* VRAM Auto must not serialize as -1: the backend casts that value to true.
     * Switching back to Auto must also remove a previously explicit boost. */
    assert(!strstr(text, "BOOST_VRAM="));
    for (int boost = 0; boost <= 1; boost++) {
        plan.options.values[GAME_VRAM] = boost;
        assert(bootstrap_commit(root, &plan, error));
        read_text(ini, text);
        assert(strstr(text, boost ? "BOOST_VRAM=1\n" : "BOOST_VRAM=0\n"));
    }
    plan.options.values[GAME_VRAM] = -1;
    assert(bootstrap_commit(root, &plan, error));
    read_text(ini, text);
    assert(!strstr(text, "BOOST_VRAM="));
    GameOptions defaults, item, effective;
    game_options_defaults(&defaults, false);
    defaults.values[GAME_CPU] = 1;
    defaults.values[GAME_LANGUAGE] = 2;
    assert(game_options_write(root, "", &defaults));
    game_options_defaults(&item, true);
    item.values[GAME_CPU] = 0;
    item.values[GAME_SLOT] = 2;
    assert(game_options_write(root, game, &item));
    game_options_effective(root, game, &effective);
    assert(effective.values[GAME_CPU] == 0 && effective.values[GAME_LANGUAGE] == 2 &&
           effective.values[GAME_SLOT] == 2);
    assert(launch_prepare(&app, game, &plan, error) && strstr(plan.save, ".sav2"));
    assert(bootstrap_commit(root, &plan, error));
    assert(!stat(plan.save, &status) && status.st_size == 8192);
    char slot2[PATH_CAP];
    path_copy(slot2, plan.save, PATH_CAP);
    write_file(slot2, "test", 4);
    assert(!launch_prepare(&app, game, &plan, error) && strstr(error, "size differs"));
    read_text(slot2, text);
    assert(!strcmp(text, "test"));
    item.values[GAME_SLOT] = 0;
    assert(game_options_write(root, game, &item));
    assert(path_join(path, root, "roms/Game with spaces.sav"));
    write_file(path, "conflict", 8);
    assert(!launch_prepare(&app, game, &plan, error) && strstr(error, "Multiple"));
    assert(!remove(path));
    item.values[GAME_SAVE_SIZE] = 0;
    assert(game_options_write(root, game, &item));
    assert(launch_prepare(&app, game, &plan, error) && !plan.save_bytes && !*plan.save);
    item.values[GAME_SAVE_SIZE] = GAME_INHERIT;
    item.values[GAME_LANGUAGE] = 999;
    assert(!game_options_write(root, game, &item));
    item.values[GAME_LANGUAGE] = GAME_INHERIT;
    assert(!game_options_write(root, "/outside/game.nds", &item));
    app_category(&app, CATEGORY_GAMES);
    for (int i = 0; i < app.entry_count; i++)
        if (!strcmp(app.entries[i].path, game))
            app.locations[1].selected = i;
    game_editor_open(&app, false);
    assert(app.game_editor.open);
    for (int i = 0; i < 40; i++) {
        game_editor_move(&app, 1);
        game_editor_adjust(&app, -1);
        render_app(&app, top, bottom, "12:00");
    }
    assert(game_editor_close(&app) && !app.game_editor.open);
    game_editor_open(&app, true);
    assert(app.game_editor.open && app.game_editor.defaults);
    assert(game_editor_close(&app));
    /* A malformed INI must not be replaced even if the ROM passes preflight. */
    game_options_defaults(&item, true);
    assert(game_options_write(root, game, &item));
    assert(launch_prepare(&app, game, &plan, error));
    write_file(ini, "[broken\n", 8);
    assert(!bootstrap_commit(root, &plan, error));
    read_text(ini, text);
    assert(!strcmp(text, "[broken\n"));
    const char binary_ini[] = "[NDS-BOOTSTRAP]\nDEBUG=1\0NDS_PATH=damaged\n";
    write_file(ini, binary_ini, sizeof binary_ini - 1);
    assert(!bootstrap_commit(root, &plan, error) && strstr(error, "binary data"));
    FILE *unchanged_ini = fopen(ini, "rb");
    assert(unchanged_ini &&
           fread(text, 1, sizeof binary_ini - 1, unchanged_ini) == sizeof binary_ini - 1);
    assert(!memcmp(text, binary_ini, sizeof binary_ini - 1));
    fclose(unchanged_ini);
    write_file(ini, old, strlen(old));
    char hb[PATH_CAP], hb_backend[PATH_CAP];
    assert(path_join(hb, root, "roms/HB target.nds"));
    memcpy(data + 12, "####", 4);
    write_file(hb, data, sizeof data);
    assert(path_join(hb_backend, root, "_nds/nds-bootstrap-hb-release.nds"));
    memcpy(data + 12, "KBSE", 4);
    write_file(hb_backend, data, sizeof data);
    memset(&plan, 0, sizeof plan);
    assert(bootstrap_prepare_homebrew(&app, hb, game, &plan, error));
    assert(plan.route == LAUNCH_BOOTSTRAP_HB && !strcmp(plan.homebrew_argument, game));
    assert(bootstrap_commit(root, &plan, error));
    read_text(ini, text);
    assert(strstr(text, "HOMEBREW_ARG=sd:/roms/Game with spaces.nds"));
    assert(strstr(text, "RAM_DRIVE_PATH=\nRAM_DRIVE_SIZE=0"));
    assert(!bootstrap_prepare_homebrew(&app, "/outside/app.nds", game, &plan, error));
    puts("PASS: bootstrap preflight/INI preservation, known save sizes, isolated slots, no-resize "
         "conflicts and game defaults/overrides/editor");
}
