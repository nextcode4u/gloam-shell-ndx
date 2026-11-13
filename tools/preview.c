#include "app.h"
#include "artwork.h"
#include "browser.h"
#include "paths.h"
#include "preferences.h"
#include "renderer.h"
#include "settings.h"
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

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: preview <SD folder> <output prefix> [category] [quick|settings]\n");
        return 1;
    }
    bool profile = argc > 4 && !strcmp(argv[4], "profile");
    double started = milliseconds();
    app_init(&app, argv[1], true, true);
    if (argc > 3)
        app_category(&app, atoi(argv[3]));
    if (argc > 4 && !strcmp(argv[4], "settings")) {
        settings_open(&app);
    } else if (argc > 4 && !profile)
        app_panel(&app, PANEL_QUICK);
    for (int frame = 0; frame < 20; frame++)
        app_tick(&app);
    app.panel_age = 20;
    artwork_load(&app);
    if (profile) {
        unsigned cycles = argc > 5 ? (unsigned)strtoul(argv[5], NULL, 10) : 0;
        if (cycles > 1000)
            return 2;
        size_t heap_warm = 0, heap_end = 0, heap_peak = 0;
        for (unsigned cycle = 0; cycle < cycles; cycle++) {
            themes_open(&app);
            themes_move(&app, (int)(cycle % (unsigned)app.themes.count) - app.themes.selected);
            themes_apply(&app);
            for (unsigned step = 0; step < 32; step++) {
                if (app.entry_count)
                    app_move(&app, (int)(((cycle * 32 + step) * 17) % (unsigned)app.entry_count) -
                                       app.locations[app.category].selected);
                for (unsigned frame = 0; frame < 8; frame++)
                    app_tick(&app);
                render_app(&app, top, bottom, "22:30");
            }
#ifdef __GLIBC__
            heap_end = mallinfo2().uordblks;
            if (cycle == 11)
                heap_warm = heap_end;
            if (heap_end > heap_peak)
                heap_peak = heap_end;
#endif
        }
        double ready = milliseconds(), slowest = 0;
        for (int frame = 0; frame < 300; frame++) {
            double begin = milliseconds();
            render_app(&app, top, bottom, "22:30");
            double elapsed = milliseconds() - begin;
            if (elapsed > slowest)
                slowest = elapsed;
            app.frame_count++;
        }
        printf("{\"prepare_ms\":%.3f,\"render_mean_ms\":%.4f,\"render_max_ms\":%.4f,"
               "\"app_state_bytes\":%zu,\"framebuffer_bytes\":%zu,\"visible_batch_items\":%d,"
               "\"cycles\":%u,\"heap_warm_bytes\":%zu,"
               "\"heap_end_bytes\":%zu,\"sampled_heap_peak_bytes\":%zu}\n",
               ready - started, (milliseconds() - ready) / 300, slowest, sizeof app,
               sizeof top + sizeof bottom, app.entry_count, cycles, heap_warm, heap_end, heap_peak);
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
    return 0;
}
