/* Dashboard preferences and a bounded journal of requests, not invented playtime. */
#include "dashboard.h"
#include "browser.h"
#include "paths.h"
#include "preferences.h"
#include "system_routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const int section_capacity[] = {5, TAB_CAP - CATEGORY_COUNT, FAVORITE_CAP, RECENT_CAP};

static bool replace_file(const char *current, const char *temporary, const char *backup)
{
    if (path_commit_file(temporary, current, backup) != FILE_COMMIT_OK)
        return false;
    return true;
}
static bool paths(const App *app, const char *stem, char current[PATH_CAP],
                  char temporary[PATH_CAP], char backup[PATH_CAP])
{
    char name[64];
    snprintf(name, sizeof name, "%s.ini", stem);
    if (!path_preferences_file(current, app->root, name))
        return false;
    snprintf(name, sizeof name, "%s.tmp", stem);
    if (!path_preferences_file(temporary, app->root, name))
        return false;
    snprintf(name, sizeof name, "%s.bak", stem);
    return path_preferences_file(backup, app->root, name);
}
static bool save_config(App *app)
{
    char current[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP];
    if (!app->storage_ready || !path_prepare_preferences_directory(app->root) ||
        !paths(app, "dashboard", current, temporary, backup))
        return false;
    FILE *file = fopen(temporary, "w");
    if (!file)
        return false;
    DashboardConfig *config = &app->dashboard.config;
    fprintf(file, "version=1\norder=%d,%d,%d,%d\nvisible=%d,%d,%d,%d\n", config->order[0],
            config->order[1], config->order[2], config->order[3], config->enabled[0],
            config->enabled[1], config->enabled[2], config->enabled[3]);
    fprintf(file, "limits=%u,%u,%u,%u\n", config->limit[0], config->limit[1], config->limit[2],
            config->limit[3]);
    bool written = !ferror(file);
    if (fclose(file))
        written = false;
    return written && replace_file(current, temporary, backup);
}
static bool read_config(App *app, const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    DashboardConfig candidate = app->dashboard.config;
    char line[128];
    bool version = false, valid = true;
    int lines = 0;
    while (fgets(line, sizeof line, file)) {
        if (++lines > 16 || (!strchr(line, '\n') && !feof(file))) {
            valid = false;
            break;
        }
        line[strcspn(line, "\r\n")] = 0;
        int values[4];
        if (!strncmp(line, "version=", 8)) {
            version = !strcmp(line, "version=1");
            valid &= version;
        } else if (sscanf(line, "order=%d,%d,%d,%d", &values[0], &values[1], &values[2],
                          &values[3]) == 4) {
            unsigned seen = 0;
            for (int i = 0; i < 4; i++) {
                if (values[i] < 0 || values[i] >= 4 || (seen & (1u << values[i]))) {
                    valid = false;
                    break;
                }
                seen |= 1u << values[i];
                candidate.order[i] = values[i];
            }
        } else if (sscanf(line, "limits=%d,%d,%d,%d", &values[0], &values[1], &values[2],
                          &values[3]) == 4) {
            for (int i = 0; i < DASH_SECTION_COUNT; i++) {
                if (values[i] < 0 || values[i] > section_capacity[i])
                    valid = false;
                else
                    candidate.limit[i] = (unsigned char)values[i];
            }
        } else if (sscanf(line, "visible=%d,%d,%d,%d", &values[0], &values[1], &values[2],
                          &values[3]) == 4) {
            for (int i = 0; i < 4; i++) {
                if (values[i] != 0 && values[i] != 1)
                    valid = false;
                candidate.enabled[i] = values[i] == 1;
            }
        }
    }
    valid = valid && version && !ferror(file);
    fclose(file);
    if (valid)
        app->dashboard.config = candidate;
    return valid;
}
static bool read_recent(App *app, const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    app->dashboard.recent_count = 0;
    char line[PATH_CAP + 64];
    bool version = false, valid = true;
    size_t bytes = 0;
    while (fgets(line, sizeof line, file)) {
        bytes += strlen(line);
        if (bytes > 16384 || (!strchr(line, '\n') && !feof(file))) {
            valid = false;
            break;
        }
        line[strcspn(line, "\r\n")] = 0;
        if (!strcmp(line, "version=1")) {
            version = true;
            continue;
        }
        unsigned timestamp;
        int offset = 0;
        if (app->dashboard.recent_count < RECENT_CAP &&
            sscanf(line, "request=%u,%n", &timestamp, &offset) == 1 && offset > 0) {
            RecentRequest *request = &app->dashboard.recent[app->dashboard.recent_count];
            if (path_resolve_sd(request->path, app->root, line + offset)) {
                request->timestamp = timestamp;
                app->dashboard.recent_count++;
            }
        }
    }
    valid = valid && version && !ferror(file);
    fclose(file);
    if (!valid)
        app->dashboard.recent_count = 0;
    return valid;
}
void dashboard_load(App *app)
{
    memset(&app->dashboard, 0, sizeof app->dashboard);
    for (int i = 0; i < 4; i++) {
        app->dashboard.config.order[i] = i;
        app->dashboard.config.enabled[i] = true;
    }
    if (!app->storage_ready)
        return;
    char current[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP];
    if (paths(app, "dashboard", current, temporary, backup) && !read_config(app, current))
        read_config(app, backup);
    if (paths(app, "recent", current, temporary, backup) && !read_recent(app, current))
        read_recent(app, backup);
}
static bool save_recent(App *app)
{
    char current[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP];
    if (!app->storage_ready || !path_prepare_preferences_directory(app->root) ||
        !paths(app, "recent", current, temporary, backup))
        return false;
    FILE *file = fopen(temporary, "w");
    if (!file)
        return false;
    fputs("version=1\n", file);
    for (int i = 0; i < app->dashboard.recent_count; i++) {
        const RecentRequest *request = &app->dashboard.recent[i];
        const char *relative = request->path + strlen(app->root);
        while (*relative == '/')
            relative++;
        fprintf(file, "request=%u,sd:/%s\n", (unsigned)request->timestamp, relative);
    }
    bool written = !ferror(file);
    if (fclose(file))
        written = false;
    return written && replace_file(current, temporary, backup);
}
bool dashboard_record_request(App *app, const char *path)
{
    if (!app->dashboard.config.enabled[DASH_RECENT])
        return true;
    if (!path_is_within_root(path, app->root))
        return false;
    char canonical[PATH_CAP];
    const char *relative = path + strlen(app->root);
    while (*relative == '/')
        relative++;
    if (!path_resolve_sd(canonical, app->root, relative))
        return false;
    int position = app->dashboard.recent_count;
    for (int i = 0; i < app->dashboard.recent_count; i++)
        if (!strcmp(app->dashboard.recent[i].path, canonical)) {
            position = i;
            break;
        }
    if (position >= RECENT_CAP)
        position = RECENT_CAP - 1;
    memmove(app->dashboard.recent + 1, app->dashboard.recent, position * sizeof(RecentRequest));
    path_copy(app->dashboard.recent[0].path, canonical, PATH_CAP);
    time_t now = time(NULL);
    app->dashboard.recent[0].timestamp = now > 0 ? (uint32_t)now : 0;
    if (app->dashboard.recent_count < RECENT_CAP && position == app->dashboard.recent_count)
        app->dashboard.recent_count++;
    return save_recent(app);
}
bool dashboard_clear_recent(App *app)
{
    int count = app->dashboard.recent_count;
    app->dashboard.recent_count = 0;
    if (save_recent(app)) {
        browser_refresh(app);
        path_copy(app->dashboard.status, "Recent requests cleared", sizeof app->dashboard.status);
        return true;
    }
    app->dashboard.recent_count = count;
    path_copy(app->dashboard.status, "Could not clear recent requests",
              sizeof app->dashboard.status);
    return false;
}
void dashboard_open(App *app)
{
    app->dashboard.open = true;
    app->dashboard.selected = 0;
    app->dashboard.status[0] = 0;
}
void dashboard_move(App *app, int direction)
{
    app->dashboard.selected = (app->dashboard.selected + direction + 4) % 4;
}
static void finish_edit(App *app, DashboardConfig previous)
{
    if (!save_config(app)) {
        app->dashboard.config = previous;
        path_copy(app->dashboard.status, "Save failed; previous layout retained",
                  sizeof app->dashboard.status);
    } else {
        browser_refresh(app);
        path_copy(app->dashboard.status, "Dashboard saved", sizeof app->dashboard.status);
    }
}
void dashboard_edit(App *app, int direction)
{
    int from = app->dashboard.selected, to = from + (direction < 0 ? -1 : 1);
    if (to < 0 || to >= 4)
        return;
    DashboardConfig previous = app->dashboard.config;
    DashboardSection value = app->dashboard.config.order[to];
    app->dashboard.config.order[to] = app->dashboard.config.order[from];
    app->dashboard.config.order[from] = value;
    app->dashboard.selected = to;
    finish_edit(app, previous);
}
void dashboard_toggle(App *app)
{
    DashboardConfig previous = app->dashboard.config;
    DashboardSection section = app->dashboard.config.order[app->dashboard.selected];
    app->dashboard.config.enabled[section] = !app->dashboard.config.enabled[section];
    finish_edit(app, previous);
}
void dashboard_limit(App *app, int direction)
{
    DashboardConfig previous = app->dashboard.config;
    DashboardSection section = app->dashboard.config.order[app->dashboard.selected];
    int value = app->dashboard.config.limit[section];
    value =
        (value + (direction < 0 ? section_capacity[section] : 1)) % (section_capacity[section] + 1);
    app->dashboard.config.limit[section] = (unsigned char)value;
    finish_edit(app, previous);
}
void dashboard_touch(App *app, int x, int y)
{
    if (x < 6 || x >= 250 || y < 41 || y >= 113)
        return;
    int selected = (y - 41) / 18;
    if (selected == app->dashboard.selected)
        dashboard_toggle(app);
    else
        app->dashboard.selected = selected;
}

/* Repoint/remove only our references. Save and game data retain their identities.
 * Both preference files are restored if either write reports a failure. */
bool dashboard_relink(App *app, const char *old_path, const char *new_path)
{
    if (!app->storage_ready || !old_path ||
        (!path_is_within_root(old_path, app->root) &&
         !(system_destination(old_path) >= 0 && new_path == NULL)))
        return false;
    struct stat status;
    if (new_path && (!path_is_within_root(new_path, app->root) || stat(new_path, &status) ||
                     (!S_ISREG(status.st_mode) && !S_ISDIR(status.st_mode))))
        return false;
    struct ShortcutBackup {
        char favorites[FAVORITE_CAP][PATH_CAP];
        RecentRequest recent[RECENT_CAP];
        int favorite_count, recent_count;
    } *backup = malloc(sizeof *backup);
    if (!backup)
        return false;
    memcpy(backup->favorites, app->favorites, sizeof backup->favorites);
    memcpy(backup->recent, app->dashboard.recent, sizeof backup->recent);
    backup->favorite_count = app->favorite_count;
    backup->recent_count = app->dashboard.recent_count;
    bool changed = false;
    for (int i = 0; i < app->favorite_count; i++) {
        if (strcmp(app->favorites[i], old_path))
            continue;
        changed = true;
        if (new_path)
            path_copy(app->favorites[i], new_path, PATH_CAP);
        else {
            memmove(app->favorites + i, app->favorites + i + 1,
                    (--app->favorite_count - i) * sizeof app->favorites[0]);
            i--;
        }
    }
    for (int i = 0; i < app->dashboard.recent_count; i++) {
        if (strcmp(app->dashboard.recent[i].path, old_path))
            continue;
        changed = true;
        if (new_path)
            path_copy(app->dashboard.recent[i].path, new_path, PATH_CAP);
        else {
            memmove(app->dashboard.recent + i, app->dashboard.recent + i + 1,
                    (--app->dashboard.recent_count - i) * sizeof app->dashboard.recent[0]);
            i--;
        }
    }
    bool saved = changed && app_save(app) && save_recent(app);
    if (!saved) {
        memcpy(app->favorites, backup->favorites, sizeof backup->favorites);
        memcpy(app->dashboard.recent, backup->recent, sizeof backup->recent);
        app->favorite_count = backup->favorite_count;
        app->dashboard.recent_count = backup->recent_count;
        if (changed) {
            app_save(app);
            save_recent(app);
        }
    }
    free(backup);
    return saved;
}
