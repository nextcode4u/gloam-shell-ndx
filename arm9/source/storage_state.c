#include "storage_state.h"
#include "app.h"
#include "artwork.h"
#include "artwork_disk_cache.h"
#include "audio.h"
#include "browser.h"
#include "icons.h"
#include "installed_titles.h"
#include "picker.h"
#include "startup.h"
#include "themes.h"

void storage_observe(App *app, bool present)
{
    app->storage_card_present = present;
    if (present || !app->storage_ready)
        return;
    browser_scan_cancel(app);
    installed_catalog_cancel(&app->installed_catalog);
    app->installed_discovery_requested = false;
    startup_cancel(app);
    app->storage_lost = true;
    app->storage_ready = false;
    app->launch_requested = false;
    app->system_menu_requested = false;
    app->system_title_requested = -1;
    app->system_discovery_requested = false;
    app->slot1_requested = false;
    if (app->picker.open)
        picker_close(app);
    if (app->themes.open)
        themes_cancel(app);
    app->rail_focus = false;
    app->dashboard.open = false;
    app->association_editor.open = false;
    /* Retain unsaved editor values in memory; no save/close path runs here. */
    app->settings.open = false;
    app->panel = PANEL_NONE;
    app_reset_preview(app);
    icons_reset(app);
    artwork_cache_clear();
    artwork_disk_cache_configure(NULL, false);
    audio_request_reload(app);
}
