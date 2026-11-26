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
static bool measure_scan;
static unsigned scan_seeks, end_seeks, backward_seeks;
static size_t scan_bytes;
int __real_fseek(FILE *, long, int);
int __wrap_fseek(FILE *file, long offset, int whence)
{
    if (measure_scan) {
        scan_seeks++;
        end_seeks += whence == SEEK_END;
        backward_seeks += whence == SEEK_SET && offset < ftell(file);
    }
    return __real_fseek(file, offset, whence);
}
size_t __real_fread(void *, size_t, size_t, FILE *);
size_t __wrap_fread(void *data, size_t size, size_t count, FILE *file)
{
    size_t result = __real_fread(data, size, count, file);
    if (measure_scan)
        scan_bytes += result * size;
    return result;
}
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
size_t __wrap___fread_chk(void *data, size_t capacity, size_t size, size_t count, FILE *file)
{
    size_t result = __real___fread_chk(data, capacity, size, count, file);
    if (measure_scan)
        scan_bytes += result * size;
    return result;
}
#endif
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
static void forward_scan_tests(void)
{
    /* A sparse 32 MiB container keeps trailing content outside the ARM9 scan.
     * Include a partial last chunk and drivers crossing two chunk boundaries. */
    const unsigned length = 0x300080;
    const unsigned positions[] = {0x3FC, 0x7FC, length - 256};
    for (unsigned variant = 0; variant <= sizeof positions / sizeof *positions; variant++) {
        fixture();
        unsigned char driver[128];
        memcpy(driver, binary + dldi, sizeof driver);
        memset(binary + dldi, 0, sizeof driver);
        word(0x2C, length);
        word(0x30, 0x200 + length);
        write_binary();
        FILE *file = fopen(path, "r+b");
        assert(file && !fseek(file, 32 * 1024 * 1024 - 1, SEEK_SET));
        assert(fputc(0, file) == 0);
        bool has_driver = variant < sizeof positions / sizeof *positions;
        if (has_driver) {
            uint32_t address = 0x02000000 + positions[variant];
            memcpy(binary + dldi, driver, sizeof driver);
            word(dldi + 0x40, address);
            word(dldi + 0x44, address + 0x88);
            for (unsigned i = 0x68; i < 0x80; i += 4)
                word(dldi + i, address + 0x80);
            assert(!fseek(file, 0x200 + positions[variant], SEEK_SET));
            assert(fwrite(binary + dldi, 1, sizeof driver, file) == sizeof driver);
        }
        assert(!fclose(file));
        scan_seeks = end_seeks = backward_seeks = 0;
        scan_bytes = 0;
        measure_scan = true;
        bool found = launch_is_homebrew_file(path);
        measure_scan = false;
        assert(found == has_driver);
        assert(scan_seeks == 1 && !end_seeks && !backward_seeks);
        assert(scan_bytes <= 0x200 + length);
        if (!has_driver)
            assert(scan_bytes == 0x200 + length);
    }
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
    forward_scan_tests();
    fixture();
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
         "forward-only bounded ARM9 reads without EOF/backward seeks; "
         "version/allocation/pointer/binary "
         "bounds and known retail routing");
}
