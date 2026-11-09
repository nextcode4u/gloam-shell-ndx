/* Temporary file view. The underlying tab, entries and scroll state are untouched. */
#include "picker.h"
#include "app.h"
#include "artwork.h"
#include "associations.h"
#include "audio.h"
#include "browser.h"
#include "configuration.h"
#include "dashboard.h"
#include "directory_scan.h"
#include "entry_sort.h"
#include "icons.h"
#include "item_preferences.h"
#include "launch.h"
#include "paths.h"
#include "themes.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int compare_entries(const void *left, const void *right)
{
    const Entry *a = left, *b = right;
    if (a->kind != b->kind)
        return a->kind == ITEM_DIRECTORY ? -1 : 1;
    return strcasecmp(a->name, b->name);
}
static void cancel_scan(PickerState *picker)
{
    if (picker->directory)
        directory_scan_close(picker->directory);
    picker->directory = NULL;
}
void picker_scan_begin(App *app)
{
    PickerState *picker = &app->picker;
    cancel_scan(picker);
    picker->count = picker->selected = picker->scroll = picker->idle_frames = 0;
    picker->scan_count = 0;
    picker->visited = picker->eligible = 0;
    picker->restore_path[0] = 0;
    picker->truncated = picker->failed = picker->preview.ready = false;
    picker->status[0] = 0;
    picker->confirmed = false;
    if (!picker->open || !picker->entries)
        return;
    picker->directory = app->storage_ready ? directory_scan_open(picker->path) : NULL;
    picker->failed = !picker->directory;
}
static void finish_scan(App *app)
{
    PickerState *picker = &app->picker;
    cancel_scan(picker);
    picker->count = picker->failed ? 0 : picker->scan_count;
    entry_sort(picker->entries, picker->count, compare_entries);
    if (*picker->restore_path)
        for (int i = 0; i < picker->count; i++)
            if (!strcmp(picker->entries[i].path, picker->restore_path)) {
                picker_move(app, i);
                break;
            }
    picker->restore_path[0] = 0;
}
void picker_scan_step(App *app)
{
    PickerState *picker = &app->picker;
    if (!picker->directory)
        return;
    /* Rejected and hidden records also count toward this frame's work. */
    for (int work = 0; work < 16; work++) {
        errno = 0;
        DirectoryRecord record;
        if (!directory_scan_read(picker->directory, &record)) {
            picker->failed = errno != 0;
            finish_scan(app);
            return;
        }
        picker->visited++;
        if (!strcmp(record.name, ".") || !strcmp(record.name, "..") ||
            (!app->show_hidden && record.name[0] == '.'))
            continue;
        char path[PATH_CAP];
        if (!path_join(path, picker->path, record.name) || !record.valid ||
            (!app->show_hidden && record.hidden) || (!app->show_system && record.system))
            continue;
        if (app->hide_save_folders && directory_scan_is_save_folder(&record))
            continue;
        if (app->hide_bios_folders && directory_scan_is_bios_folder(&record))
            continue;
        bool folder = record.directory;
        bool supported = picker->kind == PICKER_ICON ? !strcasecmp(path_extension(path), "png")
                         : picker->kind == PICKER_CONFIGURATION
                             ? !strcasecmp(path_extension(path), "dsconfig")
                         : picker->kind == PICKER_REPAIR ? true
                         : picker->kind == PICKER_AUDIO  ? !strcasecmp(path_extension(path), "wav")
                         : (picker->kind == PICKER_COVER || picker->kind == PICKER_TOP_BACKGROUND ||
                            picker->kind == PICKER_BOTTOM_BACKGROUND)
                             ? (!strcasecmp(path_extension(path), "png") ||
                                !strcasecmp(path_extension(path), "jpg") ||
                                !strcasecmp(path_extension(path), "jpeg"))
                             : icon_is_ds_title(path);
        if (!folder && (!record.regular || !supported))
            continue;
        if (picker->eligible++ < picker->batch * ENTRY_CAP)
            continue;
        if (picker->scan_count == ENTRY_CAP) {
            picker->truncated = true;
            finish_scan(app);
            return;
        }
        Entry *entry = &picker->entries[picker->scan_count++];
        memset(entry, 0, sizeof *entry);
        path_copy(entry->name, record.name, sizeof entry->name);
        path_copy(entry->path, path, PATH_CAP);
        entry->kind = folder ? ITEM_DIRECTORY : ITEM_FILE;
    }
}
static void refresh(App *app, const char *restore)
{
    picker_scan_begin(app);
    if (restore)
        path_copy(app->picker.restore_path, restore, PATH_CAP);
#ifdef HOST_PREVIEW
    while (app->picker.directory)
        picker_scan_step(app);
#endif
}
void picker_batch(App *app, int direction)
{
    PickerState *picker = &app->picker;
    if (!picker->open || picker->kind == PICKER_HANDLER || picker->directory || !app->storage_ready)
        return;
    if (direction < 0) {
        if (!picker->batch)
            return;
        picker->batch--;
    } else if (picker->truncated && picker->batch < 4095)
        picker->batch++;
    else if (picker->batch)
        picker->batch = 0;
    else
        return;
    refresh(app, NULL);
}
bool picker_open(App *app, PickerKind kind)
{
    const Entry *selected = app_selected(app);
    if (app->picker.open || !app->storage_ready || !selected || !*selected->path ||
        (selected->kind != ITEM_FILE && selected->kind != ITEM_DIRECTORY &&
         !(kind == PICKER_REPAIR && selected->kind == ITEM_MISSING)) ||
        (kind == PICKER_APPLICATION && selected->kind != ITEM_FILE))
        return false;
    Entry *entries = calloc(ENTRY_CAP, sizeof *entries);
    if (!entries)
        return false;
    PickerState *picker = &app->picker;
    memset(picker, 0, sizeof *picker);
    picker->entries = entries;
    picker->open = true;
    picker->kind = kind;
    path_copy(picker->target, selected->path, PATH_CAP);
    path_copy(picker->path, selected->path, PATH_CAP);
    if (kind == PICKER_REPAIR)
        path_copy(picker->path, app->root, PATH_CAP);
    else if (kind == PICKER_APPLICATION)
        path_default_category(picker->path, app->root, CATEGORY_APPS);
    else if (selected->kind == ITEM_FILE) {
        char *slash = strrchr(picker->path, '/');
        if (slash)
            *slash = 0;
    }
    if (!path_is_within_root(picker->path, app->root))
        path_copy(picker->path, app->root, PATH_CAP);
    refresh(app, NULL);
    return true;
}
bool picker_open_handlers(App *app, const char *path)
{
    if (app->picker.open || !app->storage_ready || !app->dsi_mode ||
        !path_is_within_root(path, app->root) || icon_is_ds_title(path) ||
        !strcasecmp(path_extension(path), "argv"))
        return false;
    ItemPreferences item;
    if (item_preferences_load(app->root, path, &item) && *item.application)
        return false;
    const Association *choices[ASSOCIATION_CAP];
    int count = association_choices(app, path, choices);
    if (count < 2)
        return false;
    Entry *entries = calloc((size_t)count, sizeof *entries);
    if (!entries)
        return false;
    PickerState *picker = &app->picker;
    memset(picker, 0, sizeof *picker);
    picker->open = true;
    picker->kind = PICKER_HANDLER;
    picker->entries = entries;
    picker->count = count;
    path_copy(picker->target, path, PATH_CAP);
    for (int i = 0; i < count; i++) {
        Entry *entry = &entries[i];
        const Association *profile = choices[i];
        const char *name = strrchr(profile->application, '/');
        path_copy(entry->name,
                  *profile->name ? profile->name
                  : name         ? name + 1
                                 : profile->application,
                  sizeof entry->name);
        path_copy(entry->path, profile->application, PATH_CAP);
        entry->kind = ITEM_FILE;
        entry->target = (int)(profile - app->associations);
    }
    return true;
}

bool picker_open_extension(App *app, const char *extension)
{
    if (!app->storage_ready || app->picker.open || !association_extension_valid(extension))
        return false;
    Entry *entries = calloc(ENTRY_CAP, sizeof *entries);
    if (!entries)
        return false;
    memset(&app->picker, 0, sizeof app->picker);
    app->picker.open = true;
    app->picker.kind = PICKER_EXTENSION_APPLICATION;
    app->picker.entries = entries;
    path_copy(app->picker.extension, extension, sizeof app->picker.extension);
    snprintf(app->picker.target, PATH_CAP, "All .%s files", extension);
    path_default_category(app->picker.path, app->root, CATEGORY_APPS);
    refresh(app, NULL);
    return true;
}
bool picker_open_configuration(App *app)
{
    if (app->picker.open || !app->storage_ready || app->recovery_mode)
        return false;
    Entry *entries = calloc(ENTRY_CAP, sizeof *entries);
    if (!entries)
        return false;
    memset(&app->picker, 0, sizeof app->picker);
    app->picker.entries = entries;
    app->picker.open = true;
    app->picker.kind = PICKER_CONFIGURATION;
    path_copy(app->picker.target, "Import frontend configuration", PATH_CAP);
    path_preferences_file(app->picker.path, app->root, "configuration");
    refresh(app, NULL);
    return true;
}
bool picker_open_audio(App *app)
{
    if (app->picker.open || !app->storage_ready)
        return false;
    Entry *entries = calloc(ENTRY_CAP, sizeof *entries);
    if (!entries)
        return false;
    memset(&app->picker, 0, sizeof app->picker);
    app->picker.open = true;
    app->picker.kind = PICKER_AUDIO;
    app->picker.entries = entries;
    path_copy(app->picker.target, "Launcher background music", PATH_CAP);
    if (!path_join(app->picker.path, app->root, "media/music")) {
        picker_close(app);
        return false;
    }
    refresh(app, NULL);
    return true;
}
bool picker_open_background(App *app, int screen)
{
    if (app->picker.open || !app->storage_ready || app->recovery_mode || screen < 0 || screen > 1)
        return false;
    Entry *entries = calloc(ENTRY_CAP, sizeof *entries);
    if (!entries)
        return false;
    memset(&app->picker, 0, sizeof app->picker);
    app->picker.entries = entries;
    app->picker.open = true;
    app->picker.kind = screen ? PICKER_BOTTOM_BACKGROUND : PICKER_TOP_BACKGROUND;
    path_copy(app->picker.target, screen ? "Bottom-screen wallpaper" : "Top-screen wallpaper",
              PATH_CAP);
    path_default_category(app->picker.path, app->root, CATEGORY_MEDIA);
    refresh(app, NULL);
    return true;
}
void picker_close(App *app)
{
    cancel_scan(&app->picker);
    free(app->picker.entries);
    memset(&app->picker, 0, sizeof app->picker);
    app_reset_preview(app);
}
void picker_move(App *app, int direction)
{
    PickerState *picker = &app->picker;
    if (!picker->open || !picker->count)
        return;
    int selected = picker->selected + direction;
    if (selected < 0)
        selected = 0;
    if (selected >= picker->count)
        selected = picker->count - 1;
    if (selected == picker->selected)
        return;
    picker->selected = selected;
    picker->confirmed = false;
    if (selected < picker->scroll)
        picker->scroll = selected;
    if (selected >= picker->scroll + PICKER_VISIBLE_ROWS)
        picker->scroll = selected - PICKER_VISIBLE_ROWS + 1;
    picker->idle_frames = 0;
    picker->preview.ready = false;
    picker->status[0] = 0;
}
void picker_back(App *app)
{
    PickerState *picker = &app->picker;
    if (picker->kind == PICKER_HANDLER || strlen(picker->path) <= strlen(app->root)) {
        picker_close(app);
        return;
    }
    char previous[PATH_CAP];
    path_copy(previous, picker->path, PATH_CAP);
    char *slash = strrchr(picker->path, '/');
    if (slash)
        *slash = 0;
    if (!path_is_within_root(picker->path, app->root))
        path_copy(picker->path, app->root, PATH_CAP);
    picker->batch = 0;
    refresh(app, previous);
}
static void message(App *app, const char *text)
{
    if (app->settings.open) {
        path_copy(app->settings.status, text, sizeof app->settings.status);
        return;
    }
    path_copy(app->message, text, sizeof app->message);
    app_panel(app, PANEL_MESSAGE);
}
static void repair_target(App *app, const char *path)
{
    char replacement[PATH_CAP];
    path_copy(replacement, path, sizeof replacement);
    if (!dashboard_relink(app, app->picker.target, replacement)) {
        path_copy(app->picker.status, "Could not save shortcut; target files stay in place",
                  sizeof app->picker.status);
        return;
    }
    picker_close(app);
    browser_refresh(app);
    message(app, "Shortcut updated; files and saves stay in place");
}
void picker_choose_folder(App *app)
{
    if (app->picker.open && app->picker.kind == PICKER_REPAIR)
        repair_target(app, app->picker.path);
}
void picker_activate(App *app)
{
    PickerState *picker = &app->picker;
    if (!picker->open || !picker->count)
        return;
    const Entry *entry = &picker->entries[picker->selected];
    if (picker->kind == PICKER_HANDLER) {
        char error[160];
        if (!launch_prepare_handler(app, picker->target, entry->target, &app->launch, error)) {
            path_copy(picker->status, error, sizeof picker->status);
            return;
        }
        picker_close(app);
        app->launch_requested = true;
        return;
    }
    if (entry->kind == ITEM_DIRECTORY) {
        path_copy(picker->path, entry->path, PATH_CAP);
        picker->batch = 0;
        refresh(app, NULL);
        return;
    }
    if (picker->kind == PICKER_CONFIGURATION) {
        char error[160] = "";
        unsigned count = 0;
        if (!configuration_inspect(entry->path, &count, error)) {
            path_copy(picker->status, error, sizeof picker->status);
            picker->confirmed = false;
            return;
        }
        if (!picker->confirmed) {
            snprintf(picker->status, sizeof picker->status,
                     "%u records checked. A again imports; B/START cancels", count);
            picker->confirmed = true;
            return;
        }
        if (!configuration_import(app->root, entry->path, error)) {
            path_copy(picker->status, error, sizeof picker->status);
            picker->confirmed = false;
            return;
        }
        picker_close(app);
        path_copy(app->message,
                  "Configuration imported. Previous records: configuration/rollback.dsconfig",
                  sizeof app->message);
        app->configuration_reload_requested = true;
        return;
    }
    if (picker->kind == PICKER_REPAIR) {
        repair_target(app, entry->path);
        return;
    }
    if (picker->kind == PICKER_TOP_BACKGROUND || picker->kind == PICKER_BOTTOM_BACKGROUND) {
        int screen = picker->kind == PICKER_BOTTOM_BACKGROUND;
        if (!themes_background_set(app, screen, entry->path)) {
            path_copy(picker->status, "Use PNG/JPEG, max 512x512 and 1 MiB", sizeof picker->status);
            return;
        }
        picker_close(app);
        message(app, "Wallpaper selected; image stays in place");
        return;
    }
    if (picker->kind == PICKER_AUDIO) {
        AudioClip preview = {0};
        if (!audio_read_wav(&preview, entry->path, AUDIO_MUSIC_CAP)) {
            path_copy(picker->status, "Use mono PCM16 WAV, 8-48 kHz, max 1 MiB",
                      sizeof picker->status);
            return;
        }
        audio_free_clip(&preview);
        path_copy(app->audio.custom_path, entry->path, PATH_CAP);
        app->audio.mode = MUSIC_CUSTOM;
        audio_request_reload(app);
        app_mark_preferences_dirty(app);
        picker_close(app);
        message(app, "Custom launcher music selected");
        return;
    }
    ItemPreferences item;
    item_preferences_load(app->root, picker->target, &item);
    if (picker->kind == PICKER_ICON) {
        if (!icon_load_png(entry->path, picker->preview.pixels)) {
            path_copy(picker->status, "Use a 32x32 PNG icon, at most 64 KiB",
                      sizeof picker->status);
            return;
        }
        path_copy(item.icon, entry->path, PATH_CAP);
    } else if (picker->kind == PICKER_COVER) {
        picker->preview.ready = false;
        if (!artwork_decode_image(&picker->preview, entry->path)) {
            path_copy(picker->status, "Image unavailable: max 512x512, 1 MiB",
                      sizeof picker->status);
            return;
        }
        path_copy(item.cover, entry->path, PATH_CAP);
    } else {
        char error[160];
        if (!launch_prepare(app, entry->path, &app->launch, error)) {
            path_copy(picker->status, error, sizeof picker->status);
            return;
        }
        if (app->launch.route != LAUNCH_DIRECT) {
            path_copy(picker->status, "Choose direct homebrew, not a retail game backend",
                      sizeof picker->status);
            return;
        }
        path_copy(item.application, entry->path, PATH_CAP);
    }
    if (picker->kind == PICKER_EXTENSION_APPLICATION) {
        if (!association_set_default(app, picker->extension, entry->path)) {
            path_copy(picker->status, "Save failed or 32 personal defaults reached",
                      sizeof picker->status);
            return;
        }
        picker_close(app);
        icons_reset(app);
        browser_refresh(app);
        path_copy(app->association_editor.status, "Default saved; per-file choices still win",
                  sizeof app->association_editor.status);
        return;
    }
    if (!item_preferences_save(app->root, picker->target, &item)) {
        path_copy(picker->status, "Save failed; previous assignment retained",
                  sizeof picker->status);
        return;
    }
    bool application = picker->kind == PICKER_APPLICATION;
    bool icon = picker->kind == PICKER_ICON;
    picker_close(app);
    if (application || icon)
        icons_reset(app);
    message(app, icon ? "Browser icon assigned; cover art and source image are unchanged."
                 : application
                     ? "Application saved for this file. A opens it with this application."
                     : "Cover assigned. The original image stays in place.");
}
bool picker_clear_assignment(App *app, bool application)
{
    const Entry *entry = app_selected(app);
    if (!entry || !*entry->path || (entry->kind != ITEM_FILE && entry->kind != ITEM_DIRECTORY))
        return false;
    ItemPreferences item;
    item_preferences_load(app->root, entry->path, &item);
    if (application)
        item.application[0] = 0;
    else
        item.cover[0] = 0;
    if (!item_preferences_save(app->root, entry->path, &item))
        return false;
    if (application)
        icons_reset(app);
    app_reset_preview(app);
    return true;
}
bool picker_clear_icon(App *app)
{
    const Entry *entry = app_selected(app);
    if (!app->storage_ready || !entry || !*entry->path)
        return false;
    ItemPreferences item;
    item_preferences_load(app->root, entry->path, &item);
    item.icon[0] = 0;
    if (!item_preferences_save(app->root, entry->path, &item))
        return false;
    icons_reset(app);
    return true;
}

void picker_touch(App *app, int x, int y)
{
    if (y >= 0 && y < 21 && x >= 205 && x < 253) {
        picker_batch(app, x < 229 ? -1 : 1);
        return;
    }
    if (x < 6 || x >= 250 || y < PICKER_TOP || y >= PICKER_TOP + 7 * PICKER_ROW_HEIGHT)
        return;
    int selected = app->picker.scroll + (y - PICKER_TOP) / PICKER_ROW_HEIGHT;
    if (selected >= app->picker.count)
        return;
    if (selected == app->picker.selected)
        picker_activate(app);
    else
        picker_move(app, selected - app->picker.selected);
}
void picker_tick(App *app)
{
    PickerState *picker = &app->picker;
    if (picker->directory) {
        picker_scan_step(app);
        return;
    }
    if (++picker->idle_frames != 7 ||
        (picker->kind != PICKER_COVER && picker->kind != PICKER_ICON &&
         picker->kind != PICKER_TOP_BACKGROUND && picker->kind != PICKER_BOTTOM_BACKGROUND) ||
        !picker->count)
        return;
    const Entry *entry = &picker->entries[picker->selected];
    if (entry->kind == ITEM_FILE && !artwork_decode_image(&picker->preview, entry->path))
        path_copy(picker->status, "Image unavailable: max 512x512, 1 MiB", sizeof picker->status);
}
