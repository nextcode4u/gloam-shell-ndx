#include "app.h"
#include "arguments.h"
#include "launch.h"
#include "paths.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app;
static LaunchPlan plan;
static unsigned char binary[0x2400];
static char path[PATH_CAP];
static const unsigned dldi = 0x200 + 0x3FC; /* Cross a scan-chunk boundary. */
static void word(unsigned offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++)
        binary[offset + i] = value >> (8 * i);
}
static void write_binary(void)
{
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(binary, 1, sizeof binary, file) == sizeof binary);
    assert(!fclose(file));
}
static void fixture(void)
{
    memset(binary, 0, sizeof binary);
    memcpy(binary + 12, "HOME", 4);
    word(0x20, 0x200);
    word(0x24, 0x02000000);
    word(0x28, 0x02000000);
    word(0x2C, 0x2000);
    word(0x30, 0x2200);
    word(0x34, 0x037F8000);
    word(0x38, 0x037F8000);
    word(0x3C, 0x100);
    const unsigned char signature[] = {0xED, 0xA5, 0x8D, 0xBF, ' ', 'C',
                                       'h',  'i',  's',  'h',  'm', 0};
    memcpy(binary + dldi, signature, sizeof signature);
    binary[dldi + 12] = 1;
    binary[dldi + 13] = binary[dldi + 15] = 8;
    word(dldi + 0x40, 0x020003FC);
    word(dldi + 0x44, 0x02000484);
    for (unsigned i = 0x68; i < 0x80; i += 4)
        word(dldi + i, 0x0200047C);
}
static void directory(const char *root, const char *relative)
{
    char name[PATH_CAP];
    assert(path_join(name, root, relative));
    assert(!mkdir(name, 0777) || errno == EEXIST);
}
static void slot1_tests(const char *root)
{
    char error[160], fallback[PATH_CAP];
    assert(!launch_prepare_slot1(&app, &plan, error));
    assert(strstr(error, "slot1launch.nds"));
    directory(root, "_nds");
    directory(root, "_nds/akmenunext");
    assert(path_join(path, root, "_nds/akmenunext/slot1launch.nds"));
    path_copy(fallback, path, sizeof fallback);
    fixture();
    write_binary();
    assert(launch_prepare_slot1(&app, &plan, error));
    assert(plan.route == LAUNCH_DIRECT && !strcmp(plan.content, "system:slot1"));
    assert(!strcmp(plan.application, fallback) && plan.arguments.count == 1);
    assert(!strcmp(arguments_get(&plan.arguments, 0), fallback));
    app.dsi_mode = false;
    assert(!launch_prepare_slot1(&app, &plan, error));
    app.dsi_mode = true;
    app.storage_ready = false;
    assert(!launch_prepare_slot1(&app, &plan, error));
    app.storage_ready = true;
    directory(root, "_nds/dsi-frontend");
    directory(root, "_nds/dsi-frontend/loaders");
    assert(path_join(path, root, "_nds/dsi-frontend/loaders/slot1launch.nds"));
    write_binary();
    assert(launch_prepare_slot1(&app, &plan, error));
    assert(!strcmp(plan.application, path)); /* Dedicated helper has precedence. */
    memcpy(binary + 12, "ASME", 4);
    write_binary();
    assert(!launch_prepare_slot1(&app, &plan, error)); /* Never route a renamed retail ROM. */
    assert(plan.route == LAUNCH_DIRECT);
    fixture();
    word(0x24, 0x04000000);
    write_binary();
    assert(!launch_prepare_slot1(&app, &plan, error)); /* Invalid helper cannot bypass bounds. */
    assert(*error);
    fixture();
    write_binary();
    app_category(&app, CATEGORY_SYSTEM);
    bool found = false;
    for (int i = 0; i < app.entry_count; ++i) {
        if (app.entries[i].kind == ITEM_SYSTEM && app.entries[i].target == 6) {
            app.locations[app.category].selected = i;
            app_open(&app);
            found = true;
            assert(app.slot1_requested && !app.system_menu_requested);
            assert(app.category == CATEGORY_SYSTEM);
        }
    }
    assert(found);
}

int main(int argc, char **argv)
{
    assert(argc == 2 && path_join(path, argv[1], "custom.nds"));
    fixture();
    write_binary();
    assert(launch_is_homebrew_file(path));
    app_init(&app, argv[1], true, true);
    char error[160];
    assert(launch_prepare(&app, path, &plan, error));
    assert(plan.route == LAUNCH_DIRECT && !strcmp(plan.application, path));
    binary[dldi + 12] = 2;
    write_binary();
    assert(!launch_is_homebrew_file(path));
    fixture();
    word(dldi + 0x40, 0x02001000);
    write_binary();
    assert(!launch_is_homebrew_file(path));
    fixture();
    binary[dldi + 15] = 7;
    write_binary();
    assert(!launch_is_homebrew_file(path));
    fixture();
    binary[dldi + 15] = 15; /* Reserved patch space must also fit ARM9. */
    write_binary();
    assert(!launch_is_homebrew_file(path));
    fixture();
    word(dldi + 0x70, 0x03000000);
    write_binary();
    assert(!launch_is_homebrew_file(path));
    fixture();
    memcpy(binary + 12, "ASME", 4); /* Known retail IDs keep the retail route. */
    write_binary();
    assert(!launch_is_homebrew_file(path));
    fixture();
    memcpy(binary + 0x2200, binary + dldi, 128);
    memset(binary + dldi, 0, 128);
    write_binary();
    assert(!launch_is_homebrew_file(path));
    fixture();
    word(0x2C, 0x400000);
    write_binary();
    assert(!launch_is_homebrew_file(path));
    fixture();
    memset(binary + dldi, 0, 128);
    memcpy(binary, "HOMEBREW", 8);
    memcpy(binary + 12, "CGIT", 4);
    write_binary();
    assert(launch_is_homebrew_file(path));
    assert(launch_prepare(&app, path, &plan, error) && plan.route == LAUNCH_DIRECT);
    binary[0x10] = '0'; /* The explicit marker includes an unset maker. */
    write_binary();
    assert(!launch_is_homebrew_file(path));
    binary[0x10] = 0;
    memcpy(binary + 12, "ASME", 4);
    write_binary();
    assert(!launch_is_homebrew_file(path));
    memcpy(binary + 12, "CGIT", 4);
    word(0x24, 0x04000000); /* Recognition does not bypass binary validation. */
    write_binary();
    assert(!launch_prepare(&app, path, &plan, error));
    slot1_tests(argv[1]);
    puts("PASS: Slot-1 helper discovery/precedence, system selection, missing/invalid helper "
         "rejection; custom-ID homebrew, linked DLDI chunk overlap, "
         "version/allocation/pointer/binary "
         "bounds and known retail routing");
}
