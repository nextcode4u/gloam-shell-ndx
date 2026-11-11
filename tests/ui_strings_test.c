#include "app.h"
#include "paths.h"
#include "tabs.h"
#include "ui_strings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static App app;
static void catalog(const char *root, const char *text)
{
    char path[PATH_CAP];
    assert(path_preferences_file(path, root, "language.ini"));
    FILE *file = fopen(path, "wb");
    assert(file && fputs(text, file) >= 0 && !fclose(file));
}
int main(int argc, char **argv)
{
    assert(argc == 2 && path_prepare_preferences_directory(argv[1]));
    catalog(argv[1], "version=1\ngames=Jeux\nsettings=Réglages\n");
    app_init(&app, argv[1], true, true);
    assert(app.language.loaded && !strcmp(tabs_label(&app, CATEGORY_GAMES), "Jeux"));
    assert(!strcmp(ui_text(&app.language, UI_SETTINGS), "Réglages"));
    assert(!strcmp(ui_text(&app.language, UI_FILES), "Files"));
    const char *bad[] = {"version=1\ngames=First\ngames=Second\n", "version=2\ngames=Jeux\n",
                         "version=1\nunknown=No\n", "version=1\ngames=\xc0\xaf\n"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        catalog(argv[1], bad[i]);
        ui_language_load(&app.language, argv[1], true);
        assert(!app.language.loaded && !strcmp(ui_text(&app.language, UI_GAMES), "Games"));
    }
    char too_long[96] = "version=1\ngames=";
    size_t prefix = strlen(too_long);
    memset(too_long + prefix, 'A', 64);
    too_long[prefix + 64] = 0;
    catalog(argv[1], too_long);
    ui_language_load(&app.language, argv[1], true);
    assert(!app.language.loaded);
    catalog(argv[1], "version=1\ngames=Jeux\n");
    app_init_mode(&app, argv[1], true, true, true);
    assert(!app.language.loaded && !strcmp(tabs_label(&app, CATEGORY_GAMES), "Games"));
    puts("PASS: partial UTF-8 UI catalog, English fallback, malformed catalog rejection and "
         "recovery bypass");
}
