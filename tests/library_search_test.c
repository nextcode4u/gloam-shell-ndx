#include "app.h"
#include "browser.h"
#include "library_search.h"
#include "paths.h"
#include "picker.h"
#include "renderer.h"
#include "storage_state.h"
#include "tabs.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static App app;
static unsigned index_reads;
FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (strstr(path, "/cache/library.gli") && mode[0] == 'r')
        index_reads++;
    return __real_fopen(path, mode);
}
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} top, bottom;
static void draw(void)
{
    top.before = top.after = bottom.before = bottom.after = 0x1234ABCD;
    render_app(&app, top.pixels, bottom.pixels, "12:00");
    assert(top.before == 0x1234ABCD && top.after == 0x1234ABCD);
    assert(bottom.before == 0x1234ABCD && bottom.after == 0x1234ABCD);
}
static void install(const char *root, const char *name)
{
    char path[PATH_CAP];
    assert(path_join(path, root, name));
    FILE *in = fopen(path, "rb");
    assert(path_preferences_file(path, root, "cache/library.gli"));
    FILE *out = fopen(path, "wb");
    assert(in && out);
    unsigned char buffer[4096];
    size_t size;
    while ((size = fread(buffer, 1, sizeof buffer, in)))
        assert(fwrite(buffer, 1, size, out) == size);
    assert(!ferror(in) && !fclose(in) && !fclose(out));
}
static void finish(void)
{
    unsigned steps = 0;
    while (app.search->pending && steps++ < 10000)
        library_search_step(&app);
    assert(!app.search->pending);
    draw();
}
static void query(const char *text)
{
    path_copy(app.search->query, text, SEARCH_QUERY_CAP);
    library_search_start(&app);
    finish();
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    assert(sizeof(LibrarySearch) < 14 * 1024);
    app_init(&app, root, true, true);
    assert(!app.search && !index_reads); /* No index I/O or allocation during boot. */
    app_category(&app, CATEGORY_GAMES);
    assert(library_search_open(&app));
    assert(app.search->available && !app.search->file);
    draw();
    strcpy(app.search->query, "no match anywhere");
    install(root, "large.gli");
    library_search_start(&app);
    library_search_step(&app);
    assert(app.search->visited > 0 && app.search->decoded <= 64 && app.search->pending);
    app_back(&app);
    assert(!app.search && !app.launch_requested); /* In-flight cancellation closes the stream. */
    install(root, "valid.gli");
    assert(library_search_open(&app));
    query("gAmE");
    assert(app.search->count == 16 && app.search->more && app.search->page == 0);
    char first[PATH_CAP];
    path_copy(first, app.search->results[0], PATH_CAP);
    library_search_page(&app, 1);
    finish();
    assert(app.search->page == 1 && !strstr(app.search->results[0], "Game0000"));
    library_search_page(&app, -1);
    finish();
    assert(app.search->page == 0 && !strcmp(first, app.search->results[0]));
    library_search_page(&app, 1);
    finish();
    library_search_edit(&app);
    library_search_start(&app);
    finish();
    assert(app.search->page == 0 && !strcmp(first, app.search->results[0]));
    query("Game0012");
    assert(app.search->count == 1);
    query("Game0012z");
    assert(!app.search->count); /* Last mismatching character must not be accepted. */
    query("books über");
    assert(!app.search->count); /* Current Games scope excludes Media. */
    library_search_scope(&app);
    query("books ÜBER");
    assert(app.search->count == 1 && strstr(app.search->results[0], "Überblick"));
    query("日本");
    assert(app.search->count == 1);
    library_search_edit(&app);
    library_search_erase(&app);
    assert(!strcmp(app.search->query, "日"));
    query("Hidden");
    assert(!app.search->count);
    app.show_hidden = true;
    query("Hidden");
    assert(app.search->count == 1);
    query("System game");
    assert(!app.search->count);
    app.show_system = true;
    query("System game");
    assert(app.search->count == 1);
    /* Search must not expose descendants of folders hidden in the browser.
     * Folder matching is case-insensitive, whole-component and excludes filenames. */
    query("SaveNeedle");
    assert(!app.search->count);
    query("StateNeedle");
    assert(!app.search->count);
    query("FirmwareNeedle");
    assert(!app.search->count);
    app.hide_save_folders = false;
    query("SaveNeedle");
    assert(app.search->count == 1);
    query("StateNeedle");
    assert(app.search->count == 1);
    query("FirmwareNeedle");
    assert(!app.search->count);
    app.hide_bios_folders = false;
    query("FirmwareNeedle");
    assert(app.search->count == 1);
    app.hide_save_folders = app.hide_bios_folders = true;
    /* An explicitly assigned tab root remains browsable even when its own
     * name is "saves"; only hidden descendants of that root are filtered. */
    int direct = tabs_find(&app, "extra");
    assert(direct >= CATEGORY_COUNT);
    char previous_root[PATH_CAP];
    path_copy(previous_root, app.tabs[direct].home, PATH_CAP);
    assert(path_join(app.tabs[direct].home, app.root, "roms/SAVES"));
    query("SaveNeedle");
    assert(app.search->count == 1);
    app.search->all = false;
    query("SaveNeedle");
    assert(!app.search->count); /* Games still hides that descendant. */
    app.search->all = true;
    path_copy(app.tabs[direct].home, previous_root, PATH_CAP);
    query("VisibleNeedle");
    assert(app.search->count == 1);
    query("saves.nds");
    assert(app.search->count == 1);
    query("bios.nds");
    assert(app.search->count == 1);
    query("Ignore");
    assert(!app.search->count); /* Outside configured library roots. */
    app.filter_recognized = true;
    query("Game0012");
    assert(app.search->count == 1); /* Recognized TXT files also appear in browsing. */
    library_search_locate(&app);
    assert(!app.search && app_selected(&app) && app.filter_recognized);
    assert(strstr(app_selected(&app)->path, "/Game0012.txt"));
    assert(library_search_open(&app));
    library_search_scope(&app); /* Continue checking all libraries. */
    query("Standalone");
    assert(app.search->count == 1); /* A known image type does not require a player. */
    query("Unsupported");
    assert(!app.search->count);
    query("Player");
    assert(app.search->count == 1);
    library_search_activate(&app);
    assert(app.search && app.panel == PANEL_MESSAGE && !app.launch_requested);
    draw();
    app_back(&app);
    assert(app.search && app.panel == PANEL_NONE);
    query("Notes.md");
    assert(app.search->count == 1);
    library_search_activate(&app);
    assert(app.search && app.picker.open && app.picker.kind == PICKER_HANDLER);
    draw();
    app_back(&app);
    assert(app.search && !app.picker.open);
    app.filter_recognized = false;
    query("MovedAway");
    assert(app.search->count == 1);
    library_search_activate(&app);
    assert(app.search && strstr(app.search->status, "moved"));
    /* Locate a title in a later folder batch, retaining the normal browser/launch path. */
    library_search_close(&app);
    app_category(&app, CATEGORY_GAMES);
    browser_batch(&app, 1);
    assert(app.locations[app.category].batch == 1 && app.entry_count);
    char later[PATH_CAP];
    later[0] = 0;
    for (int i = 0; i < app.entry_count; i++)
        if (app.entries[i].kind == ITEM_FILE) {
            path_copy(later, app.entries[i].path, PATH_CAP);
            break;
        }
    assert(*later);
    assert(library_search_open(&app));
    query(strrchr(later, '/') + 1);
    assert(app.search->count == 1);
    library_search_locate(&app);
    assert(!app.search && app_selected(&app) && !strcmp(app_selected(&app)->path, later));
    assert(app.locations[app.category].batch == 1 && !app.launch_requested);
    assert(library_search_open(&app));
    /* Special keys stay recognizable independently of the active font, and
     * use the same focus ink and guarded framebuffer as ordinary keys. */
    for (unsigned key = 40; key <= 42; key++) {
        app.search->cursor = key;
        draw();
        unsigned ink_pixels = 0;
        int x = 8 + (key - 40) * 80 + 31;
        for (int y = 147; y < 163; y++)
            for (int xx = x; xx < x + 16; xx++)
                ink_pixels +=
                    bottom.pixels[y * 256 + xx] == themes_selection_text(&app.themes.active);
        assert(ink_pixels >= 20);
    }
    app.search->cursor = 0;
    library_search_touch(&app, 9, 55); /* First key: q. */
    assert(!strcmp(app.search->query, "q"));
    library_search_touch(&app, 10, 150); /* Space. */
    assert(!strcmp(app.search->query, "q "));
    library_search_touch(&app, 90, 150); /* Backspace. */
    assert(!strcmp(app.search->query, "q"));
    library_search_touch(&app, 90, 150);
    assert(!*app.search->query);
    library_search_touch(&app, 170, 150); /* Search. */
    assert(!app.search->editing);
    finish();
    library_search_edit(&app);
    for (int i = 0; i < 100; i++)
        library_search_move(&app, -1, -1);
    assert(app.search->cursor < 43);
    draw();
    library_search_touch(&app, 10, 180);
    assert(!app.search);
    app_panel(&app, PANEL_ACTIONS);
    app.panel_selection = 2;
    app_search_open(&app); /* Same entry point as X in the item menu. */
    assert(app.search && app.panel == PANEL_NONE);
    app_back(&app);
    assert(!app.search && app.panel == PANEL_ACTIONS && app.panel_selection == 2);
    app_panel_close(&app);
    /* Custom roots bound search and locate chooses the most specific tab. */
    int collection = tabs_find(&app, "collection");
    assert(collection >= CATEGORY_COUNT);
    app_category(&app, collection);
    assert(library_search_open(&app) && !app.search->all);
    query("Escape");
    assert(!app.search->count); /* A similarly named sibling is outside this tab. */
    query("Scoped");
    assert(app.search->count == 1);
    library_search_locate(&app);
    assert(!app.search && app.category == collection && app_selected(&app));
    assert(strstr(app_selected(&app)->path, "/Collection/Deep/Scoped.nds"));
    assert(!app.launch_requested);
    app_back(&app);
    assert(!strcmp(app.locations[collection].path, app.tabs[collection].home));
    app_back(&app);
    assert(!strcmp(app.locations[collection].path, app.tabs[collection].home));
    app_category(&app, CATEGORY_GAMES);
    assert(library_search_open(&app));
    for (int n = 0; n < 32; n++) {
        char name[64];
        snprintf(name, sizeof name, "truncated-%d.gli", n);
        install(root, name);
        assert(library_search_open(&app) && !app.search->available);
        draw();
        app_back(&app);
        assert(!app.search);
    }
    for (int n = 0; n < 8; n++) {
        char name[64];
        snprintf(name, sizeof name, "bad-path-%d.gli", n);
        install(root, name);
        assert(library_search_open(&app));
        query("");
        assert(!app.search->available && !app.search->count && !app.search->file);
        library_search_close(&app);
    }
    for (int n = 0; n < 6; n++) {
        char name[64];
        snprintf(name, sizeof name, "bad-block-%d.gli", n);
        install(root, name);
        assert(library_search_open(&app));
        query("");
        assert(!app.search->available && !app.search->count && !app.search->file);
        library_search_close(&app);
    }
    const char *broken[] = {"bad-checksum.gli", "bad-count.gli", "bad-zero-count.gli",
                            "bad-table.gli"};
    for (unsigned n = 0; n < sizeof broken / sizeof *broken; n++) {
        install(root, broken[n]);
        assert(library_search_open(&app));
        query("no matching term");
        assert(!app.search->available && !app.search->file && !app.search->count);
        library_search_close(&app);
    }
    install(root, "empty.gli");
    assert(library_search_open(&app));
    query("");
    assert(app.search->available && !app.search->count);
    library_search_close(&app);
    install(root, "large.gli");
    assert(library_search_open(&app));
    query("FinalNeedle");
    assert(app.search->records == 50000 && app.search->visited == 50000);
    assert(app.search->blocked && app.search->count == 1 && app.search->decoded < 128);
    library_search_close(&app);
    install(root, "legacy.gli");
    assert(library_search_open(&app));
    query("Game0012");
    assert(!app.search->blocked && app.search->count == 1);
    query("");
    library_search_page(&app, 1);
    finish();
    assert(strstr(app.search->results[0], "Game0016"));
    library_search_close(&app);
    install(root, "valid.gli");
    assert(library_search_open(&app));
    library_search_start(&app);
    storage_observe(&app, false);
    assert(!app.search && app.storage_lost && !app.launch_requested);
    draw();
    puts("PASS: no boot index I/O, bounded/paged Unicode search, scope/filters, stale/malformed "
         "indexes, late-batch locate, touch/back and SD-loss cleanup");
}
