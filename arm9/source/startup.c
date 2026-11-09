/* Optional delayed launch. Recovery/input cancellation always wins; failures
 * return to a usable browser and cannot restart the countdown this session. */
#include "startup.h"
#include "app.h"
#include "diagnostics.h"
#include "launch.h"
#include "picker.h"
#include <stdio.h>

void startup_prepare(App *app)
{
    app->startup_countdown = 0;
    app->startup_cancelled = app->startup_due = false;
    if (app->storage_ready && !app->recovery_mode && app->panel == PANEL_NONE &&
        !app->tab_config_skipped && app->dashboard.config.enabled[DASH_RECENT] &&
        app->dashboard.recent_count &&
        (app->auto_launch_delay == 3 || app->auto_launch_delay == 5 ||
         app->auto_launch_delay == 10))
        app->startup_countdown = (unsigned)app->auto_launch_delay * 60;
}
void startup_cancel(App *app)
{
    app->startup_countdown = 0;
    app->startup_cancelled = true;
    if (app->startup_due)
        app->launch_requested = false;
    app->startup_due = false;
}
void startup_tick(App *app)
{
    if (app->startup_cancelled || !app->startup_countdown)
        return;
    if (!app->storage_ready || app->storage_lost || app->recovery_mode) {
        startup_cancel(app);
        return;
    }
    if (--app->startup_countdown)
        return;
    app->startup_cancelled = true;
    if (picker_open_handlers(app, app->dashboard.recent[0].path))
        return;
    if (launch_prepare(app, app->dashboard.recent[0].path, &app->launch, app->message))
        app->launch_requested = app->startup_due = true;
    else {
        diagnostics_record_launch(app, LAUNCH_REPORT_REJECTED, app->message);
        app_panel(app, PANEL_MESSAGE);
    }
}
