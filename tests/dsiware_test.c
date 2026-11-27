#include "app.h"
#include "bootstrap.h"
#include "dsiware.h"
#include "paths.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static App app;
static const char *probe_save;
static unsigned fail_check, probe_checks;
static bool measure_save_writes;
static unsigned save_writes, fail_write;
size_t __real_fwrite(const void *, size_t, size_t, FILE *);
size_t __wrap_fwrite(const void *data, size_t size, size_t count, FILE *file)
{
    if (measure_save_writes && ++save_writes == fail_write && count)
        count--;
    return __real_fwrite(data, size, count, file);
}
int __real_stat(const char *, struct stat *);
int __wrap_stat(const char *path, struct stat *status)
{
    if (probe_save && !strcmp(path, probe_save) && ++probe_checks == fail_check) {
        errno = EIO;
        return -1;
    }
    return __real_stat(path, status);
}
static void put32(unsigned char *p, uint32_t value)
{
    for (int i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}
static uint16_t crc16(const unsigned char *p, unsigned bytes)
{
    uint16_t crc = 0xFFFF;
    while (bytes--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    return crc;
}
static unsigned get16(const unsigned char *p)
{
    return p[0] | ((unsigned)p[1] << 8);
}
static unsigned get32(const unsigned char *p)
{
    return get16(p) | (get16(p + 2) << 16);
}
static void verify_volume(const char *path, unsigned bytes)
{
    unsigned char boot[512], first[512], second[512];
    FILE *file = fopen(path, "rb");
    assert(file && fread(boot, 1, sizeof boot, file) == sizeof boot);
    unsigned sectors = get16(boot + 19);
    if (!sectors)
        sectors = get32(boot + 32);
    unsigned fat = get16(boot + 22), roots = (get16(boot + 17) * 32 + 511) / 512;
    unsigned data_start = get16(boot + 14) + boot[16] * fat + roots;
    assert(sectors * 512 == bytes && get16(boot + 11) == 512 && boot[16] == 2);
    assert(boot[13] && !(boot[13] & (boot[13] - 1)) && data_start < sectors);
    unsigned clusters = (sectors - data_start) / boot[13];
    assert(clusters && clusters < 4085 && ((clusters + 2) * 3 + 1) / 2 <= fat * 512);
    for (unsigned sector = 0; sector < fat; sector++) {
        assert(!fseek(file, (1 + sector) * 512, SEEK_SET));
        assert(fread(first, 1, 512, file) == 512);
        assert(!fseek(file, (1 + fat + sector) * 512, SEEK_SET));
        assert(fread(second, 1, 512, file) == 512 && !memcmp(first, second, 512));
        for (unsigned i = 0; i < 512; i++)
            assert(first[i] == (!sector && i < 3 ? (i ? 0xFF : 0xF8) : 0));
    }
    assert(!fseek(file, (1 + 2 * fat) * 512, SEEK_SET));
    for (unsigned i = 0; i < roots * 512; i++)
        assert(fgetc(file) == 0);
    /* No reserved FAT entries may leak into the blank data area across batches. */
    unsigned char blank[4096];
    size_t remaining = bytes - data_start * 512;
    while (remaining) {
        size_t count = remaining < sizeof blank ? remaining : sizeof blank;
        assert(fread(blank, 1, count, file) == count);
        for (size_t i = 0; i < count; i++)
            assert(!blank[i]);
        remaining -= count;
    }
    assert(!fclose(file));
    /* Data at both ends of a private volume survives a repeated prepare. */
    file = fopen(path, "r+b");
    assert(file && !fseek(file, data_start * 512, SEEK_SET) && fputc(0xA7, file) == 0xA7);
    assert(!fseek(file, bytes - 1, SEEK_SET) && fputc(0x42, file) == 0x42 && !fclose(file));
    char error[160] = "";
    save_writes = 0;
    measure_save_writes = true;
    assert(dsiware_create_save(path, bytes, error));
    measure_save_writes = false;
    assert(!save_writes);
    file = fopen(path, "rb");
    assert(file && !fseek(file, data_start * 512, SEEK_SET) && fgetc(file) == 0xA7);
    assert(!fseek(file, bytes - 1, SEEK_SET) && fgetc(file) == 0x42 && !fclose(file));
}
static void save_write_failures(const char *root)
{
    char path[PATH_CAP], temporary[PATH_CAP], error[160];
    assert(path_join(path, root, "failed-volume.prv"));
    assert(snprintf(temporary, sizeof temporary, "%s.frontend-new", path) < (int)sizeof temporary);
    save_writes = 0;
    measure_save_writes = true;
    assert(dsiware_create_save(path, 65536, error));
    measure_save_writes = false;
    unsigned complete_writes = save_writes;
    assert(complete_writes && !remove(path));
    for (unsigned failure = 1; failure <= complete_writes; failure++) {
        save_writes = 0;
        fail_write = failure;
        measure_save_writes = true;
        assert(!dsiware_create_save(path, 65536, error));
        measure_save_writes = false;
        assert(save_writes == failure && *error);
        struct stat metadata;
        assert(stat(path, &metadata) && errno == ENOENT);
        assert(stat(temporary, &metadata) && errno == ENOENT);
    }
    fail_write = 0;
    assert(dsiware_create_save(path, 65536, error));
    verify_volume(path, 65536);
}
static void save_preflight_failures(const char *root, const char *target,
                                    const unsigned char *header, LaunchPlan *plan)
{
    const char *extensions[] = {"pub", "prv"};
    const char *patterns[] = {"roms/WARE.%s", "roms/WARE.DSI.%s", "roms/saves/WARE.%s"};
    char path[PATH_CAP], name[80], error[160];
    for (unsigned extension = 0; extension < 2; extension++) {
        for (unsigned variant = 0; variant < 3; variant++) {
            snprintf(name, sizeof name, variant == 2 ? "roms/saves/WARE.%s" : "roms/%s.%s",
                     variant == 2 ? extensions[extension]
                     : variant    ? "WARE.DSI"
                                  : "WARE",
                     extensions[extension]);
            assert(path_join(path, root, name));
            for (unsigned failure = 1; failure <= (variant == 2 ? 2u : 1u); failure++) {
                probe_save = path;
                probe_checks = 0;
                fail_check = failure;
                error[0] = 0;
                assert(!bootstrap_prepare(&app, target, plan, error) && strstr(error, "inspect"));
                probe_save = NULL;
            }
        }
        snprintf(name, sizeof name, "roms/saves/WARE.%s2", extensions[extension]);
        assert(path_join(path, root, name));
        plan->options.values[GAME_SLOT] = 2;
        probe_save = path;
        probe_checks = 0;
        fail_check = 1;
        error[0] = 0;
        assert(!dsiware_prepare(&app, target, header, plan, error) && strstr(error, "inspect"));
        probe_save = NULL;
    }
    assert(bootstrap_prepare(&app, target, plan, error));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    char path[PATH_CAP], target[PATH_CAP], error[160] = "", alias[64];
    assert(path_prepare_preferences_directory(root));
    assert(path_join(path, root, "roms") && !mkdir(path, 0777));
    /* Original header/range fixture, not runnable game code. */
    unsigned char binary[0x1200] = {0};
    memcpy(binary + 12, "ZDSW", 4);
    binary[0x12] = 3;
    for (int cpu = 0; cpu < 2; cpu++) {
        put32(binary + 0x20 + cpu * 16, 0x1000 + cpu * 0x100);
        put32(binary + 0x2C + cpu * 16, 0x100);
    }
    uint16_t crc = crc16(binary, 0x15E);
    binary[0x15E] = crc;
    binary[0x15F] = crc >> 8;
    put32(binary + 0x210, sizeof binary);
    put32(binary + 0x238, 16384);
    put32(binary + 0x23C, 8192);
    assert(path_join(target, root, "roms/WARE.DSI"));
    FILE *file = fopen(target, "wb");
    assert(file && fwrite(binary, 1, sizeof binary, file) == sizeof binary && !fclose(file));
    assert(path_join(path, root, "_nds/nds-bootstrap-release.nds"));
    file = fopen(path, "w");
    assert(file && fputs("Backend presence fixture; not executable", file) >= 0 && !fclose(file));
    app_init(&app, root, true, true);
    LaunchPlan plan = {0};
    assert(bootstrap_prepare(&app, target, &plan, error));
    assert(plan.route == LAUNCH_DSIWARE && plan.save_bytes == 16384 && plan.private_bytes == 8192);
    save_preflight_failures(root, target, binary, &plan);
    assert(strstr(plan.save, "saves/WARE.pub") && strstr(plan.private_save, "saves/WARE.prv"));
    /* A previous launcher must not redirect this title's Quit Game target. */
    assert(path_join(path, root, "_nds/nds-bootstrap.ini"));
    file = fopen(path, "w");
    assert(file && fputs("[NDS-BOOTSTRAP]\nQUIT_PATH=sd:/old-loader.nds\n", file) >= 0 &&
           !fclose(file));
    assert(bootstrap_commit(root, &plan, error));
    assert(dsiware_alias(root, plan.save, alias) && !strcmp(alias, "sd:/ROMS/SAVES/WARE.PUB"));
    assert(!dsiware_alias(root, "/outside/WARE.pub", alias));
    unsigned char boot[512];
    file = fopen(plan.save, "rb");
    assert(file && fread(boot, 1, sizeof boot, file) == sizeof boot && !fclose(file));
    assert(boot[510] == 0x55 && boot[511] == 0xAA && !memcmp(boot + 54, "FAT12   ", 8));
    assert(boot[16] == 2 && (boot[11] | boot[12] << 8) == 512);
    assert(path_join(path, root, "_nds/nds-bootstrap.ini"));
    char config[4096] = "";
    file = fopen(path, "r");
    assert(file && fread(config, 1, sizeof config - 1, file) > 0 && !fclose(file));
    const char *quit = strstr(config, "QUIT_PATH=sd:/BOOT.NDS\n");
    assert(quit && !strstr(quit + 1, "QUIT_PATH=") && !strstr(config, "old-loader.nds"));
    assert(strstr(config, "APP_PATH=sd:/ROMS/WARE.DSI"));
    assert(strstr(config, "SAV_PATH=sd:/ROMS/SAVES/WARE.PUB"));
    assert(strstr(config, "PRV_PATH=sd:/ROMS/SAVES/WARE.PRV"));
    assert(strstr(config, "DSI_MODE=1") && strstr(config, "DSIWARE_SLOT1_MODE=0"));
    /* Existing save data is reused byte-for-byte. Size conflicts never resize. */
    file = fopen(plan.save, "r+b");
    assert(file && !fseek(file, 4096, SEEK_SET) && fputc(0x42, file) == 0x42 && !fclose(file));
    assert(dsiware_create_save(plan.save, plan.save_bytes, error));
    file = fopen(plan.save, "rb");
    assert(file && !fseek(file, 4096, SEEK_SET) && fgetc(file) == 0x42 && !fclose(file));
    assert(!dsiware_create_save(plan.save, 32768, error));
    /* Repeated launches must preserve both title-owned save volumes. */
    file = fopen(plan.private_save, "r+b");
    assert(file && !fseek(file, 4096, SEEK_SET) && fputc(0xA7, file) == 0xA7 && !fclose(file));
    assert(bootstrap_commit(root, &plan, error));
    const char *save_paths[] = {plan.save, plan.private_save};
    const int markers[] = {0x42, 0xA7};
    for (unsigned i = 0; i < 2; i++) {
        file = fopen(save_paths[i], "rb");
        assert(file && !fseek(file, 4096, SEEK_SET) && fgetc(file) == markers[i] && !fclose(file));
    }
    /* A private-size conflict blocks handoff without rewriting either save. */
    plan.private_bytes *= 2;
    assert(!bootstrap_commit(root, &plan, error));
    struct stat private_status;
    assert(!stat(plan.private_save, &private_status) && private_status.st_size == 8192);
    for (unsigned i = 0; i < 2; i++) {
        file = fopen(save_paths[i], "rb");
        assert(file && !fseek(file, 4096, SEEK_SET) && fgetc(file) == markers[i] && !fclose(file));
    }
    plan.private_bytes /= 2;
    plan.options.values[GAME_SLOT] = 2;
    assert(dsiware_prepare(&app, target, binary, &plan, error));
    assert(strstr(plan.save, ".pub2") && strstr(plan.private_save, ".prv2"));
    app.dsi_mode = false;
    assert(!dsiware_prepare(&app, target, binary, &plan, error));
    app.dsi_mode = true;
    plan.options.values[GAME_MODE] = 0;
    assert(!dsiware_prepare(&app, target, binary, &plan, error));
    plan.options.values[GAME_MODE] = 1;
    put32(binary + 0x238, 12345);
    assert(!dsiware_prepare(&app, target, binary, &plan, error));
    save_write_failures(root);
    for (unsigned bytes = 4096; bytes <= 33554432; bytes *= 2) {
        char name[32];
        snprintf(name, sizeof name, "volume-%u.prv", bytes);
        assert(path_join(path, root, name));
        save_writes = 0;
        measure_save_writes = true;
        assert(dsiware_create_save(path, bytes, error));
        measure_save_writes = false;
        assert(save_writes <= bytes / 4096 + 3);
        struct stat status;
        assert(!stat(path, &status) && (uint64_t)status.st_size == bytes);
        verify_volume(path, bytes);
    }
    puts("PASS: DSiWare header-derived public/private FAT12 volumes, short-path configuration, "
         "size/no-resize policy, independent slots and route constraints");
}
