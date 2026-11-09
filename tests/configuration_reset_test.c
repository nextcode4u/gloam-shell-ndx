#include "app.h"
#include "configuration_reset.h"
#include "paths.h"
#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static App app;
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
int main(int argc, char **argv)
{
    assert(argc == 2);
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
