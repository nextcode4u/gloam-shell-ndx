#include "paths.h"
#include "system_routes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static void be32(unsigned char *p, uint32_t value)
{
    for (int i = 0; i < 4; i++)
        p[i] = value >> (24 - i * 8);
}
static void directory(const char *root, const char *relative)
{
    char p[PATH_CAP];
    assert(path_join(p, root, relative));
    assert(!mkdir(p, 0777));
}
static void title(const char *root, uint32_t category, const char *name, uint32_t id)
{
    char dir[128], p[PATH_CAP];
    snprintf(dir, sizeof dir, "title/%08lx/%s", (unsigned long)category, name);
    directory(root, dir);
    snprintf(dir, sizeof dir, "title/%08lx/%s/content", (unsigned long)category, name);
    directory(root, dir);
    unsigned char tmd[0x208] = {0};
    be32(tmd, 0x10001);
    be32(tmd + 0x18C, category);
    be32(tmd + 0x190, id);
    tmd[0x1DF] = 1;
    be32(tmd + 0x1E4, 7);
    be32(tmd + 0x1F0, 512);
    char relative[160];
    snprintf(relative, sizeof relative, "%s/title.tmd", dir);
    assert(path_join(p, root, relative));
    FILE *f = fopen(p, "wb");
    assert(f && fwrite(tmd, 1, sizeof tmd, f) == sizeof tmd && !fclose(f));
    snprintf(relative, sizeof relative, "%s/00000007.app", dir);
    assert(path_join(p, root, relative));
    unsigned char header[512] = {0};
    f = fopen(p, "wb");
    assert(f && fwrite(header, 1, sizeof header, f) == sizeof header && !fclose(f));
}
static void regional_settings_handoff(const char *root)
{
    const char *regions = "JEPUCK";
    for (const char *region = regions; *region; region++) {
        char name[16], nand[PATH_CAP], id[9];
        snprintf(name, sizeof name, "region-%c", *region);
        directory(root, name);
        assert(path_join(nand, root, name));
        directory(nand, "title");
        directory(nand, "title/00030015");
        directory(nand, "title/00030017");
        uint32_t settings_id = 0x484E4200 | (unsigned char)*region;
        uint32_t launcher_id = 0x484E4100 | (unsigned char)*region;
        snprintf(id, sizeof id, "%08lx", (unsigned long)settings_id);
        title(nand, 0x30015, id, settings_id);
        snprintf(id, sizeof id, "%08lx", (unsigned long)launcher_id);
        title(nand, 0x30017, id, launcher_id);
        SystemCatalog catalog;
        system_catalog_scan(&catalog, nand);
        assert(catalog.titles[0].available && catalog.titles[0].low == settings_id);
        uint32_t warmboot[8], expected[8];
        unsigned char autoload[0x400], expected_autoload[0x400];
        assert(system_title_handoff(warmboot, autoload, &catalog.titles[0], &catalog));
        assert(system_title_request(expected, &catalog.titles[0]));
        assert(!memcmp(warmboot, expected, sizeof warmboot));
        assert(system_title_autoload(expected_autoload, &catalog.launcher));
        assert(!memcmp(autoload, expected_autoload, sizeof autoload));
        assert(warmboot[2] == settings_id && warmboot[4] == settings_id);
        /* Missing or unrelated Launcher metadata must not trigger direct Settings boot. */
        catalog.launcher.available = false;
        assert(!system_title_handoff(warmboot, autoload, &catalog.titles[0], &catalog));
        for (unsigned i = 0; i < 8; i++)
            assert(!warmboot[i]);
        for (unsigned i = 0; i < sizeof autoload; i++)
            assert(!autoload[i]);
        catalog.launcher.available = true;
        catalog.launcher.low = settings_id;
        assert(!system_title_handoff(warmboot, autoload, &catalog.titles[0], &catalog));
        NativeTitle sound = {0x30005, 0x484E4B00 | (unsigned char)*region, true, 9};
        assert(system_title_handoff(warmboot, autoload, &sound, NULL));
        assert(system_title_autoload(expected_autoload, &sound));
        assert(!memcmp(autoload, expected_autoload, sizeof autoload));
    }
}

static void put_le32(unsigned char *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++)
        p[i] = value >> (8 * i);
}
static void launcher_without_tmd(const char *root)
{
    char nand[PATH_CAP], path[PATH_CAP], second[PATH_CAP];
    directory(root, "patched-launcher");
    assert(path_join(nand, root, "patched-launcher"));
    directory(nand, "title");
    directory(nand, "title/00030017");
    directory(nand, "title/00030017/484e414a");
    directory(nand, "title/00030017/484e414a/content");
    assert(path_join(path, nand, "title/00030017/484e414a/content/00000002.app"));
    unsigned char app[0x1100] = {0};
    app[0x12] = 3;
    put_le32(app + 0x230, 0x484E414A);
    put_le32(app + 0x234, 0x30017);
    const unsigned offsets[] = {0x20, 0x30, 0x1C0, 0x1D0};
    for (unsigned i = 0; i < 4; i++) {
        put_le32(app + offsets[i], 0x1000 + i * 4);
        put_le32(app + offsets[i] + 12, 4);
    }
    FILE *f = fopen(path, "wb");
    assert(f && fwrite(app, 1, sizeof app, f) == sizeof app && !fclose(f));
    SystemCatalog catalog;
    system_catalog_scan(&catalog, nand);
    assert(catalog.launcher.available && catalog.launcher.low == 0x484E414A &&
           catalog.launcher.boot_content == 2);
    /* An unusable TMD must not hide the validated executable. */
    assert(path_join(second, nand, "title/00030017/484e414a/content/title.tmd"));
    f = fopen(second, "wb");
    assert(f && fputs("modified metadata", f) >= 0 && !fclose(f));
    system_catalog_scan(&catalog, nand);
    assert(catalog.launcher.available);
    /* Prefer the patched root menu even when the original installed menu is
     * present, and never fall back to stock if the patched file is malformed. */
    char patched[PATH_CAP];
    assert(path_join(patched, nand, "Launcher.dsi"));
    f = fopen(patched, "wb");
    assert(f && fwrite(app, 1, sizeof app, f) == sizeof app && !fclose(f));
    system_catalog_scan(&catalog, nand);
    assert(catalog.patched_launcher_present && catalog.patched_launcher_valid);
    unsigned char autoload[0x400];
    assert(system_launcher_autoload(autoload, &catalog));
    const char *route = "nand:/launcher.dsi";
    for (size_t i = 0; i <= strlen(route); i++)
        assert(autoload[0x38 + i * 2] == route[i] && !autoload[0x39 + i * 2]);
    NativeTitle settings = {0x30015, 0x484E424A, true, 1};
    uint32_t warmboot[8], expected[8];
    memset(warmboot, 0xA5, sizeof warmboot);
    assert(system_menu_handoff(warmboot, autoload, &catalog));
    for (unsigned i = 0; i < 8; i++)
        assert(!warmboot[i]);
    for (size_t i = 0; i <= strlen(route); i++)
        assert(autoload[0x38 + i * 2] == route[i] && !autoload[0x39 + i * 2]);
    catalog.launcher.available = false;
    assert(system_title_handoff(warmboot, autoload, &settings, &catalog));
    assert(system_title_request(expected, &settings));
    assert(!memcmp(warmboot, expected, sizeof expected));
    for (size_t i = 0; i <= strlen(route); i++)
        assert(autoload[0x38 + i * 2] == route[i] && !autoload[0x39 + i * 2]);
    f = fopen(patched, "wb");
    assert(f && fputs("invalid", f) >= 0 && !fclose(f));
    system_catalog_scan(&catalog, nand);
    assert(catalog.launcher.available && catalog.patched_launcher_present &&
           !catalog.patched_launcher_valid);
    assert(!system_launcher_autoload(autoload, &catalog));
    assert(!system_menu_handoff(warmboot, autoload, &catalog));
    assert(!system_title_handoff(warmboot, autoload, &settings, &catalog));
    for (unsigned i = 0; i < sizeof autoload; i++)
        assert(!autoload[i]);
    for (unsigned i = 0; i < 8; i++)
        assert(!warmboot[i]);
    NativeTitle sound = {0x30005, 0x484E4B4A, true, 1};
    assert(system_title_handoff(warmboot, autoload, &sound, &catalog));
    assert(!remove(patched));
    system_catalog_scan(&catalog, nand);
    assert(!catalog.patched_launcher_present && system_launcher_autoload(autoload, &catalog));
    assert(system_menu_handoff(warmboot, autoload, &catalog));
    system_menu_request(expected);
    assert(!memcmp(warmboot, expected, sizeof expected));
    /* Neither content order nor a higher version chooses between two matches. */
    assert(path_join(second, nand, "title/00030017/484e414a/content/00000003.app"));
    f = fopen(second, "wb");
    assert(f && fwrite(app, 1, sizeof app, f) == sizeof app && !fclose(f));
    system_catalog_scan(&catalog, nand);
    assert(!catalog.launcher.available);
    assert(!remove(second));
    /* Wrong embedded region and out-of-file executable sections are rejected. */
    app[0x230] = 'E';
    f = fopen(path, "wb");
    assert(f && fwrite(app, 1, sizeof app, f) == sizeof app && !fclose(f));
    system_catalog_scan(&catalog, nand);
    assert(!catalog.launcher.available);
    app[0x230] = 'J';
    put_le32(app + 0x1CC, 0xFFFFFFFF);
    f = fopen(path, "wb");
    assert(f && fwrite(app, 1, sizeof app, f) == sizeof app && !fclose(f));
    system_catalog_scan(&catalog, nand);
    assert(!catalog.launcher.available);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    regional_settings_handoff(root);
    launcher_without_tmd(root);
    directory(root, "title");
    directory(root, "title/00030015");
    title(root, 0x30015, "484e4245", 0x484E4245);
    directory(root, "title/00030017");
    title(root, 0x30017, "484e4145", 0x484E4145);
    SystemCatalog catalog;
    system_catalog_scan(&catalog, root);
    assert(catalog.attempted && catalog.ready && catalog.titles[0].available &&
           !catalog.titles[1].available);
    assert(catalog.titles[0].boot_content == 7);
    char expected_content[PATH_CAP];
    assert(path_join(expected_content, root, "title/00030015/484e4245/content/00000007.app"));
    assert(!strcmp(catalog.content_paths[0], expected_content));
    assert(!*catalog.content_paths[1]);
    uint32_t request[8];
    assert(system_title_request(request, &catalog.titles[0]));
    assert(request[0] == 0x434E4C54 && (request[1] & 65535) == 0x1801 && (request[1] >> 16));
    assert(request[2] == 0x484E4245 && request[3] == 0x30015 && request[4] == request[2] &&
           request[5] == request[3] && request[6] == 0x17);
    assert(!system_title_request(request, &catalog.titles[1]));
    unsigned char autoload[0x400];
    assert(system_title_autoload(autoload, &catalog.titles[0]));
    assert(!memcmp(autoload, "AutoLoadInfo", 12));
    assert(autoload[12] == 0xF0 && autoload[13] == 3 && autoload[16] == 3);
    assert(autoload[14] == 0xE0 && autoload[15] == 0xCD);
    const char *boot_path = "nand:/title/00030015/484e4245/content/00000007.app";
    for (size_t i = 0; i <= strlen(boot_path); i++) {
        assert(autoload[0x38 + i * 2] == (unsigned char)boot_path[i]);
        assert(autoload[0x39 + i * 2] == 0);
    }
    assert(!system_title_autoload(autoload, &catalog.titles[1]));
    for (size_t i = 0; i < sizeof autoload; i++)
        assert(autoload[i] == 0);
    assert(catalog.launcher.available && catalog.launcher.high == 0x30017 &&
           catalog.launcher.low == 0x484E4145 && catalog.launcher.boot_content == 7);
    assert(system_title_autoload(autoload, &catalog.launcher));
    const char *menu_path = "nand:/title/00030017/484e4145/content/00000007.app";
    for (size_t i = 0; i <= strlen(menu_path); i++)
        assert(autoload[0x38 + i * 2] == menu_path[i] && !autoload[0x39 + i * 2]);
    title(root, 0x30017, "484e414a", 0x484E414A);
    system_catalog_scan(&catalog, root);
    assert(!catalog.launcher.available && !system_title_autoload(autoload, &catalog.launcher));
    /* Another region must be treated as ambiguous, not selected by directory order. */
    title(root, 0x30015, "484e424a", 0x484E424A);
    system_catalog_scan(&catalog, root);
    assert(!catalog.titles[0].available && !*catalog.content_paths[0]);
    char p[PATH_CAP];
    assert(path_join(p, root, "title/00030015/484e424a/content/00000007.app"));
    FILE *f = fopen(p, "wb");
    assert(f && !fclose(f));
    system_catalog_scan(&catalog, root);
    assert(catalog.titles[0].available);
    /* A valid content file cannot substitute for a missing boot index. */
    assert(path_join(p, root, "title/00030015/484e4245/content/title.tmd"));
    f = fopen(p, "r+b");
    assert(f && !fseek(f, 0x1E1, SEEK_SET) && fputc(9, f) != EOF && !fclose(f));
    system_catalog_scan(&catalog, root);
    assert(!catalog.titles[0].available && !*catalog.content_paths[0]);
    /* The boot index is a content-table field, not an array position or ID. */
    f = fopen(p, "r+b");
    assert(f && !fseek(f, 0x1E9, SEEK_SET) && fputc(9, f) != EOF && !fclose(f));
    system_catalog_scan(&catalog, root);
    assert(catalog.titles[0].available && catalog.titles[0].boot_content == 7);
    /* Duplicate boot indices are ambiguous even if both files are present. */
    unsigned char duplicate[36];
    f = fopen(p, "r+b");
    assert(f && !fseek(f, 0x1E4, SEEK_SET) &&
           fread(duplicate, 1, sizeof duplicate, f) == sizeof duplicate);
    assert(!fseek(f, 0, SEEK_END) && fwrite(duplicate, 1, sizeof duplicate, f) == sizeof duplicate);
    assert(!fseek(f, 0x1DF, SEEK_SET) && fputc(2, f) != EOF && !fclose(f));
    system_catalog_scan(&catalog, root);
    assert(!catalog.titles[0].available && !*catalog.content_paths[0]);
    system_card_request(request);
    assert(request[2] == 0x43415254 && request[4] == 0x43415254 && request[6] == 0x13 &&
           (request[1] >> 16));
    system_card_autoload(autoload);
    assert(!memcmp(autoload, "AutoLoadInfo", 12));
    const char *card_path = "cart:";
    for (size_t i = 0; i <= strlen(card_path); i++) {
        assert(autoload[0x38 + 2 * i] == card_path[i]);
        assert(autoload[0x39 + 2 * i] == 0);
    }
    assert(autoload[0x10] == 3 && autoload[0x0C] == 0xF0 && autoload[0x0D] == 3);
    system_catalog_scan(&catalog, "/not/a/nand/root");
    assert(catalog.attempted && !catalog.ready && !catalog.titles[0].available);
    puts("PASS: bounded read-only title discovery, installed TMD/content identity, ambiguous "
         "region rejection and title/Slot-1 warmboot requests");
}
