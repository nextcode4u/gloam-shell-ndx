/* Exercise real cooperative preview cancellation without ASan's unsupported
 * alternate-stack instrumentation. Renderer bounds have separate ASan suites. */
#include "app.h"
#include "artwork.h"
#include "font_pack.h"
#include "image_job.h"
#include "paths.h"
#include "storage_state.h"
#include "themes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static App app;
static void write_theme(const char *root, const char *id, const char *png)
{
    char relative[160], path[PATH_CAP];
    snprintf(relative, sizeof relative, "_nds/dsi-frontend/themes/%s/theme.ini", id);
    assert(path_join(path, root, relative));
    assert(path_prepare_preferences_directory(root));
    char directory[PATH_CAP];
    assert(path_join(directory, root, "_nds/dsi-frontend/themes"));
    mkdir(directory, 0777);
    snprintf(relative, sizeof relative, "_nds/dsi-frontend/themes/%s", id);
    assert(path_join(directory, root, relative));
    assert(!mkdir(directory, 0777));
    FILE *file = fopen(path, "w");
    assert(file);
    assert(fprintf(file,
                   "[theme]\nversion=1\nname=%s\n[backgrounds]\ntop=picture.png\n"
                   "bottom=picture.png\n",
                   id) > 0);
    assert(!fclose(file));
    snprintf(relative, sizeof relative, "_nds/dsi-frontend/themes/%s/picture.png", id);
    assert(path_join(path, root, relative));
    FILE *source = fopen(png, "rb");
    file = fopen(path, "wb");
    assert(source && file);
    unsigned char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof buffer, source)))
        assert(fwrite(buffer, 1, count, file) == count);
    assert(!ferror(source));
    assert(!fclose(source) && !fclose(file));
}
static void settle(void)
{
    for (unsigned frame = 0; app.themes.pending || app.themes.loading; frame++) {
        assert(frame < 20000);
        app_tick(&app);
    }
}
static void start(void)
{
    themes_preview(&app);
    for (int frame = 0; !app.themes.loading; frame++) {
        assert(frame < 10);
        app_tick(&app);
    }
    assert(image_job_task_pending() && !app.themes.ready);
    assert(!themes_preview_visible());
}
int main(int argc, char **argv)
{
    assert(argc == 3);
    write_theme(argv[1], "alpha", argv[2]);
    write_theme(argv[1], "beta", argv[2]);
    app_init(&app, argv[1], true, true);
    themes_open(&app);
    settle();
    assert(app.themes.ready && app.themes.count == 3);
    themes_move(&app, 1);
    assert(app.themes.selected == 1 && !app.themes.pending && !app.themes.ready);
    themes_touch(&app, 20, 62); /* Tap the selected row to request a preview. */
    assert(app.themes.pending && !app.themes.loading);
    themes_move(&app, 1); /* A different row cancels that request before any decode. */
    themes_move(&app, -1);
    assert(!app.themes.pending && !app.themes.ready);
    for (int frame = 0; frame < 120; frame++) {
        app_tick(&app);
        assert(!image_job_task_pending());
    }
    start();
    unsigned yields = image_job_yields();
    assert(yields && !image_job_take());
    /* Artwork polling must never resume or consume the theme-owned context. */
    assert(!artwork_cache_update() && image_job_yields() == yields);
    assert(font_current() == &app.themes.active.font);
    themes_preview(&app); /* Repeated X must not restart an in-progress decode. */
    assert(image_job_yields() == yields);
    themes_apply(&app); /* Apply cannot commit or queue partially loaded assets. */
    assert(app.themes.open && !*app.theme_id);
    themes_move(&app, 1); /* Supersede and unwind the paused PNG decoder. */
    assert(app.themes.selected == 2 && !image_job_busy() && !app.themes.pending);
    assert(!app.themes.ready && !*app.theme_id);
    themes_move(&app, -1); /* Beta is skipped without starting its decode. */
    start();
    themes_apply(&app);
    settle();
    assert(app.themes.open && app.themes.ready && !*app.theme_id);
    themes_apply(&app);
    assert(!app.themes.open && !strcmp(app.theme_id, "alpha"));
    assert(!image_job_busy() && themes_preview_visible());
    assert(font_current() == &app.themes.active.font);
    themes_open(&app);
    start(); /* X deliberately reloads, supporting on-disk theme edits. */
    themes_cancel(&app);
    assert(!app.themes.open && !image_job_busy() && !app.themes.ready);
    themes_open(&app);
    themes_move(&app, 1);
    start();
    themes_move(&app, -1); /* Return to the applied theme without another decode. */
    assert(app.themes.ready && app.themes.reuse_active && !image_job_busy());
    themes_cancel(&app);
    write_theme(argv[1], "broken", argv[2]);
    char bad[PATH_CAP];
    assert(path_join(bad, argv[1], "_nds/dsi-frontend/themes/broken/theme.ini"));
    FILE *file = fopen(bad, "w");
    assert(file && fputs("[theme]\nversion=99\n", file) >= 0 && !fclose(file));
    themes_open(&app);
    themes_move(&app, 2); /* Alpha -> malformed package; selection does no I/O. */
    themes_apply(&app);
    assert(!app.themes.pending && !image_job_busy());
    themes_preview(&app);
    settle();
    assert(app.themes.open && !app.themes.ready && !strcmp(app.theme_id, "alpha"));
    assert(!image_job_busy() && themes_preview_visible());
    themes_cancel(&app);
    themes_open(&app);
    themes_move(&app, 1);
    start();
    storage_observe(&app, false);
    assert(app.storage_lost && !app.themes.open && !image_job_busy());
    assert(font_current() == &app.themes.active.font);
    puts("PASS: selection without decode, explicit/repeated X, cooperative task ownership, "
         "ready-only apply, superseded PNG cleanup, cancel, active reuse, invalid packages and SD "
         "loss");
}
