/* Entire NAND fixture is synthetic metadata; never a playable title or real save. */
#include "app.h"
#include "browser.h"
#include "icons.h"
#include "installed_titles.h"
#include "paths.h"
#include "preferences.h"
#include "system_routes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app, restored;
static void be32(unsigned char *p, uint32_t n)
{
    for (int i = 0; i < 4; i++)
        p[i] = n >> (24 - i * 8);
}
static void le32(unsigned char *p, uint32_t n)
{
    for (int i = 0; i < 4; i++)
        p[i] = n >> (i * 8);
}
static void directory(const char *root, const char *relative)
{
    char p[PATH_CAP];
    assert(path_join(p, root, relative));
    assert(!mkdir(p, 0777));
}
static void make_title(const char *root, uint32_t id, const char *name, bool unsigned_tmd)
{
    char dir[PATH_CAP], path[PATH_CAP], relative[120];
    snprintf(relative, sizeof relative, "title/00030004/%08lx", (unsigned long)id);
    directory(root, relative);
    assert(path_join(dir, root, relative));
    directory(dir, "content");
    directory(dir, "data");
    assert(path_join(path, dir, "data/public.sav"));
    FILE *file = fopen(path, "wb");
    assert(file && fputs("Keep my save", file) >= 0 && !fclose(file));
    unsigned char rom[0xC40] = {0}, tmd[0x208] = {0};
    rom[0x12] = 3;
    le32(rom + 0x230, id);
    le32(rom + 0x234, 0x00030004);
    le32(rom + 0x20, 0x240);
    le32(rom + 0x2C, 4);
    le32(rom + 0x30, 0x244);
    le32(rom + 0x3C, 4);
    le32(rom + 0x68, 0x400);
    rom[0x400] = 1;
    memset(rom + 0x420, 0x11, 512);
    rom[0x622] = 31;
    for (size_t i = 0; name[i]; i++)
        rom[0x400 + 0x340 + 2 * i] = name[i];
    be32(tmd, unsigned_tmd ? 0 : 0x10001);
    be32(tmd + 0x18C, 0x30004);
    be32(tmd + 0x190, id);
    tmd[0x1DF] = 1;
    tmd[0x1E1] = 5;
    be32(tmd + 0x1E4, 7);
    tmd[0x1E9] = 5;
    be32(tmd + 0x1F0, sizeof rom);
    assert(path_join(path, dir, "content/title.tmd"));
    file = fopen(path, "wb");
    assert(file && fwrite(tmd, 1, sizeof tmd, file) == sizeof tmd && !fclose(file));
    assert(path_join(path, dir, "content/00000007.app"));
    file = fopen(path, "wb");
    assert(file && fwrite(rom, 1, sizeof rom, file) == sizeof rom && !fclose(file));
}
static void scan(InstalledCatalog *catalog, const char *root)
{
    installed_catalog_begin(catalog, root, 1);
    assert(catalog->scanning && !catalog->count && !catalog->ready);
    unsigned steps = 0;
    while (catalog->scanning) {
        installed_catalog_step(catalog);
        assert(++steps < 4098);
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    directory(root, "title");
    directory(root, "title/00030004");
    make_title(root, 0x4B414245, "Zulu", false);
    make_title(root, 0x474C4458, "Alpha", true);
    make_title(root, 0x4B424245, "Broken", false);
    char path[PATH_CAP];
    assert(path_join(path, root, "title/00030004/4b424245/content/title.tmd"));
    FILE *file = fopen(path, "r+b");
    assert(file && !fseek(file, 0x190, SEEK_SET) && fputc(1, file) != EOF && !fclose(file));
    app_init(&app, root, true, true);
    scan(&app.installed_catalog, root);
    assert(app.installed_catalog.ready && app.installed_catalog.count == 2 &&
           app.installed_catalog.skipped == 1);
    assert(!strcmp(app.installed_catalog.titles[0].name, "Alpha"));
    assert(app.installed_catalog.titles[0].title.boot_content == 7);
    assert(installed_title_revalidate(&app.installed_catalog, 0));
    uint32_t request[8];
    unsigned char autoload[0x400];
    assert(system_title_request(request, &app.installed_catalog.titles[0].title));
    assert(request[2] == 0x474C4458 && request[3] == 0x30004 && request[6] == 0x17);
    assert(system_title_autoload(autoload, &app.installed_catalog.titles[0].title));
    const char *target = "nand:/title/00030004/474c4458/content/00000007.app";
    for (size_t i = 0; i <= strlen(target); i++)
        assert(autoload[0x38 + i * 2] == target[i] && !autoload[0x39 + i * 2]);
    app_category(&app, CATEGORY_SYSTEM);
    assert(app.entry_count == 8 && app.entries[7].kind == ITEM_INSTALLED_LIBRARY);
    app_move(&app, 7);
    app_open(&app);
    assert(app.installed_open && app.entry_count == 2 &&
           app.entries[0].kind == ITEM_INSTALLED_TITLE);
    app.browser_style = BROWSER_LIST;
    for (int i = 0; i < 8; i++)
        app_tick(&app);
    assert(icons_for_entry(&app, 0) && icons_for_entry(&app, 0)->pixels[0] == 0x801F);
    app_panel(&app, PANEL_ACTIONS);
    assert(app.action_enabled[0] && app.action_enabled[2]);
    assert(!app.action_enabled[1] && !app.action_enabled[3] && !app.action_enabled[8]);
    app.panel = PANEL_NONE;
    app_move(&app, 1);
    app_open(&app);
    assert(app.system_title_requested == 6 && app.installed_selected_id == 0x4B414245);
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(restored.installed_open && restored.installed_selected_id == 0x4B414245);
    app_back(&app);
    assert(!app.installed_open && app.entries[7].kind == ITEM_INSTALLED_LIBRARY);
    installed_catalog_begin(&app.installed_catalog, root, 1);
    installed_catalog_cancel(&app.installed_catalog);
    assert(!app.installed_catalog.directory && !app.installed_catalog.scanning);
    scan(&app.installed_catalog, root);
    assert(installed_title_path(&app.installed_catalog, 0, path));
    file = fopen(path, "r+b");
    assert(file && !fseek(file, 0x230, SEEK_SET) && fputc(0, file) != EOF && !fclose(file));
    assert(!installed_title_revalidate(&app.installed_catalog, 0));
    assert(path_join(path, root, "title/00030004/474c4458/data/public.sav"));
    file = fopen(path, "rb");
    char save[32] = {0};
    assert(file && fread(save, 1, 31, file) == 12 && !fclose(file));
    assert(!strcmp(save, "Keep my save"));
    puts("PASS: bounded installed-title discovery, names/metadata identity, native requests/icons, "
         "read-only actions, persistence and save preservation");
}
