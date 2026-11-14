#include "app.h"
#include "browser.h"
#include "paths.h"
#include "preferences.h"
#include "title_metadata.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static App app, restored;
static unsigned char data[0xC40];
int main(int argc, char **argv)
{
    assert(argc == 2);
    char path[PATH_CAP], title[128], display[256];
    assert(path_join(path, argv[1], "roms"));
    assert(!mkdir(path, 0777));
    assert(path_join(path, argv[1], "roms/Actual filename.nds"));
    data[0x69] = 2;
    data[0x200] = 3;
    memcpy(data + 12, "TST0", 4);
    const unsigned char english[] = {'C', 0, 'a', 0, 'f', 0, 0xE9, 0, 0, 0};
    const unsigned char french[] = {'J', 0, 'e', 0, 'u', 0, 0, 0};
    memcpy(data + 0x200 + 0x340, english, sizeof english);
    memcpy(data + 0x200 + 0x440, french, sizeof french);
    FILE *f = fopen(path, "wb");
    assert(f && fwrite(data, 1, sizeof data, f) == sizeof data && !fclose(f));
    assert(title_metadata_read(path, 1, title) && !strcmp(title, "Caf\xC3\xA9"));
    assert(title_metadata_read(path, 2, title) && !strcmp(title, "Jeu"));
    assert(title_metadata_read(path, 3, title) && !strcmp(title, "Caf\xC3\xA9"));
    assert(title_metadata_read(path, 7, title) && !strcmp(title, "Caf\xC3\xA9"));
    app_init(&app, argv[1], true, true);
    app_category(&app, CATEGORY_GAMES);
    assert(app.entry_count == 1);
    app.internal_titles = true;
    app.title_language = 2;
    app.browser_style = BROWSER_LIST;
    for (int i = 0; i < 5; i++)
        app_tick(&app);
    /* List stays text-only below, but prepares one selected banner above. */
    assert(app.icon_reads == 1 && app.entries[0].title_loaded);
    browser_display_name(&app, &app.entries[0], display);
    assert(!strcmp(display, "Jeu"));
    assert(!strcmp(app.entries[0].path, path));
    assert(app_save(&app));
    app_init(&restored, argv[1], true, true);
    assert(restored.internal_titles && restored.title_language == 2);
    app.internal_titles = false;
    browser_display_name(&app, &app.entries[0], display);
    assert(!strcmp(display, "Actual filename.nds"));
    /* Details remain available with filename labels and covers disabled. */
    app.detail_tab = 1;
    app.show_covers = false;
    app_reset_preview(&app);
    app.selection_idle_frames = 2;
    title_metadata_tick(&app);
    assert(app.info_loaded && app.info_header_valid && app.info_unit == 0);
    assert(!strcmp(app.info_code, "TST0") && !strcmp(app.info_title, "Jeu"));
    data[0x12] = 3;
    data[0x1e] = 7;
    f = fopen(path, "wb");
    assert(f && fwrite(data, 1, sizeof data, f) == sizeof data && !fclose(f));
    title_metadata_tick(&app);
    assert(app.info_unit == 0); /* Selection metadata is cached. */
    app_reset_preview(&app);
    assert(!app.info_loaded && !*app.info_title && !*app.info_code);
    app.selection_idle_frames = 2;
    title_metadata_tick(&app);
    assert(app.info_header_valid && app.info_unit == 3 && app.info_revision == 7);
    f = fopen(path, "wb");
    assert(f && fwrite(data, 1, 8, f) == 8 && !fclose(f));
    app_reset_preview(&app);
    app.selection_idle_frames = 2;
    title_metadata_tick(&app);
    assert(app.info_loaded && !app.info_header_valid && !*app.info_code && !*app.info_title);
    data[0x200] = 0xFF;
    f = fopen(path, "wb");
    assert(f && fwrite(data, 1, sizeof data, f) == sizeof data && !fclose(f));
    assert(!title_metadata_read(path, 1, title));
    puts("PASS: bounded localized banner titles, UTF-16 conversion, English fallback, lazy "
         "text-only browsing and preference persistence");
}
