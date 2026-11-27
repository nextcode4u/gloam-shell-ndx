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
static const char *watched_binary;
static FILE *failed_stream;
static unsigned binary_opens, failed_open;
static bool fail_metadata, fail_read, fail_stream, fail_close;
static unsigned reads_before_failure;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_fstat(int, struct stat *);
int __real_ferror(FILE *);
FILE *__wrap_fopen(const char *name, const char *mode)
{
    FILE *file = __real_fopen(name, mode);
    if (watched_binary && !strcmp(name, watched_binary) && *mode == 'r' &&
        ++binary_opens == failed_open)
        failed_stream = file;
    return file;
}
int __wrap_fclose(FILE *file)
{
    bool deny = file == failed_stream && fail_close;
    if (file == failed_stream)
        failed_stream = NULL;
    int result = __real_fclose(file);
    if (deny) {
        errno = EIO;
        return EOF;
    }
    return result;
}
int __wrap_fstat(int descriptor, struct stat *info)
{
    if (failed_stream && fileno(failed_stream) == descriptor && fail_metadata) {
        errno = EIO;
        return -1;
    }
    return __real_fstat(descriptor, info);
}
int __wrap_ferror(FILE *file)
{
    return (file == failed_stream && fail_stream) || __real_ferror(file);
}
static bool reject_read(FILE *file)
{
    if (file != failed_stream || !fail_read)
        return false;
    if (!reads_before_failure)
        return true;
    reads_before_failure--;
    return false;
}
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
    if (reject_read(file))
        return 0;
    size_t result = __real_fread(data, size, count, file);
    if (measure_scan)
        scan_bytes += result * size;
    return result;
}
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
size_t __wrap___fread_chk(void *data, size_t capacity, size_t size, size_t count, FILE *file)
{
    if (reject_read(file))
        return 0;
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
static void preflight_read_failures(void)
{
    char error[160];
    for (unsigned kind = 0; kind < 3; kind++) {
        fixture();
        if (kind == 1)
            memcpy(binary + 12, "####", 4);
        if (kind == 2) {
            memcpy(binary, "HOMEBREW", 8);
            memset(binary + 8, 0, 4);
            memset(binary + dldi, 0, 128);
        }
        write_binary();
        for (unsigned stage = 1; stage <= (kind == 1 ? 2u : 3u); stage++) {
            for (unsigned failure = 0; failure < 4; failure++) {
                watched_binary = path;
                failed_open = stage;
                binary_opens = 0;
                fail_metadata = failure == 0;
                fail_read = failure == 1;
                fail_stream = failure == 2;
                fail_close = failure == 3;
                assert(!launch_prepare(&app, path, &plan, error));
                assert(*error && binary_opens == stage);
                if (stage == 1) {
                    unsigned char header[0x200];
                    uint32_t bytes = 0xDEADBEEF;
                    binary_opens = 0;
                    assert(!launch_read_header(path, header, sizeof header, &bytes));
                    assert(bytes == 0xDEADBEEF);
                    bool homebrew = true;
                    binary_opens = 0;
                    assert(!launch_probe_homebrew(path, &homebrew) && !homebrew);
                }
                watched_binary = NULL;
                fail_metadata = fail_read = fail_stream = fail_close = false;
                assert(launch_prepare(&app, path, &plan, error) && plan.route == LAUNCH_DIRECT);
            }
        }
    }
    /* A failed DLDI body read must stop classification before any retail fallback. */
    fixture();
    write_binary();
    watched_binary = path;
    binary_opens = 0;
    failed_open = 2;
    fail_read = true;
    reads_before_failure = 1;
    assert(!launch_prepare(&app, path, &plan, error) && binary_opens == 2);
    watched_binary = NULL;
    fail_read = false;
    reads_before_failure = 0;
    assert(launch_prepare(&app, path, &plan, error) && plan.route == LAUNCH_DIRECT);
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
    preflight_read_failures();
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
    bool homebrew = true;
    assert(launch_probe_homebrew(path, &homebrew) && !homebrew);
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
