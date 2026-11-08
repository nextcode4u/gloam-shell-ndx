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
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
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
