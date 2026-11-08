#include "app.h"
#include "artwork.h"
#include "audio.h"
#include "browser.h"
#include "game_options.h"
#include "preferences.h"
#include "renderer.h"
#include "storage_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static App app;
static uint16_t top[256 * 192], bottom[256 * 192];
int main(int argc, char **argv)
{
    assert(argc == 2);
    app_init(&app, argv[1], true, true);
    storage_observe(&app, true);
    assert(app.storage_ready && !app.storage_lost);
    app_category(&app, CATEGORY_GAMES);
    app.preferences_dirty = true;
    app.launch_requested = app.system_menu_requested = true;
    app.settings.open = true;
    app.game_editor.open = true;
    app.game_editor.options.values[GAME_CPU] = 1;
    app.audio.reload_requested = false;
    storage_observe(&app, false);
    assert(app.storage_lost && !app.storage_ready && !app.storage_card_present);
    assert(!app.launch_requested && !app.system_menu_requested && !app.settings.open);
    assert(app.preferences_dirty && app.game_editor.options.values[GAME_CPU] == 1);
    assert(app.audio.reload_requested && !app.artwork.ready);
    unsigned reads = app.icon_reads;
    for (int i = 0; i < 120; i++) {
        app_tick(&app);
        render_app(&app, top, bottom, "12:00");
    }
    assert(app.icon_reads == reads);
    assert(!app_save(&app));
    storage_observe(&app, true);
    assert(app.storage_lost && !app.storage_ready && app.storage_card_present);
    audio_reload(&app);
    assert(!app.audio.music.samples);
    /* Home links remain usable in an initially unmounted model without stat'ing pins. */
    app_init(&app, argv[1], false, true);
    storage_observe(&app, false);
    assert(!app.storage_lost && !app.storage_ready);
    puts("PASS: latched SD-loss state, pending launch cancellation, no-save policy, retained "
         "unsaved values and reinsertion restart requirement");
}
