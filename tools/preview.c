#include "app.h"
#include "artwork.h"
#include "audio.h"
#include "browser.h"
#include "inspection.h"
#include "library_search.h"
#include "localization.h"
#include "paths.h"
#include "picker.h"
#include "preferences.h"
#include "renderer.h"
#include "script_fonts.h"
#include "settings.h"
#include "theme_motion.h"
#include "themes.h"
#ifdef __GLIBC__
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static App app;
static uint16_t top[256 * 192], bottom[256 * 192];
static void render_write_ppm(const char *path, const uint16_t *pixels, int height)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n256 %d\n255\n", height);
    for (int i = 0; i < 256 * height; i++) {
        uint16_t p = pixels[i];
        unsigned char c[3] = {(p & 31) * 255 / 31, ((p >> 5) & 31) * 255 / 31,
                              ((p >> 10) & 31) * 255 / 31};
        fwrite(c, 1, 3, f);
    }
    fclose(f);
}

static double milliseconds(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000.0 + time.tv_nsec / 1000000.0;
}

static size_t heap_bytes(void)
{
#ifdef __GLIBC__
    struct mallinfo2 info = mallinfo2();
    return info.uordblks + info.hblkhd;
#else
    return 0;
#endif
}

static unsigned common_period(unsigned left, unsigned right)
{
    unsigned a = left, b = right;
    while (b) {
        unsigned remainder = a % b;
        a = b;
        b = remainder;
    }
    return left / a * right;
}
int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr,
                "usage: preview <SD folder> <output prefix> [category] [quick|settings|picker]\n");
        return 1;
    }
    bool profile = argc > 4 && !strcmp(argv[4], "profile");
    bool picker_preview = argc > 4 && !strcmp(argv[4], "picker");
    double started = milliseconds();
    app_init(&app, argv[1], true, true);
    if (argc > 6 && atoi(argv[6]) >= 0 && atoi(argv[6]) < TOP_CONTENT_COUNT)
        app.top_content = atoi(argv[6]);
    if (argc > 3)
        app_category(&app, atoi(argv[3]));
    if (argc > 4 && !strcmp(argv[4], "settings")) {
        settings_open(&app);
    } else if (argc > 4 && (!strcmp(argv[4], "search") || !strcmp(argv[4], "search-edit"))) {
        if (!library_search_open(&app))
            return 2;
        if (!strcmp(argv[4], "search")) {
            if (argc > 5)
                path_copy(app.search->query, argv[5], SEARCH_QUERY_CAP);
            library_search_start(&app);
            for (int frame = 0; app.search->pending && frame < 50000; frame++)
                app_tick(&app);
        }
    } else if (argc > 4 && !strcmp(argv[4], "inspect")) {
        for (int i = 0; i < app.entry_count; i++)
            if (app.entries[i].kind == ITEM_FILE) {
                app.locations[app.category].selected = i;
                break;
            }
        app_panel(&app, PANEL_INFO);
        app_panel_adjust(&app, 1);
    } else if (argc > 4 && !profile && !picker_preview)
        app_panel(&app, PANEL_QUICK);
    for (int frame = 0; frame < 20; frame++)
        app_tick(&app);
    if (picker_preview) {
        if (!picker_open(&app, PICKER_COVER) ||
            !path_join(app.picker.path, app.root, "media/artwork"))
            return 2;
        picker_scan_begin(&app);
        for (int frame = 0; frame < 40; frame++)
            app_tick(&app);
        path_copy(app.picker.path, "sd:/media/artwork", sizeof app.picker.path);
    }
    app.panel_age = 20;
    artwork_load(&app);
    if (profile) {
        double ready = milliseconds();
        audio_reload(&app);
        double audio_ready = milliseconds();
        /* Deferred decoration is measured separately from initial readiness. */
        for (int frame = 0; frame < 180; frame++)
            app_tick(&app);
        double deferred_ready = milliseconds();
        size_t theme_heap = heap_bytes();
        unsigned cycles = argc > 5 ? (unsigned)strtoul(argv[5], NULL, 10) : 0;
        if (cycles > 1000)
            return 2;
        size_t heap_warm = 0, heap_end = 0, heap_peak = 0;
        unsigned theme_changes = 0, cancellations = 0, locale_changes = 0;
        /* Compare equal theme, language, label and selection phases, rather than
         * interpreting different retained artwork as a memory leak. */
        themes_open(&app);
        unsigned theme_count = app.themes.count;
        themes_cancel(&app);
        unsigned period = common_period(app.entry_count ? app.entry_count : 1, 32) / 32;
        period = common_period(period, theme_count ? theme_count : 1);
        period = common_period(period, 3);
        period = common_period(period, 2);
        for (unsigned cycle = 0; cycle < cycles; cycle++) {
            themes_open(&app);
            themes_move(&app, (int)(cycle % (unsigned)app.themes.count) - app.themes.selected);
            char previous[THEME_ID_CAP], selected[THEME_ID_CAP];
            path_copy(previous, app.theme_id, sizeof previous);
            path_copy(selected, app.themes.ids[app.themes.selected], sizeof selected);
            themes_preview(&app);
            if (!app.themes.ready) {
                fprintf(stderr, "Profile preview failed: %s\n", app.themes.status);
                return 3;
            }
            if (cycle % 4 == 0) {
                themes_cancel(&app);
                if (strcmp(app.theme_id, previous))
                    return 3;
                cancellations++;
                themes_open(&app);
                for (int i = 0; i < app.themes.count; i++)
                    if (!strcmp(app.themes.ids[i], selected)) {
                        themes_move(&app, i - app.themes.selected);
                        break;
                    }
                themes_preview(&app);
                if (!app.themes.ready)
                    return 3;
            }
            themes_apply(&app);
            if (strcmp(app.theme_id, selected))
                return 3;
            theme_changes += strcmp(previous, app.theme_id) != 0;
            static const char *locales[] = {"en", "ja", "es"};
            path_copy(app.interface_language, locales[cycle % 3], sizeof app.interface_language);
            localization_load(&app);
            app_scan(&app);
            locale_changes++;
            app.show_icon_labels = (cycle % 2) != 0;
            app.audio.mode = MUSIC_THEME;
            audio_reload(&app);
            for (unsigned step = 0; step < 32; step++) {
                if (app.entry_count)
                    app_move(&app, (int)(((cycle * 32 + step) * 17) % (unsigned)app.entry_count) -
                                       app.locations[app.category].selected);
                for (unsigned frame = 0; frame < 8; frame++)
                    app_tick(&app);
                render_app(&app, top, bottom, "22:30");
            }
#ifdef __GLIBC__
            heap_end = heap_bytes();
            if (cycle + 1 == period)
                heap_warm = heap_end;
            if (heap_end > heap_peak)
                heap_peak = heap_end;
#endif
        }
        double render_started = milliseconds(), slowest = 0;
        for (int frame = 0; frame < 300; frame++) {
            double begin = milliseconds();
            render_app(&app, top, bottom, "22:30");
            double elapsed = milliseconds() - begin;
            if (elapsed > slowest)
                slowest = elapsed;
            app.frame_count++;
            themes_motion_tick(&app);
        }
        printf("{\"prepare_ms\":%.3f,\"stress_ms\":%.3f,\"render_mean_ms\":%.4f,\"render_max_ms\":%"
               ".4f,"
               "\"app_state_bytes\":%zu,\"framebuffer_bytes\":%zu,\"visible_batch_items\":%d,"
               "\"audio_load_ms\":%.3f,\"deferred_setup_ms\":%.3f,\"theme_heap_bytes\":%zu,"
               "\"heap_comparison_period\":%u,\"heap_comparable\":%s,\"cycles\":%u,\"theme_"
               "changes\":%u,\"preview_cancellations\":%u,\"locale_changes\":"
               "%u,\"script_font_bytes\":%zu,\"heap_warm_bytes\":%zu,"
               "\"heap_end_bytes\":%zu,\"sampled_heap_peak_bytes\":%zu}\n",
               ready - started, render_started - ready, (milliseconds() - render_started) / 300,
               slowest, sizeof app, sizeof top + sizeof bottom, app.entry_count,
               audio_ready - ready, deferred_ready - audio_ready, theme_heap, period,
               cycles >= 2 * period && cycles % period == 0 ? "true" : "false", cycles,
               theme_changes, cancellations, locale_changes, script_fonts_bytes(), heap_warm,
               heap_end, heap_peak);
        return 0;
    }
    /* Rendering uses native SD paths; host fixture locations are private. */
    Location *location = &app.locations[app.category];
    size_t root_length = strlen(app.root);
    if (!strncmp(location->path, app.root, root_length) &&
        (location->path[root_length] == '/' || !location->path[root_length])) {
        char display[PATH_CAP];
        snprintf(display, sizeof display, "sd:/%s",
                 location->path + root_length + (location->path[root_length] == '/'));
        path_copy(location->path, display, sizeof location->path);
    }
    render_app(&app, top, bottom, "22:30");
    char path[1024];
    snprintf(path, sizeof path, "%s-top.ppm", argv[2]);
    render_write_ppm(path, top, 192);
    snprintf(path, sizeof path, "%s-bottom.ppm", argv[2]);
    render_write_ppm(path, bottom, 192);
    library_search_close(&app);
    inspection_close(&app);
    return 0;
}
