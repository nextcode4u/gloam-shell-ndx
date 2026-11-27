/* Read-only resolver/preflight; never commits a launch or creates saves. */
#include "inspection.h"
#include "associations.h"
#include "browser.h"
#include "icons.h"
#include "item_preferences.h"
#include "launch.h"
#include "paths.h"
#include "script_fonts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void inspection_close(App *app)
{
    free(app->inspection);
    app->inspection = NULL;
}
bool inspection_available(const App *app)
{
    const Entry *entry = app_selected(app);
    return app->storage_ready && !app->storage_lost && entry && entry->kind == ITEM_FILE &&
           path_is_within_root(entry->path, app->root);
}

void inspection_prepare(App *app, const char *path)
{
    inspection_close(app);
    if (!app->storage_ready || app->storage_lost || !path_is_within_root(path, app->root))
        return;
    LaunchExplanation *details = calloc(1, sizeof *details);
    LaunchPlan *plan = calloc(1, sizeof *plan);
    if (!details || !plan) {
        free(details);
        free(plan);
        return;
    }
    ItemPreferences item;
    if (!item_preferences_resolve(app->root, path, &item)) {
        path_copy(details->reason, "Per-file settings unavailable", sizeof details->reason);
        path_copy(details->backend, "Unavailable", sizeof details->backend);
        path_copy(details->save, "Not resolved until saved settings can be read",
                  sizeof details->save);
        path_copy(details->private_save, "Not resolved", sizeof details->private_save);
        path_copy(details->status,
                  "Could not read this file's saved settings or backup. Check SD access or "
                  "restore the record before launching.",
                  sizeof details->status);
        goto finished;
    }
    bool personal = *item.application;
    /* The launcher handles native titles and shortcuts before extension rules. */
    bool built_in = icon_is_ds_title(path) || !strcasecmp(path_extension(path), "argv");
    const Association *profile = personal || built_in ? NULL : association_find(app, path);
    const Association *custom =
        personal || built_in ? NULL : association_user(app, path_extension(path));
    const Association *choices[ASSOCIATION_CAP];
    int count = personal || built_in ? 0 : association_choices(app, path, choices);
    const char *reason = personal ? "Application assigned to this file"
                         : !strcasecmp(path_extension(path), "argv") ? "ARGV shortcut"
                         : icon_is_ds_title(path)     ? "DS/DSi header determines the route"
                         : custom && custom->disabled ? "This extension is disabled"
                         : custom                     ? "Personal default for this extension"
                         : count > 1 ? "Several handlers match; choose one on launch"
                         : !profile  ? "No matching file handler"
                         : association_matches_folder(app, profile, path)
                             ? "Preferred folder matches this extension"
                             : "Only one handler matches this extension";
    path_copy(details->reason, reason, sizeof details->reason);
    path_copy(details->rule,
              profile && !custom && association_matches_folder(app, profile, path)
                  ? profile->content_root
              : profile ? profile->extensions
                        : path_extension(path),
              sizeof details->rule);
    if (personal)
        path_copy(details->application, item.application, PATH_CAP);
    else if (profile)
        path_copy(details->application, profile->application, PATH_CAP);
    else if (icon_is_ds_title(path))
        path_copy(details->application, path, PATH_CAP);
    bool ready = launch_prepare(app, path, plan, details->status);
    if (*plan->target)
        path_copy(details->application, plan->target, PATH_CAP);
    else if (*plan->application)
        path_copy(details->application, plan->application, PATH_CAP);
    const char *backend = plan->route == LAUNCH_DSIWARE        ? "nds-bootstrap / DSiWare"
                          : plan->route == LAUNCH_BOOTSTRAP    ? "nds-bootstrap / retail"
                          : plan->route == LAUNCH_BOOTSTRAP_HB ? "nds-bootstrap / homebrew"
                          : count > 1 && !personal             ? "Choose a handler first"
                          : profile && (profile->bootstrap || profile->hbargv)
                              ? "nds-bootstrap / homebrew"
                          : !*details->application ? "Unavailable"
                                                   : "Direct homebrew loader";
    snprintf(details->backend, sizeof details->backend, "%s%s", backend,
             plan->arguments.count > 1 || *plan->homebrew_argument ? " + argv" : "");
    path_copy(details->save,
              !ready        ? "Not resolved until preflight succeeds"
              : *plan->save ? plan->save
              : plan->route == LAUNCH_DIRECT || plan->route == LAUNCH_BOOTSTRAP_HB
                  ? "Managed by the application; location is not declared to Gloam"
                  : "No save path selected",
              sizeof details->save);
    path_copy(details->private_save,
              !ready                ? "Not resolved"
              : *plan->private_save ? plan->private_save
                                    : "Not used",
              sizeof details->private_save);
    if (ready)
        path_copy(details->status, "Preflight passed. No saves or launch files were created.",
                  sizeof details->status);
    else if (!*details->status)
        path_copy(details->status, "Launch could not be prepared.", sizeof details->status);
finished:
    free(plan);
    script_fonts_prepare_text(details->application);
    script_fonts_prepare_text(details->rule);
    script_fonts_prepare_text(details->save);
    script_fonts_prepare_text(details->private_save);
    app->inspection = details;
}
