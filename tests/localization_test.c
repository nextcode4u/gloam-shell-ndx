#include "app.h"
#include "localization.h"
#include "paths.h"
#include "preferences.h"
#include "settings.h"
#include "tabs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static App app, restored;
static void write_text(const char *root, const char *name, const char *text)
{
    char path[PATH_CAP];
    assert(path_preferences_file(path, root, name));
    FILE *f = fopen(path, "w");
    assert(f && fputs(text, f) >= 0 && !fclose(f));
}
int main(int argc, char **argv)
{
    assert(argc == 3 && path_prepare_preferences_directory(argv[1]));
    char directory[PATH_CAP];
    assert(path_preferences_file(directory, argv[1], "languages") && !mkdir(directory, 0777));
    write_text(argv[1], "language.ini", "version=1\nhome=Legacy home\n");
    write_text(argv[1], "languages/sample.ini", "version=1\nhome=Catalog home\n");
    write_text(argv[1], "languages/bad.ini", "version=99\nhome=Invalid\n");
    app_init(&app, argv[1], true, true);
    assert(!strcmp(app.interface_language, "auto") && !strcmp(tabs_label(&app, 0), "Legacy home"));
    settings_open(&app);
    assert(app.settings.locale_count == 4);
    assert(!strcmp(app.settings.locales[2], "bad") && !strcmp(app.settings.locales[3], "sample"));
    localization_adjust(&app, 1);
    assert(!strcmp(app.interface_language, "en") && !strcmp(tabs_label(&app, 0), "Home"));
    localization_adjust(&app, 1);
    assert(!strcmp(app.interface_language, "bad") && !app.language.loaded);
    assert(strstr(app.settings.status, "unavailable") && !strcmp(tabs_label(&app, 0), "Home"));
    localization_adjust(&app, 1);
    assert(!strcmp(app.interface_language, "sample") && app.language.loaded);
    assert(!strcmp(tabs_label(&app, 0), "Catalog home"));
    assert(app_save(&app));
    app_init(&restored, argv[1], true, true);
    assert(!strcmp(restored.interface_language, "sample") && restored.language.loaded);
    assert(!strcmp(tabs_label(&restored, 0), "Catalog home"));
    app_init_mode(&restored, argv[1], true, true, true);
    assert(!restored.language.loaded && !strcmp(tabs_label(&restored, 0), "Home"));
    assert(app_save(&restored));
    app_init(&restored, argv[1], true, true);
    assert(!strcmp(restored.interface_language, "sample") && restored.language.loaded);
    assert(!ui_locale_valid("../escape") && !ui_locale_valid("ja.ini") && !ui_locale_valid(""));
    assert(ui_locale_valid("pt-BR") && ui_locale_valid("zh_Hant"));
    ui_language_load_selected(&restored.language, argv[1], true, "../escape");
    assert(!restored.language.loaded);
    ui_language_load_selected(&restored.language, argv[2], true, "ja");
    assert(restored.language.loaded && !strcmp(ui_text(&restored.language, UI_HOME), "ホーム"));
    ui_language_load_selected(&restored.language, argv[2], true, "es");
    assert(restored.language.loaded && strcmp(ui_text(&restored.language, UI_HOME), "Home"));
    puts("PASS: external locale discovery, legacy catalog, English override, live labels, invalid "
         "fallback, persistence and recovery");
}
