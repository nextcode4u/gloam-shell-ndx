/* Exercise FAT's non-replacing rename semantics and failed save transactions. */
#include "paths.h"
#include "preferences.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static App app;
static bool deny_open, deny_close, deny_remove, deny_rotate, deny_install, deny_restore;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_remove(const char *);
int __real_rename(const char *, const char *);
static bool named(const char *path, const char *name)
{
    const char *base = strrchr(path, '/');
    return !strcmp(base ? base + 1 : path, name);
}
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (deny_open && named(path, "state.tmp") && *mode == 'w') {
        errno = EROFS;
        return NULL;
    }
    return __real_fopen(path, mode);
}
int __wrap_fclose(FILE *file)
{
    int result = __real_fclose(file);
    if (deny_close) {
        errno = ENOSPC;
        return EOF;
    }
    return result;
}
int __wrap_remove(const char *path)
{
    if (deny_remove && named(path, "state.bak")) {
        errno = EACCES;
        return -1;
    }
    return __real_remove(path);
}
int __wrap_rename(const char *from, const char *to)
{
    if ((deny_rotate && named(from, "state.ini")) || (deny_install && named(from, "state.tmp")) ||
        (deny_restore && named(from, "state.bak"))) {
        errno = EIO;
        return -1;
    }
    if (access(to, F_OK) == 0) {
        errno = EEXIST; /* FatFs does not implement POSIX destination replacement. */
        return -1;
    }
    return __real_rename(from, to);
}
static void put(const char *path, const char *text)
{
    FILE *f = __real_fopen(path, "w");
    assert(f && fputs(text, f) >= 0);
    assert(__real_fclose(f) == 0);
}
static void expect(const char *path, const char *text)
{
    char line[128];
    FILE *f = __real_fopen(path, "r");
    assert(f && fgets(line, sizeof line, f));
    assert(!strcmp(line, text));
    assert(__real_fclose(f) == 0);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    path_copy(app.root, argv[1], sizeof app.root);
    app.storage_ready = true;
    assert(path_prepare_preferences_directory(app.root));
    char current[PATH_CAP], backup[PATH_CAP];
    assert(path_preferences_file(current, app.root, "state.ini"));
    assert(path_preferences_file(backup, app.root, "state.bak"));
    put(current, "original\n");
    put(backup, "older\n");

    deny_open = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "open state.tmp"));
    expect(current, "original\n");
    deny_open = false;
    deny_close = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "write state.tmp"));
    expect(current, "original\n");
    deny_close = false;
    deny_remove = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "remove old state.bak"));
    expect(current, "original\n");
    expect(backup, "older\n");
    deny_remove = false;
    deny_rotate = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "back up state.ini"));
    expect(current, "original\n");
    deny_rotate = false;
    deny_install = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "install state.ini"));
    expect(current, "original\n"); /* Successful rollback. */
    deny_restore = true;
    assert(!app_save(&app) && strstr(app.preferences_error, "kept in state.bak"));
    expect(backup, "original\n");
    assert(access(current, F_OK) != 0);
    deny_restore = false;
    assert(!app_save(&app)); /* Retrying must not delete the sole recovery copy. */
    expect(backup, "original\n");
    assert(app.preferences_dirty);
    deny_install = false;
    assert(app_save(&app) && !app.preferences_dirty && !*app.preferences_error);
    expect(current, "version=1\n");
    expect(backup, "original\n");
    assert(app_save(&app)); /* Repeated saves with FAT rename behavior. */
    expect(backup, "version=1\n");
    assert(path_commit_file(current, current, backup) == FILE_COMMIT_INSPECT);
    expect(current, "version=1\n");
    puts("PASS: FAT save rotation, read-only/full-disk errors, rollback and recovery-copy "
         "preservation");
}
