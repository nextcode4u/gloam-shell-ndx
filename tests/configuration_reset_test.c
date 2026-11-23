#include "app.h"
#include "configuration_reset.h"
#include "paths.h"
#include "settings.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static App app;
static const char *watched_state;
static FILE *created_state, *read_state;
static bool fail_write, fail_close, fail_cleanup, prefix_read, late_edit;
static unsigned state_reads;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_fputs(const char *, FILE *);
size_t __real_fwrite(const void *, size_t, size_t, FILE *);
size_t __real_fread(void *, size_t, size_t, FILE *);
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
#endif
int __real_remove(const char *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    bool watched = watched_state && !strcmp(path, watched_state);
    if (watched && *mode == 'r' && ++state_reads == 2 && late_edit) {
        const char replacement[] = "external edit after preflight\n";
        FILE *changed = __real_fopen(path, "wb");
        assert(changed &&
               __real_fwrite(replacement, 1, sizeof replacement - 1, changed) ==
                   sizeof replacement - 1 &&
               !__real_fclose(changed));
    }
    FILE *file = __real_fopen(path, mode);
    if (watched && strchr(mode, 'x'))
        created_state = file;
    if (watched && *mode == 'r')
        read_state = file;
    return file;
}
int __wrap_fputs(const char *text, FILE *file)
{
    if (file == created_state && fail_write) {
        assert(__real_fwrite(text, 1, 3, file) == 3);
        errno = ENOSPC;
        return EOF;
    }
    return __real_fputs(text, file);
}
size_t __wrap_fwrite(const void *data, size_t size, size_t count, FILE *file)
{
    if (file == created_state && fail_write) {
        size_t bytes = size * count, prefix = bytes > 3 ? 3 : bytes / 2;
        assert(__real_fwrite(data, 1, prefix, file) == prefix);
        errno = ENOSPC;
        return size ? prefix / size : 0;
    }
    return __real_fwrite(data, size, count, file);
}
size_t __wrap_fread(void *data, size_t size, size_t count, FILE *file)
{
    if (file == read_state && prefix_read && count > 10)
        count = 10;
    return __real_fread(data, size, count, file);
}
#ifdef __GLIBC__
size_t __wrap___fread_chk(void *data, size_t capacity, size_t size, size_t count, FILE *file)
{
    if (file == read_state && prefix_read && count > 10)
        count = 10;
    return __real___fread_chk(data, capacity, size, count, file);
}
#endif
int __wrap_fclose(FILE *file)
{
    bool deny = file == created_state && fail_close;
    if (file == created_state)
        created_state = NULL;
    if (file == read_state)
        read_state = NULL;
    int result = __real_fclose(file);
    if (deny) {
        errno = ENOSPC;
        return EOF;
    }
    return result;
}
int __wrap_remove(const char *path)
{
    if (fail_cleanup && watched_state && !strcmp(path, watched_state)) {
        errno = EACCES;
        return -1;
    }
    return __real_remove(path);
}
static void write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    assert(file && fputs(text, file) >= 0 && !fclose(file));
}
static void check_text(const char *path, const char *expected)
{
    char text[128] = "";
    FILE *file = fopen(path, "r");
    assert(file);
    size_t bytes = fread(text, 1, sizeof text - 1, file);
    assert(bytes == strlen(expected) && !strcmp(text, expected) && !fclose(file));
}
static void failed_reset_writes(const char *root)
{
    for (unsigned failure = 0; failure < 3; failure++) {
        char folder[PATH_CAP], name[32], state[PATH_CAP], tabs[PATH_CAP], pending[PATH_CAP];
        char saved[PATH_CAP], untouched[PATH_CAP], error[160];
        snprintf(name, sizeof name, "reset-write-%u", failure);
        assert(path_join(folder, root, name) && !mkdir(folder, 0777));
        assert(path_prepare_preferences_directory(folder));
        assert(path_preferences_file(state, folder, "state.ini"));
        assert(path_preferences_file(tabs, folder, "tabs.ini"));
        assert(path_preferences_file(pending, folder, "configuration/reset-pending"));
        assert(path_join(saved, pending, "00"));
        assert(path_join(untouched, folder, "save.sav"));
        write_text(state, "version=1\nshow_hidden=1\n");
        write_text(tabs, "version=1\n# keep my tabs\n");
        write_text(untouched, "A save must survive");
        watched_state = state;
        fail_write = failure != 1;
        fail_close = failure == 1;
        fail_cleanup = failure == 2;
        assert(!configuration_reset(folder, error));
        fail_write = fail_close = fail_cleanup = false;
        watched_state = NULL;
        if (failure == 2) {
            check_text(saved, "version=1\nshow_hidden=1\n");
            assert(!access(pending, F_OK));
            assert(!remove(state)); /* The fixture owns this intentionally blocked partial file. */
            assert(configuration_reset_recover(folder, error));
        }
        check_text(state, "version=1\nshow_hidden=1\n");
        check_text(tabs, "version=1\n# keep my tabs\n");
        check_text(untouched, "A save must survive");
        assert(access(pending, F_OK) && errno == ENOENT);
    }
}
static void external_reset_edits(const char *root)
{
    for (unsigned late = 0; late < 2; late++) {
        char folder[PATH_CAP], name[32], state[PATH_CAP], pending[PATH_CAP], saved[PATH_CAP];
        char path[PATH_CAP], error[160];
        snprintf(name, sizeof name, "reset-edit-%u", late);
        assert(path_join(folder, root, name) && !mkdir(folder, 0777));
        assert(path_prepare_preferences_directory(folder));
        assert(path_preferences_file(path, folder, "configuration") && !mkdir(path, 0777));
        assert(path_preferences_file(state, folder, "state.ini"));
        assert(path_preferences_file(pending, folder, "configuration/reset-pending"));
        assert(!mkdir(pending, 0777) && path_join(saved, pending, "00"));
        write_text(saved, "version=1\nshow_hidden=1\n");
        write_text(state, late ? "version=1\n" : "version=1\nexternal_edit=keep\n");
        watched_state = state;
        state_reads = 0;
        late_edit = late != 0;
        prefix_read = !late;
        assert(!configuration_reset_recover(folder, error));
        prefix_read = late_edit = false;
        watched_state = NULL;
        check_text(state,
                   late ? "external edit after preflight\n" : "version=1\nexternal_edit=keep\n");
        check_text(saved, "version=1\nshow_hidden=1\n");
        assert(!access(pending, F_OK));
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    failed_reset_writes(argv[1]);
    external_reset_edits(argv[1]);
    char state[PATH_CAP], old[PATH_CAP], pending[PATH_CAP], path[PATH_CAP], other[PATH_CAP];
    char error[160] = "";
    assert(path_prepare_preferences_directory(argv[1]));
    assert(path_preferences_file(state, argv[1], "state.ini"));
    write_text(state, "version=1\nshow_hidden=1\n");
    assert(path_preferences_file(path, argv[1], "state.bak"));
    write_text(path, "version=1\nshow_hidden=1\n");
    assert(path_preferences_file(path, argv[1], "games"));
    assert(!mkdir(path, 0777));
    assert(path_join(other, path, "0123456789abcdef.ini"));
    write_text(other, "content=sd:/game.nds\n");
    assert(path_preferences_file(path, argv[1], "save.sav"));
    write_text(path, "A save must survive");
    assert(configuration_reset(argv[1], error));
    check_text(state, "version=1\n");
    check_text(path, "A save must survive");
    assert(path_preferences_file(old, argv[1], "configuration/reset-backup-000/00"));
    check_text(old, "version=1\nshow_hidden=1\n");
    assert(access(other, F_OK));
    app_init(&app, argv[1], true, true);
    assert(!app.show_hidden && app.favorite_count == 0);
    /* Restore an interrupted move at next startup, but refuse a conflicting
     * external edit without partially restoring other records. */
    assert(path_preferences_file(pending, argv[1], "configuration/reset-pending"));
    assert(!mkdir(pending, 0777));
    assert(path_join(old, pending, "00"));
    assert(!rename(state, old));
    write_text(state, "external edit\n");
    assert(!configuration_reset_recover(argv[1], error));
    check_text(state, "external edit\n");
    assert(!remove(state));
    app_init(&app, argv[1], true, true);
    assert(app.storage_ready && access(pending, F_OK));
    check_text(state, "version=1\n");
    /* Redirection is rejected before any records are moved. */
    assert(path_preferences_file(path, argv[1], "games"));
    assert(!symlink("/tmp", path));
    assert(!configuration_reset(argv[1], error));
    check_text(state, "version=1\n");
    assert(!unlink(path));
    app.advanced_settings = true;
    settings_open(&app);
    app.settings.section = 5;
    for (int i = 0; i < settings_option_count(&app, app.settings.section); i++)
        if (!strcmp(settings_option(&app, app.settings.section, i)->label, "Reset configuration"))
            app.settings.selected = i;
    settings_activate(&app);
    assert(app.settings.reset_confirm && !app.configuration_reload_requested);
    settings_move(&app, -1);
    assert(!app.settings.reset_confirm);
    settings_move(&app, 1);
    settings_activate(&app);
    settings_back(&app);
    assert(!app.settings.reset_confirm);
    app.advanced_settings = true;
    settings_open(&app);
    app.settings.section = 5;
    app.settings.selected = settings_option_count(&app, app.settings.section) - 1;
    settings_activate(&app);
    settings_activate(&app);
    assert(app.configuration_reload_requested && !app.settings.open);
    assert(path_preferences_file(old, argv[1], "configuration/reset-backup-001/00"));
    assert(!access(old, F_OK));
    app_init_mode(&app, argv[1], true, true, true);
    app.advanced_settings = true;
    settings_open(&app);
    app.settings.section = 5;
    app.settings.selected = settings_option_count(&app, app.settings.section) - 1;
    settings_activate(&app);
    assert(!app.settings.reset_confirm && !app.configuration_reload_requested);
    puts("PASS: archived reset, default reload, saves retained, interrupted rollback, external "
         "edits preserved, symlinks rejected, confirmation/cancel and recovery gates");
}
