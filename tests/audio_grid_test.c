#include "app.h"
#include "audio.h"
#include "browser.h"
#include "grid.h"
#include "layout.h"
#include "paths.h"
#include "preferences.h"
#include "renderer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static App app, restored;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} top, bottom;
int main(int argc, char **argv)
{
    assert(argc == 3);
    AudioClip clip = {0};
    assert(audio_read_wav(&clip, argv[2], AUDIO_MUSIC_CAP));
    assert(clip.samples && clip.rate == 16000 && clip.bytes == 128000);
    assert(((uintptr_t)clip.samples & 31) == 0);
    assert(!audio_read_wav(&clip, argv[2], AUDIO_MUSIC_CAP)); /* Never overwrite a live buffer. */
    audio_free_clip(&clip);
    assert(!audio_read_wav(&clip, argv[2], 100));
    unsigned char wav[128044];
    FILE *source = fopen(argv[2], "rb");
    assert(source && fread(wav, 1, sizeof wav, source) == sizeof wav && !fclose(source));
    const int offsets[] = {22, 34, 24, 40, 16, 4};
    const unsigned char values[] = {2, 8, 0, 255, 255, 255};
    char invalid[PATH_CAP];
    assert(path_join(invalid, argv[1], "bad.wav"));
    for (unsigned i = 0; i < sizeof offsets / sizeof offsets[0]; i++) {
        unsigned char previous = wav[offsets[i]];
        wav[offsets[i]] = values[i];
        FILE *file = fopen(invalid, "wb");
        assert(file && fwrite(wav, 1, sizeof wav, file) == sizeof wav && !fclose(file));
        assert(!audio_read_wav(&clip, invalid, AUDIO_MUSIC_CAP));
        assert(!clip.samples);
        wav[offsets[i]] = previous;
    }
    app_init(&app, argv[1], true, true);
    app_category(&app, CATEGORY_GAMES);
    assert(app.entry_count == 17);
    app.browser_style = BROWSER_GRID;
    app.icon_size = ICON_SMALL;
    browser_reflow(&app);
    assert(layout_visible_rows(&app) == 12 && layout_list_bottom(&app) == 161);
    browser_direction(&app, 0, 1);
    assert(app.locations[1].selected == 2);
    browser_direction(&app, 0, -1);
    assert(app.locations[1].selected == 0);
    browser_direction(&app, -1, 0);
    assert(app.locations[1].selected == 16 && app.locations[1].scroll == 6);
    assert(grid_touch_index(&app, 10, 43) == 6);
    assert(grid_touch_index(&app, 135, 43) == 7);
    assert(grid_touch_index(&app, 249, 160) == -1);
    assert(grid_touch_index(&app, 253, 43) == -1);
    app.icon_size = ICON_LARGE;
    browser_reflow(&app);
    assert(layout_visible_rows(&app) == 8 && app.locations[1].scroll % 4 == 0);
    for (int i = 0; i < 120; i++) {
        if (i == 60) {
            app.icon_size = ICON_SMALL;
            browser_reflow(&app);
        }
        browser_direction(&app, i & 1 ? 1 : 0, i & 1 ? 0 : -1);
        assert(app.locations[1].selected >= 0 && app.locations[1].selected < 17);
        assert(app.locations[1].scroll % layout_grid_columns(&app) == 0);
        int visible_index = app.locations[1].selected - app.locations[1].scroll;
        assert(visible_index >= 0 && visible_index < layout_visible_rows(&app));
        int x = 6 + visible_index % layout_grid_columns(&app) * layout_grid_cell_width(&app) + 8;
        int y = layout_list_top(app.category) +
                visible_index / layout_grid_columns(&app) * layout_row_height(&app) + 8;
        assert(grid_touch_index(&app, x, y) == app.locations[1].selected);
        top.before = top.after = bottom.before = bottom.after = 0xCAFE1234;
        render_app(&app, top.pixels, bottom.pixels, "12:00");
        assert(top.before == 0xCAFE1234 && top.after == 0xCAFE1234);
        assert(bottom.before == 0xCAFE1234 && bottom.after == 0xCAFE1234);
    }
    app.browser_style = BROWSER_SHELF;
    app.locations[1].selected = 0;
    browser_reflow(&app);
    assert(layout_visible_rows(&app) == 3);
    assert(grid_touch_index(&app, 249, 50) == -1);
    assert(layout_list_bottom(&app) == UI_LIST_BOTTOM);
    browser_direction(&app, 0, 1);
    assert(app.locations[1].selected == 3 && app.locations[1].scroll == 3);
    browser_direction(&app, -1, 0);
    assert(app.locations[1].selected == 2 && app.locations[1].scroll == 0);
    browser_direction(&app, 0, -1);
    assert(app.locations[1].selected == 16 && app.locations[1].scroll == 15);
    assert(grid_touch_index(&app, 170, 50) == -1); /* Empty last-page cell. */
    for (int i = 0; i < 36; i++) {
        app_tick(&app);
        render_app(&app, top.pixels, bottom.pixels, "12:00");
        assert(top.before == 0xCAFE1234 && top.after == 0xCAFE1234);
        assert(bottom.before == 0xCAFE1234 && bottom.after == 0xCAFE1234);
    }
    assert(app_save(&app));
    app_init(&restored, argv[1], true, true);
    assert(restored.browser_style == BROWSER_SHELF);
    app.browser_style = BROWSER_GRID;
    app.audio.mode = MUSIC_CUSTOM;
    app.audio.music_volume = 45;
    char path[PATH_CAP];
    assert(path_join(path, argv[1], "media/music/loop.wav"));
    path_copy(app.audio.custom_path, path, PATH_CAP);
    audio_reload(&app);
    assert(app.audio.music.samples && !*app.audio.status);
    audio_event(&app, AUDIO_CONFIRM);
    assert(app.audio.event == AUDIO_CONFIRM);
    assert(app_save(&app));
    audio_free_clip(&app.audio.music);
    app_init(&restored, argv[1], true, true);
    assert(restored.browser_style == BROWSER_GRID && restored.audio.mode == MUSIC_CUSTOM);
    assert(restored.audio.music_volume == 45 && !strcmp(restored.audio.custom_path, path));
    restored.recovery_mode = true;
    audio_reload(&restored);
    assert(!restored.audio.music.samples && *restored.audio.status);
    audio_event(&restored, AUDIO_CURSOR);
    assert(restored.audio.event == AUDIO_NONE);
    puts("PASS: bounded PCM validation/alignment, audio persistence/recovery, grid "
         "navigation/touch/render bounds");
}
