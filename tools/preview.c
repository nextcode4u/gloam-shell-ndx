#include "app.h"
#include "artwork.h"
#include "browser.h"
#include "paths.h"
#include "preferences.h"
#include "renderer.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: preview <SD folder> <output prefix> [category] [quick|settings]\n");
        return 1;
    }
    app_init(&app, argv[1], true, true);
    if (argc > 3)
        app_category(&app, atoi(argv[3]));
    if (argc > 4 && !strcmp(argv[4], "settings")) {
        settings_open(&app);
    } else if (argc > 4)
        app_panel(&app, PANEL_QUICK);
    for (int frame = 0; frame < 20; frame++)
        app_tick(&app);
    app.panel_age = 20;
    artwork_load(&app);
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
