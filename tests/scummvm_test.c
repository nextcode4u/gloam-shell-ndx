/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "arguments.h"
#include "paths.h"
#include "scummvm.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool watching, fail_open, short_read, fail_close, fail_metadata;
FILE *__real_fopen(const char *, const char *);
size_t __real_fread(void *, size_t, size_t, FILE *);
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
#endif
int __real_fclose(FILE *);
int __real_fstat(int, struct stat *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (watching && fail_open) {
        errno = EIO;
        return NULL;
    }
    return __real_fopen(path, mode);
}
size_t __wrap_fread(void *data, size_t size, size_t count, FILE *file)
{
    if (watching && short_read && count > 10)
        count = 10;
    return __real_fread(data, size, count, file);
}
#ifdef __GLIBC__
size_t __wrap___fread_chk(void *data, size_t capacity, size_t size, size_t count, FILE *file)
{
    if (watching && short_read && count > 10)
        count = 10;
    return __real___fread_chk(data, capacity, size, count, file);
}
#endif
int __wrap_fclose(FILE *file)
{
    int result = __real_fclose(file);
    if (watching && fail_close) {
        errno = EIO;
        return EOF;
    }
    return result;
}
int __wrap_fstat(int descriptor, struct stat *status)
{
    if (watching && fail_metadata) {
        errno = EIO;
        return -1;
    }
    return __real_fstat(descriptor, status);
}

static LaunchPlan plan;
static char error[160], config_path[PATH_CAP];

static void settings(const char *text)
{
    FILE *file = fopen(config_path, "wb");
    assert(file && fputs(text, file) >= 0 && !fclose(file));
}
static void reset(const char *root, const char *application)
{
    memset(&plan, 0, sizeof plan);
    assert(path_join(plan.application, root, application));
    assert(arguments_add(&plan.arguments, plan.application));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    const char *dirs[] = {"_nds",
                          "_nds/dsi-frontend",
                          "_nds/dsi-frontend/plugins",
                          "_nds/dsi-frontend/plugins/startup",
                          "roms",
                          "roms/ScummVM",
                          "roms/ScummVM/titles",
                          "Alternate titles"};
    char path[PATH_CAP], expected[PATH_CAP + 8];
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        assert(path_join(path, root, dirs[i]));
        assert(!mkdir(path, 0777));
    }
    assert(path_join(config_path, root, "_nds/dsi-frontend/plugins/startup/scummvm.ini"));
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 1);
    settings("[scummvm]\napplication=/roms/ScummVM/scummvm.nds\n"
             "titles=/roms/ScummVM/titles\nauto_add=1\nrecursive=1\n");
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 5);
    assert(!strcmp(arguments_get(&plan.arguments, 1), "--add"));
    assert(!strcmp(arguments_get(&plan.arguments, 2), "--recursive"));
    assert(path_join(path, root, "roms/ScummVM/titles"));
    snprintf(expected, sizeof expected, "--path=%s", path);
    assert(!strcmp(arguments_get(&plan.arguments, 3), expected));
    assert(!strcmp(arguments_get(&plan.arguments, 4), "--no-exit"));
    /* A second launch still requests discovery, including folders added since last time. */
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 5);
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(arguments_add(&plan.arguments, "custom-game-id"));
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 2);
    reset(root, "roms/ScummVM/scummvm.nds");
    plan.route = LAUNCH_BOOTSTRAP_HB;
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 1);
    settings("\xEF\xBB\xBF"
             "[scummvm]\r\n titles = /Alternate titles\r\nrecursive=0\r\n");
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 4);
    assert(path_join(path, root, "Alternate titles"));
    snprintf(expected, sizeof expected, "--path=%s", path);
    assert(!strcmp(arguments_get(&plan.arguments, 2), expected));
    settings("[scummvm]\nauto_add=0\n");
    /* Never turn an unreadable/partial config into implicit discovery defaults. */
    for (unsigned failure = 0; failure < 4; failure++) {
        reset(root, "roms/ScummVM/scummvm.nds");
        LaunchPlan original = plan;
        fail_open = failure == 0;
        short_read = failure == 1;
        fail_close = failure == 2;
        fail_metadata = failure == 3;
        watching = true;
        bool prepared = scummvm_prepare_arguments(root, &plan, error);
        watching = fail_open = short_read = fail_close = fail_metadata = false;
        assert(!prepared && *error && !memcmp(&original, &plan, sizeof plan));
    }
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 1);
    settings("[scummvm]\ntitles=/missing\n");
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 1);
    settings("[scummvm]\ntitles=/../escape\n");
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(!scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 1);
    settings("[scummvm]\nrecursive=invalid\n");
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(!scummvm_prepare_arguments(root, &plan, error));
    reset(root, "apps/other.nds");
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 1);
    settings("[scummvm]\napplication=/apps/scummvm.nds\n");
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(scummvm_prepare_arguments(root, &plan, error) && plan.arguments.count == 1);
    FILE *file = fopen(config_path, "wb");
    assert(file);
    for (int i = 0; i < 4097; ++i)
        assert(fputc('x', file) != EOF);
    assert(!fclose(file));
    reset(root, "roms/ScummVM/scummvm.nds");
    assert(!scummvm_prepare_arguments(root, &plan, error));
    puts("ScummVM startup argument tests passed.");
}
