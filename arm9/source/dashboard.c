/* Dashboard preferences and a bounded journal of requests, not invented playtime. */
#include "dashboard.h"
#include "browser.h"
#include "paths.h"
#include "preferences.h"
#include "system_routes.h"
#include <errno.h>
#include <limits.h>
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
static bool read_values(const char *text, int values[DASH_SECTION_COUNT])
{
    for (int i = 0; i < DASH_SECTION_COUNT; i++) {
        char *end;
        errno = 0;
        long number = strtol(text, &end, 10);
        if (errno || end == text || number < 0 || number > INT_MAX ||
            *end != (i + 1 == DASH_SECTION_COUNT ? '\0' : ','))
            return false;
        values[i] = (int)number;
        text = end + (i + 1 != DASH_SECTION_COUNT);
    }
    return true;
}
static bool read_config(App *app, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    /* The format allows at most sixteen bounded lines. Validate the complete
     * file first so embedded NULs and short reads cannot hide invalid records. */
    struct stat metadata;
    if (fstat(fileno(file), &metadata) || !S_ISREG(metadata.st_mode) || metadata.st_size < 0 ||
        metadata.st_size > 2048) {
        fclose(file);
        return false;
    }
    char contents[2049];
    size_t size = (size_t)metadata.st_size;
    bool complete = fread(contents, 1, size, file) == size && !ferror(file);
    if (fclose(file))
        complete = false;
    if (!complete || memchr(contents, 0, size))
        return false;
    contents[size] = 0;
    char *cursor = contents;
    DashboardConfig candidate = app->dashboard.config;
    char line[128];
    bool valid = true;
    unsigned seen_fields = 0;
    int lines = 0;
    while (*cursor) {
        char *next = strchr(cursor, '\n');
        size_t length = next ? (size_t)(next - cursor) : strlen(cursor);
        if (++lines > 16 || length >= sizeof line) {
            valid = false;
            break;
        }
        memcpy(line, cursor, length);
        line[length] = 0;
        cursor += length + (next != NULL);
        line[strcspn(line, "\r\n")] = 0;
        int values[DASH_SECTION_COUNT];
        unsigned field;
        if (!strncmp(line, "version=", 8)) {
            field = 1;
            valid = !strcmp(line, "version=1");
        } else if (!strncmp(line, "order=", 6)) {
            field = 2;
            if (!read_values(line + 6, values)) {
                valid = false;
                break;
            }
            unsigned seen = 0;
            for (int i = 0; i < DASH_SECTION_COUNT; i++) {
                if (values[i] >= DASH_SECTION_COUNT || (seen & (1u << values[i]))) {
                    valid = false;
                    break;
                }
                seen |= 1u << values[i];
                candidate.order[i] = values[i];
            }
        } else if (!strncmp(line, "limits=", 7)) {
            field = 8;
            if (!read_values(line + 7, values)) {
                valid = false;
                break;
            }
            for (int i = 0; i < DASH_SECTION_COUNT; i++) {
                if (values[i] > section_capacity[i])
                    valid = false;
                else
                    candidate.limit[i] = (unsigned char)values[i];
            }
        } else if (!strncmp(line, "visible=", 8)) {
            field = 4;
            if (!read_values(line + 8, values)) {
                valid = false;
                break;
            }
            for (int i = 0; i < DASH_SECTION_COUNT; i++) {
                if (values[i] != 0 && values[i] != 1)
                    valid = false;
                candidate.enabled[i] = values[i] == 1;
            }
        } else
            continue; /* Retain compatibility with optional future metadata. */
        if (seen_fields & field)
            valid = false;
        seen_fields |= field;
        if (!valid)
            break;
    }
    /* Version, order and visibility are required; older files may omit limits. */
    valid = valid && (seen_fields & 7) == 7;
    if (valid)
        app->dashboard.config = candidate;
    return valid;
}
static bool read_recent(App *app, const char *path)
{
    app->dashboard.recent_count = 0;
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    struct stat metadata;
    if (fstat(fileno(file), &metadata) || !S_ISREG(metadata.st_mode) || metadata.st_size < 0 ||
        metadata.st_size > 16384) {
        fclose(file);
        return false;
    }
    size_t size = (size_t)metadata.st_size;
    char *contents = malloc(size + 1);
    if (!contents) {
        fclose(file);
        return false;
    }
    bool complete = fread(contents, 1, size, file) == size && !ferror(file);
    if (fclose(file))
        complete = false;
    if (!complete || memchr(contents, 0, size)) {
        free(contents);
        return false;
    }
    contents[size] = 0;
    char *cursor = contents;
    bool version = false, valid = true;
    while (*cursor) {
        char *line = cursor, *next = strchr(cursor, '\n');
        size_t length = next ? (size_t)(next - cursor) : strlen(cursor);
        if (length >= PATH_CAP + 64) {
            valid = false;
            break;
        }
        cursor += length + (next != NULL);
        if (next)
            *next = 0;
        if (length && line[length - 1] == '\r')
            line[length - 1] = 0;
        if (!strncmp(line, "version=", 8)) {
            if (version || strcmp(line, "version=1")) {
                valid = false;
                break;
            }
            version = true;
            continue;
        }
        if (strncmp(line, "request=", 8))
            continue;
        const char *number = line + 8;
        if (*number < '0' || *number > '9') {
            valid = false;
            break;
        }
        char *end;
        errno = 0;
        unsigned long timestamp = strtoul(number, &end, 10);
        char resolved[PATH_CAP];
        if (errno || *end != ',' || timestamp > UINT32_MAX ||
            !path_resolve_sd(resolved, app->root, end + 1)) {
            valid = false;
            break;
        }
        /* Preserve the first bounded set from older, longer history files. */
        if (app->dashboard.recent_count < RECENT_CAP) {
            RecentRequest *request = &app->dashboard.recent[app->dashboard.recent_count++];
            path_copy(request->path, resolved, sizeof request->path);
            request->timestamp = (uint32_t)timestamp;
        }
    }
    valid = valid && version;
    free(contents);
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
