/* Directory enumeration and built-in dashboard/system entries. */
#include "browser.h"
#include "app.h"
#include "artwork.h"
#include "associations.h"
#include "directory_scan.h"
#include "entry_sort.h"
#include "file_types.h"
#include "icons.h"
#include "installed_titles.h"
#include "layout.h"
#include "paths.h"
#include "system_routes.h"
#include "tabs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

const char *entry_type(const Entry *entry)
{
    return entry_file_type(entry).label;
}

const Entry *app_selected(const App *app)
{
    int i = app->locations[app->category].selected;
    return i >= 0 && i < app->entry_count ? &app->entries[i] : NULL;
}

static int directory_order(const Entry *a, const Entry *b)
{
    return (a->kind == ITEM_DIRECTORY) == (b->kind == ITEM_DIRECTORY) ? 0
           : a->kind == ITEM_DIRECTORY                                ? -1
                                                                      : 1;
}
static int compare_names(const void *left, const void *right)
{
    const Entry *a = left, *b = right;
    int folders = directory_order(a, b);
    return folders ? folders : strcasecmp(a->name, b->name);
}
static int compare_names_desc(const void *left, const void *right)
{
    const Entry *a = left, *b = right;
    int folders = directory_order(a, b);
    return folders ? folders : strcasecmp(b->name, a->name);
}
static int compare_types(const void *left, const void *right)
{
    const Entry *a = left, *b = right;
    int folders = directory_order(a, b);
    if (folders)
        return folders;
    int type = a->kind == ITEM_DIRECTORY
                   ? 0
                   : strcasecmp(path_extension(a->name), path_extension(b->name));
    return type ? type : strcasecmp(a->name, b->name);
}
static int compare_modified(const void *left, const void *right)
{
    const Entry *a = left, *b = right;
    int folders = directory_order(a, b);
    if (folders)
        return folders;
    if (a->modified != b->modified)
        return a->modified > b->modified ? -1 : 1;
    return strcasecmp(a->name, b->name);
}
static bool recognized_file(const App *app, const char *name)
{
    if (icon_is_ds_title(name))
        return true;
    const char *extension = path_extension(name);
    if (association_user(app, extension))
        return true;
    static const char *known[] = {"argv", "mp3", "wav", "mod", "png", "bmp",  "jpg", "jpeg",
                                  "epub", "txt", "pdf", "avi", "fv",  "rvid", "sfc", "smc"};
    for (unsigned i = 0; i < sizeof known / sizeof *known; i++)
        if (!strcasecmp(extension, known[i]))
            return true;
    return association_registered(app, name);
}
void browser_display_name(const App *app, const Entry *entry, char output[256])
{
    if (app->internal_titles && *entry->internal_title) {
        path_copy(output, entry->internal_title, 256);
        return;
    }
    path_copy(output, entry->name, 256);
    if (!app->show_extensions && entry->kind == ITEM_FILE) {
        char *dot = strrchr(output, '.');
        if (dot && dot != output)
            *dot = 0;
    }
}

static void add_browser_entry(App *app, const char *name, const char *path, ItemKind kind,
                              int target)
{
    if (app->entry_count >= ENTRY_CAP) {
        app->truncated = true;
        return;
    }
    Entry *entry = &app->entries[app->entry_count++];
    memset(entry, 0, sizeof *entry);
    path_copy(entry->name, name, sizeof entry->name);
    path_copy(entry->path, path, sizeof entry->path);
    entry->kind = kind;
    entry->target = target;
}

void browser_scan_cancel(App *app)
{
    if (app->scan_directory)
        directory_scan_close(app->scan_directory);
    app->scan_directory = NULL;
    app->scan_count = 0;
    app->scan_restore_path[0] = 0;
}
void browser_scan_begin(App *app)
{
    browser_scan_cancel(app);
    app->entry_count = 0;
    app->scan_visited = app->scan_eligible = 0;
    app->truncated = app->directory_error = false;
    Location *location = &app->locations[app->category];
    const char *root = tabs_browse_root(app, app->category);
    if (!path_is_within_root(location->path, root)) {
        path_copy(location->path, root, PATH_CAP);
        location->selected = location->scroll = 0;
        location->batch = 0;
        app_mark_preferences_dirty(app);
    }
    app->scan_directory = app->storage_ready ? directory_scan_open(location->path) : NULL;
    app->directory_error = !app->scan_directory;
    icons_reset(app);
    app_reset_preview(app);
}
static void finish_scan(App *app)
{
    directory_scan_close(app->scan_directory);
    app->scan_directory = NULL;
    app->entry_count = app->scan_count;
    int (*comparators[])(const void *, const void *) = {compare_names, compare_names_desc,
                                                        compare_types, compare_modified};
    entry_sort(app->entries, app->entry_count, comparators[app->sort_mode]);
    if (*app->scan_restore_path)
        for (int i = 0; i < app->entry_count; i++)
            if (!strcmp(app->entries[i].path, app->scan_restore_path)) {
                app->locations[app->category].selected = i;
                break;
            }
    app->scan_restore_path[0] = 0;
    icons_reset(app);
    browser_reflow(app);
}
void browser_scan_step(App *app)
{
    if (!app->scan_directory)
        return;
    /* Count every directory record, including hidden/unsupported entries.
     * A folder full of rejected records must also yield to input each frame. */
    for (int work = 0; work < 16; work++) {
        DirectoryRecord record;
        if (!directory_scan_read(app->scan_directory, &record)) {
            finish_scan(app);
            return;
        }
        app->scan_visited++;
        if (!strcmp(record.name, ".") || !strcmp(record.name, ".."))
            continue;
        if (!app->show_hidden && record.name[0] == '.')
            continue;
        if (!strcasecmp(record.name, "cover.png") || !strcasecmp(record.name, "cover.jpg") ||
            !strcasecmp(record.name, "cover.jpeg"))
            continue;
        const char *dot = strrchr(record.name, '.');
        if (dot && (!strcasecmp(dot + 1, "png") || !strcasecmp(dot + 1, "jpg") ||
                    !strcasecmp(dot + 1, "jpeg"))) {
            char original[PATH_CAP], base[256];
            snprintf(base, sizeof base, "%.*s", (int)(dot - record.name), record.name);
            struct stat cover_target;
            if (path_join(original, app->locations[app->category].path, base) &&
                !stat(original, &cover_target) && S_ISREG(cover_target.st_mode))
                continue;
        }
        char path[PATH_CAP];
        if (!path_join(path, app->locations[app->category].path, record.name))
            continue;
        if (!record.valid || (!app->show_hidden && record.hidden) ||
            (!app->show_system && record.system))
            continue;
        if (app->hide_save_folders && directory_scan_is_save_folder(&record))
            continue;
        if (app->hide_bios_folders && directory_scan_is_bios_folder(&record))
            continue;
        if (app->filter_recognized && record.regular && !recognized_file(app, record.name))
            continue;
        if (app->scan_eligible++ < app->locations[app->category].batch * ENTRY_CAP)
            continue;
        if (app->scan_count >= ENTRY_CAP) {
            app->truncated = true;
            finish_scan(app);
            return;
        }
        Entry *entry = &app->entries[app->scan_count++];
        memset(entry, 0, sizeof *entry);
        path_copy(entry->name, record.name, sizeof entry->name);
        path_copy(entry->path, path, sizeof entry->path);
        entry->kind = record.directory ? ITEM_DIRECTORY : ITEM_FILE;
        entry->size = record.size;
        entry->modified = record.modified;
    }
}

void app_scan(App *app)
{
    browser_scan_cancel(app);
    app->entry_count = 0;
    app->truncated = false;
    app->directory_error = false;
    if (app->category == CATEGORY_HOME) {
        for (int order = 0; order < DASH_SECTION_COUNT; order++) {
            DashboardSection section = app->dashboard.config.order[order];
            bool visible = app->dashboard.config.enabled[section];
            int limit = app->dashboard.config.limit[section];
            if (section == DASH_LINKS) {
                static const char *names[] = {"Browse games", "Apps & tools", "Media",
                                              "System applications", "Browse SD card"};
                for (int i = 0; visible && i < (limit ? limit : 5); i++)
                    add_browser_entry(app, names[i], "", ITEM_CATEGORY, i + 1);
            } else if (visible && section == DASH_TABS) {
                for (int i = CATEGORY_COUNT;
                     i < app->tab_count && (!limit || i - CATEGORY_COUNT < limit); i++)
                    add_browser_entry(app, tabs_label(app, i), "", ITEM_CATEGORY, i);
            } else if (app->storage_ready && visible &&
                       (section == DASH_PINS || section == DASH_RECENT)) {
                int count =
                    section == DASH_PINS ? app->favorite_count : app->dashboard.recent_count;
                for (int i = 0; i < count && (!limit || i < limit); i++) {
                    const char *path =
                        section == DASH_PINS ? app->favorites[i] : app->dashboard.recent[i].path;
                    int destination = system_destination(path);
                    if (destination >= 0) {
                        add_browser_entry(app, system_destination_name(destination), path,
                                          ITEM_SYSTEM, destination);
                        continue;
                    }
                    struct stat status;
                    bool missing = stat(path, &status) != 0;
                    const char *name = strrchr(path, '/');
                    char label[256];
                    snprintf(label, sizeof label, "%s%.*s",
                             missing                  ? "Missing: "
                             : section == DASH_RECENT ? "Recent: "
                                                      : "",
                             240, name ? name + 1 : path);
                    add_browser_entry(app, label, path,
                                      missing                   ? ITEM_MISSING
                                      : S_ISDIR(status.st_mode) ? ITEM_DIRECTORY
                                                                : ITEM_FILE,
                                      0);
                }
            }
        }
    } else if (app->category == CATEGORY_SYSTEM) {
        if (app->installed_open) {
            if (!app->installed_catalog.attempted)
                app->installed_discovery_requested = true;
            if (app->installed_catalog.ready) {
                for (unsigned i = 0; i < app->installed_catalog.count; i++) {
                    char path[PATH_CAP];
                    if (!installed_title_path(&app->installed_catalog, i, path))
                        continue;
                    InstalledTitle *title = &app->installed_catalog.titles[i];
                    add_browser_entry(app, title->name, path, ITEM_INSTALLED_TITLE, (int)i);
                    app->entries[app->entry_count - 1].size = title->bytes;
                    if (title->title.low == app->installed_selected_id)
                        app->locations[CATEGORY_SYSTEM].selected = (int)i;
                }
            }
        } else {
            if (!app->system_catalog.attempted)
                app->system_discovery_requested = true;
            for (int target = 0; target < 7; target++)
                add_browser_entry(app, system_destination_name(target),
                                  system_destination_uri(target), ITEM_SYSTEM, target);
            add_browser_entry(app, "Installed DSiWare", "system:installed", ITEM_INSTALLED_LIBRARY,
                              7);
        }
    } else if (app->storage_ready) {
        browser_scan_begin(app);
#ifdef HOST_PREVIEW
        /* Existing host consumers request a completed snapshot. Tests can use
         * begin/step explicitly to exercise the same bounded native job. */
        while (app->scan_directory)
            browser_scan_step(app);
#endif
    }
    if (app->scan_directory)
        return;
    icons_reset(app);
    browser_reflow(app);
}

void browser_batch(App *app, int direction)
{
    if (!app->storage_ready || app->category == CATEGORY_HOME || app->category == CATEGORY_SYSTEM ||
        app->scan_directory)
        return;
    Location *location = &app->locations[app->category];
    if (direction < 0) {
        if (!location->batch)
            return;
        location->batch--;
    } else if (app->truncated && location->batch < 4095)
        location->batch++;
    else if (location->batch)
        location->batch = 0;
    else
        return;
    location->selected = location->scroll = 0;
    app_scan(app);
    app_mark_preferences_dirty(app);
}

void browser_reflow(App *app)
{
    Location *location = &app->locations[app->category];
    if (location->selected >= app->entry_count)
        location->selected = app->entry_count ? app->entry_count - 1 : 0;
    if (location->selected < 0)
        location->selected = 0;
    if (layout_is_grid(app)) {
        int visible = layout_visible_rows(app) / layout_grid_columns(app);
        int selected_row = location->selected / layout_grid_columns(app);
        int first_row = location->scroll / layout_grid_columns(app);
        if (selected_row < first_row)
            first_row = selected_row;
        if (selected_row >= first_row + visible)
            first_row = selected_row - visible + 1;
        int maximum =
            (app->entry_count + layout_grid_columns(app) - 1) / layout_grid_columns(app) - visible;
        if (maximum < 0)
            maximum = 0;
        if (first_row > maximum)
            first_row = maximum;
        location->scroll = first_row * layout_grid_columns(app);
        app->selection_y_fixed = (selected_row - first_row) * layout_row_height(app) * 256;
        app_reset_preview(app);
        return;
    }
    if (location->scroll > location->selected)
        location->scroll = location->selected;
    if (location->selected >= location->scroll + layout_visible_rows(app))
        location->scroll = location->selected - layout_visible_rows(app) + 1;
    int last_scroll = app->entry_count - layout_visible_rows(app);
    if (last_scroll < 0)
        last_scroll = 0;
    if (location->scroll > last_scroll)
        location->scroll = last_scroll;
    app->selection_y_fixed = (location->selected - location->scroll) * layout_row_height(app) * 256;
    app_reset_preview(app);
}

void browser_restore_selection(App *app, const char *path)
{
    if (!*path)
        return;
    if (app->scan_directory) {
        path_copy(app->scan_restore_path, path, sizeof app->scan_restore_path);
        return;
    }
    for (int index = 0; index < app->entry_count; index++)
        if (!strcmp(app->entries[index].path, path)) {
            app->locations[app->category].selected = index;
            break;
        }
    browser_reflow(app);
}

void browser_refresh(App *app)
{
    if (app->category == CATEGORY_SYSTEM && app->installed_open) {
        installed_catalog_cancel(&app->installed_catalog);
        app->installed_catalog.attempted = app->installed_catalog.ready = false;
        app->installed_discovery_requested = true;
    }
    artwork_cache_clear();
    char selected_path[PATH_CAP] = "";
    const Entry *entry = app_selected(app);
    if (entry)
        path_copy(selected_path, entry->path, sizeof selected_path);
    app_scan(app);
    browser_restore_selection(app, selected_path);
}
