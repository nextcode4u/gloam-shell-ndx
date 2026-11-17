/* Read-only resolver/preflight; never commits a launch or creates saves. */
#include "associations.h"
#include "icons.h"
#include "inspection.h"
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
    bool personal = item_preferences_load(app->root, path, &item) && *item.application;
    const Association *profile = personal ? NULL : association_find(app, path);
    const Association *custom = association_user(app, path_extension(path));
    const Association *choices[ASSOCIATION_CAP];
    int count = association_choices(app, path, choices);
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
    else if (!*details->application && *plan->application)
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
             plan->arguments.count > 1 || personal ||
                     (profile && (profile->use_argv || profile->hbargv))
                 ? " + argv"
                 : "");
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
    free(plan);
    script_fonts_prepare_text(details->application);
    script_fonts_prepare_text(details->rule);
    script_fonts_prepare_text(details->save);
    script_fonts_prepare_text(details->private_save);
    app->inspection = details;
}
