#include "renderer.h"
#include "app.h"
#include "artwork.h"
#include "association_editor.h"
#include "associations.h"
#include "browser.h"
#include "cheats.h"
#include "dashboard.h"
#include "game_options.h"
#include "grid.h"
#include "icons.h"
#include "layout.h"
#include "picker.h"
#include "settings.h"
#include "storage_state.h"
#include "tabs.h"
#include "themes.h"

#include "drawing.h"
#include "paths.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef ThemePalette Palette;

static void draw_browser_icon(const App *app, uint16_t *pixels, int index, int x, int y, int size,
                              uint16_t color);

static void draw_library_index(const App *app, uint16_t *pixels, Palette colors, int offset)
{
    int active = browser_index_group(app, app->locations[app->category].selected);
    unsigned folders = app->entry_count ? app->library_index_counts[0] : 0;
    bool selected = active == 0 && folders;
    if (selected)
        draw_rectangle(pixels, offset + 12, 49, 154, 16, colors.accent);
    char label[40];
    snprintf(label, sizeof label, "Folders  %u", folders);
    draw_text(pixels, offset + 15, 51, label,
              selected  ? colors.ink
              : folders ? colors.text
                        : colors.line,
              21);
    for (int group = 1; group < LIBRARY_INDEX_GROUPS; group++) {
        int x = offset + 12 + ((group - 1) % 7) * 22, y = 69 + ((group - 1) / 7) * 18;
        unsigned count = app->entry_count ? app->library_index_counts[group] : 0;
        bool focused = count && group == active;
        if (focused)
            draw_rectangle(pixels, x, y, 20, 16, colors.accent);
        char letter[2] = {group == 1 ? '#' : group == 28 ? '*' : (char)('A' + group - 2), 0};
        draw_text(pixels, x + 6, y + 2, letter,
                  focused ? colors.ink
                  : count ? colors.text
                          : colors.line,
                  1);
    }
    char group_name[16];
    if (active == 0 || active == 1 || active == 28)
        snprintf(group_name, sizeof group_name, "%s",
                 active == 0   ? "Folders"
                 : active == 1 ? "Numbers"
                               : "Other");
    else
        snprintf(group_name, sizeof group_name, "%c", 'A' + active - 2);
    snprintf(label, sizeof label, "%s: %u", group_name,
             app->entry_count ? app->library_index_counts[active] : 0);
    draw_text(pixels, offset + 12, 148, app->scan_directory ? "Reading folder..." : label,
              colors.muted, 22);
    draw_text(pixels, offset + 12, 164,
              app->truncated || app->locations[app->category].batch ? "Current batch only"
                                                                    : "Left/right: groups",
              colors.muted, 22);
}

/* Facts use cached entries/metadata only: no file access in rendering. */
static void draw_item_information(const App *app, uint16_t *pixels, Palette colors, int offset)
{
    const Entry *entry = app_selected(app);
    int index = app->locations[app->category].selected;
    char size[32] = "", extra[48] = "", facts[80] = "";
    const char *type = entry ? entry_type(entry) : "No selection";
    const char *storage = "SD card";
    const char *title = entry ? entry->name : "Choose an item";
    const char *hint = "A: open selection";
    if (entry) {
        draw_browser_icon(app, pixels, index, offset + 12, 53, 32, colors.accent);
        /* Directory context already appears on the bottom screen. */
        if ((entry->kind == ITEM_FILE || entry->kind == ITEM_DIRECTORY) && entry->modified > 0) {
            time_t modified = (time_t)entry->modified;
            struct tm date;
            if ((int64_t)modified == entry->modified && localtime_r(&modified, &date))
                strftime(extra, sizeof extra, "Modified: %Y-%m-%d", &date);
        }
        if (!*extra)
            snprintf(extra, sizeof extra, "Favorite: %s",
                     app_is_favorite(app, entry->path) ? "Yes" : "No");
        if (entry->kind == ITEM_FILE || entry->kind == ITEM_INSTALLED_TITLE) {
            if (entry->size >= 1024 * 1024)
                snprintf(size, sizeof size, "%lu.%lu MiB", (unsigned long)(entry->size / 1048576),
                         (unsigned long)((entry->size % 1048576) * 10ULL / 1048576));
            else if (entry->size >= 1024)
                snprintf(size, sizeof size, "%lu KiB", (unsigned long)(entry->size / 1024));
            else
                snprintf(size, sizeof size, "%lu bytes", (unsigned long)entry->size);
        }
        if (entry->kind == ITEM_FILE) {
            hint = "A: check and launch";
            if (icon_is_ds_title(entry->path)) {
                if (app->info_header_valid) {
                    snprintf(extra, sizeof extra, "ROM revision: %u", app->info_revision);
                    type = app->info_unit == 0   ? "Nintendo DS"
                           : app->info_unit == 2 ? "DSi enhanced"
                           : app->info_unit == 3 ? "DSi only"
                                                 : "Unknown DS header";
                }
                if (*app->info_title)
                    title = app->info_title;
                snprintf(facts, sizeof facts, "%s%s", *app->info_code ? "Game code: " : "",
                         *app->info_code    ? app->info_code
                         : app->info_loaded ? "Code unavailable"
                                            : "Reading metadata...");
            } else {
                const Association *choices[ASSOCIATION_CAP];
                int count = association_choices(app, entry->path, choices);
                if (count == 1) {
                    title = *choices[0]->name ? choices[0]->name : choices[0]->application;
                    snprintf(facts, sizeof facts, "%s / %s",
                             choices[0]->bootstrap || choices[0]->hbargv ? "nds-bootstrap"
                                                                         : "Direct",
                             choices[0]->use_argv || choices[0]->hbargv ? "argv" : "no argv");
                } else
                    snprintf(facts, sizeof facts, "%s",
                             count > 1 ? "Choose app on launch" : "No file association");
            }
        } else if (entry->kind == ITEM_INSTALLED_TITLE && entry->target >= 0 &&
                   (unsigned)entry->target < app->installed_catalog.count) {
            const NativeTitle *native = &app->installed_catalog.titles[entry->target].title;
            storage = "NAND";
            snprintf(facts, sizeof facts, "ID %08lX%08lX", (unsigned long)native->high,
                     (unsigned long)native->low);
            hint = "A: launch from NAND";
        } else if (entry->kind == ITEM_SYSTEM) {
            storage = entry->target == 6 ? "Slot-1" : "NAND";
            hint = entry->target == 5   ? "A: Nintendo Menu"
                   : entry->target == 6 ? "A: launch card"
                                        : "A: launch system app";
            if (entry->target == 6 && *app->slot1.title)
                title = app->slot1.title;
        } else if (entry->kind == ITEM_DIRECTORY || entry->kind == ITEM_CATEGORY) {
            unsigned files = 0, folders = 0;
            for (int i = 0; i < app->entry_count; i++) {
                files += app->entries[i].kind == ITEM_FILE;
                folders += app->entries[i].kind == ITEM_DIRECTORY;
            }
            title = "Current browser batch";
            snprintf(facts, sizeof facts, "%u files / %u folders", files, folders);
        } else if (entry->kind == ITEM_INSTALLED_LIBRARY) {
            storage = "NAND";
            snprintf(facts, sizeof facts, "%u discovered titles", app->installed_catalog.count);
        }
    }
    draw_text_scrolling(pixels, offset + 52, 52, type, colors.accent, 114,
                        app->selection_idle_frames, !app->reduced_motion);
    draw_text(pixels, offset + 52, 68, size, colors.text, 16);
    draw_text(pixels, offset + 52, 84, storage, colors.muted, 16);
    draw_text_scrolling(pixels, offset + 12, 103, title, colors.text, 154,
                        app->selection_idle_frames, !app->reduced_motion);
    draw_text_scrolling(pixels, offset + 12, 117, facts, colors.muted, 154,
                        app->selection_idle_frames, !app->reduced_motion);
    draw_text_scrolling(pixels, offset + 12, 131, extra, colors.muted, 154,
                        app->selection_idle_frames, !app->reduced_motion);
    draw_text(pixels, offset + 12, 146, hint, colors.muted, 22);
    draw_text(pixels, offset + 12, 164,
              app->browser_style == BROWSER_INDEX ? "Y: library index"
              : app->show_covers                  ? "Y: artwork"
                                                  : "Covers disabled",
              colors.accent, 22);
}

static void draw_poster(const App *app, uint16_t *pixels, Palette colors)
{
    const ThemePackage *theme = &app->themes.active;
    const Entry *entry = app_selected(app);
    int selected = app->locations[app->category].selected;
    bool native = entry && (entry->kind == ITEM_SYSTEM || entry->kind == ITEM_INSTALLED_TITLE) &&
                  icons_for_entry(app, selected);
    if (!native && app->artwork.ready) {
        int width = 108, height = app->artwork.height * 108 / app->artwork.width;
        if (height > 104) {
            height = 104;
            width = app->artwork.width * 104 / app->artwork.height;
        }
        if (width < 1)
            width = 1;
        if (height < 1)
            height = 1;
        int x = 10 + (108 - width) / 2, y = 48 + (104 - height) / 2;
        for (int yy = 0; yy < height; yy++)
            for (int xx = 0; xx < width; xx++)
                pixels[(y + yy) * 256 + x + xx] =
                    app->artwork.pixels[(yy * app->artwork.height / height) * app->artwork.width +
                                        xx * app->artwork.width / width];
    } else {
        draw_theme_component(theme, pixels, THEME_CARD, 10, 48, 108, 104);
        if (native)
            draw_browser_icon(app, pixels, selected, 24, 60, 80, colors.accent);
        else
            draw_theme_icon(theme, pixels, 24, 60, 80, entry_file_type(entry).icon, colors.accent);
    }
    draw_text_wrapped(pixels, 130, 53, entry ? entry_type(entry) : "Browse", colors.accent, 16, 2);
    char size[32] = "";
    if (entry && entry->size)
        snprintf(size, sizeof size, "%lu KiB", (unsigned long)(entry->size / 1024));
    draw_text(pixels, 130, 86, size, colors.text, 16);
    draw_text(pixels, 130, 105, app->dsi_mode ? "Nintendo DSi" : "Nintendo DS", colors.muted, 16);
    draw_text(pixels, 130, 131, "Y: details", colors.muted, 16);
    int focus = app->rail_focus ? app->rail_selected : app->category;
    for (int cell = 0; cell < 3; cell++) {
        int index = focus + cell - 1;
        if (index < 0 || index >= app->tab_count)
            continue;
        int x = 7 + cell * 82;
        bool active = index == focus;
        if (active && !draw_theme_component(theme, pixels, THEME_TAB_SELECTED, x, 155, 78, 18))
            draw_rectangle(pixels, x, 155, 78, 18, colors.accent);
        draw_text_scrolling(pixels, x + 4, 158, tabs_label(app, index),
                            active ? themes_tab_text(theme) : colors.muted, 70,
                            app->frame_count - app->tab_text_started_frame,
                            active && !app->reduced_motion);
    }
}

static void draw_top_screen(const App *app, uint16_t *pixels, Palette colors, const char *clock)
{
    const ThemePackage *theme = &app->themes.active;
    int offset = theme->top_layout == 1 ? 78 : 0;
    int rail = theme->top_layout == 1 ? -178 : 0;
    bool poster = theme->top_layout == 2 && app->detail_tab == 0 && app->show_covers &&
                  app->browser_style != BROWSER_INDEX;
    themes_background_draw(&app->themes.active, 0, pixels);
    draw_theme_chrome(&app->themes.active, pixels, 22);
    draw_text_scrolling(pixels, 10, 7, app->recovery_mode ? "RECOVERY" : theme->header,
                        colors.text, 138, app->frame_count, !app->reduced_motion);
    char tab_position[16];
    snprintf(tab_position, sizeof tab_position, "%d/%d", app->category + 1, app->tab_count);
    draw_text(pixels, 205, 7, tab_position, colors.muted, 6);
    if (!theme->minimal_chrome)
        draw_rectangle(pixels, 10, 23, 236, 1, colors.line);
    if (!poster && !theme->minimal_chrome)
        draw_rectangle(pixels, offset ? 78 : 174, 33, 1, 133, colors.line);
    const Entry *entry = app_selected(app);
    const char *name = entry                ? entry->name
                       : app->storage_ready ? "Nothing here yet"
                                            : "SD card unavailable";
    if (entry && entry->kind == ITEM_SYSTEM && entry->target == 6 && app->slot1.icon.ready &&
        *app->slot1.title)
        name = app->slot1.title;
    draw_text_scrolling(pixels, offset + 12, 31, name, colors.text, poster ? 232 : 154,
                        app->selection_idle_frames, !app->reduced_motion);
    if (poster) {
        draw_poster(app, pixels, colors);
    } else if (app->browser_style == BROWSER_INDEX && app->detail_tab == 0) {
        draw_library_index(app, pixels, colors, offset);
    } else if (app->detail_tab == 0 && app->show_covers) {
        int x = offset + 15, y = 53, w = 96, h = 104;
        int selected = app->locations[app->category].selected;
        if (entry && (entry->kind == ITEM_SYSTEM || entry->kind == ITEM_INSTALLED_TITLE) &&
            icons_for_entry(app, selected)) {
            draw_browser_icon(app, pixels, selected, offset + 31, 73, 64, colors.accent);
            draw_text(pixels, offset + 24, 149,
                      entry->kind == ITEM_INSTALLED_TITLE           ? "NAND TITLE"
                      : entry->target == 6 && app->slot1.icon.ready ? "CARD ICON"
                                                                    : "SYSTEM ICON",
                      colors.muted, 14);
        } else if (app->artwork.ready) {
            w = app->artwork.width;
            h = app->artwork.height;
            if (w == 32 && h == 32) {
                w = h = 64;
            }
            x = offset + 15 + (96 - w) / 2;
            y = 53 + (104 - h) / 2;
            draw_rectangle(pixels, x + 3, y + 3, w, h, colors.line);
            for (int yy = 0; yy < h; yy++)
                for (int xx = 0; xx < w; xx++)
                    pixels[(y + yy) * 256 + x + xx] =
                        app->artwork.pixels[(yy * app->artwork.height / h) * app->artwork.width +
                                            xx * app->artwork.width / w];
        } else {
            if (!draw_theme_component(&app->themes.active, pixels, THEME_CARD, x, y, w, h)) {
                draw_rectangle(pixels, x + 3, y + 3, w, h, colors.line);
                draw_rectangle(pixels, x, y, w, h, colors.surface);
                draw_rectangle(pixels, x, y, 3, h, colors.accent);
            }
            EntryFileType type = entry_file_type(entry);
            draw_theme_icon(&app->themes.active, pixels, x + (w - 48) / 2, y + 12, 48, type.icon,
                            colors.accent);
            const char *label =
                app->show_covers && !app->artwork_resolved && app->selection_idle_frames >= 7
                    ? "LOADING"
                    : placeholder_label(type.icon);
            draw_text(pixels, x + (w - (int)strlen(label) * 7) / 2, y + 67, label, colors.text, 11);
            char suffix[9] = "";
            if (entry && entry->kind == ITEM_FILE) {
                const char *extension = path_extension(entry->name);
                for (int i = 0; i < 8 && extension[i]; i++)
                    suffix[i] = (char)toupper((unsigned char)extension[i]);
            }
            draw_text(pixels, x + (w - (int)strlen(suffix) * 7) / 2, y + 85, suffix, colors.muted,
                      8);
        }
        /* Preserve the theme's readable font: type labels such as SYSTEM APP
         * wrap inside the metadata column instead of becoming SYSTE.. . */
        draw_text_wrapped(pixels, offset + 119, 59, entry ? entry_type(entry) : "BROWSE",
                          colors.accent, 7, 3);
        if (entry && (entry->kind == ITEM_FILE || entry->kind == ITEM_INSTALLED_TITLE) &&
            entry->size) {
            char b[24];
            if (entry->size > 1024 * 1024)
                snprintf(b, sizeof b, "%lu MB", (unsigned long)(entry->size / (1024 * 1024)));
            else
                snprintf(b, sizeof b, "%lu KB", (unsigned long)(entry->size / 1024));
            draw_text(pixels, offset + 119, 97, b, colors.text, 7);
        }
        draw_text(pixels, offset + 119, 115, app->dsi_mode ? "DSi" : "DS", colors.muted, 7);
        draw_text(pixels, offset + 119, 131,
                  entry && app_is_favorite(app, entry->path) ? "STARRED"
                  : entry && entry->kind == ITEM_SYSTEM ? (entry->target == 6 ? "SLOT-1" : "NAND")
                  : entry && (entry->kind == ITEM_INSTALLED_TITLE ||
                              entry->kind == ITEM_INSTALLED_LIBRARY)
                      ? "NAND"
                      : "SD CARD",
                  colors.muted, 8);
        bool installed_library = app->category == CATEGORY_SYSTEM && app->installed_open;
        draw_text_scrolling(pixels, offset + 12, 164,
                            installed_library    ? app->installed_catalog.status
                            : app->title[0]      ? app->title
                            : app->artwork.ready ? app->artwork.source
                                                 : "Y: item details",
                            colors.muted, 154,
                            app->selection_idle_frames * (installed_library ? 3u : 1u),
                            !app->reduced_motion);
    } else {
        draw_item_information(app, pixels, colors, offset);
    }
    for (int row = 0; !poster && row < TAB_VISIBLE_COUNT; row++) {
        int index = app->tab_scroll + row;
        if (index >= app->tab_count)
            break;
        int y = 38 + row * 20;
        bool active = index == app->category;
        if (active && !draw_theme_component(&app->themes.active, pixels, THEME_TAB_SELECTED,
                                            rail + 182, y - 2, 65, 16))
            draw_rectangle(pixels, rail + 182, y - 2, 65, 16, colors.accent);
        if (app->rail_focus && index == app->rail_selected) {
            draw_rectangle(pixels, rail + 180, y - 4, 69, 1, colors.text);
            draw_rectangle(pixels, rail + 180, y + 15, 69, 1, colors.text);
            draw_rectangle(pixels, rail + 180, y - 4, 1, 20, colors.text);
            draw_rectangle(pixels, rail + 248, y - 4, 1, 20, colors.text);
        }
        bool focused = app->rail_focus ? index == app->rail_selected : active;
        draw_text_scrolling(pixels, rail + 188, y, tabs_label(app, index),
                            active ? themes_tab_text(theme) : colors.muted, 56,
                            app->frame_count - app->tab_text_started_frame,
                            focused && !app->reduced_motion);
    }
    if (!poster && app->tab_scroll > 0)
        draw_text(pixels, rail + 209, 24, "^", colors.muted, 1);
    if (!poster && app->tab_scroll + TAB_VISIBLE_COUNT < app->tab_count)
        draw_text(pixels, rail + 209, 158, "v", colors.muted, 1);
    if (!theme->minimal_chrome)
        draw_rectangle(pixels, 10, 176, 236, 1, colors.line);
    draw_text(pixels, 10, 179, clock, colors.muted, 11);
    draw_text(pixels, app->clock_12_hour ? 76 : 54, 179, ui_text(&app->language, UI_TAB_HINT),
              colors.muted, 15);
    draw_text(pixels, 179, 179, app->storage_ready ? "SD OK" : "NO SD",
              app->storage_ready ? colors.accent : colors.muted, 6);
    draw_text(pixels, 226, 179, app->dsi_mode ? "DSi" : "DS", colors.muted, 3);
}
static void draw_overlay(const App *app, uint16_t *pixels, Palette colors)
{
    if (app->panel == PANEL_NONE)
        return;
    int y = layout_panel_top(app);
    int height = app->panel == PANEL_QUICK || app->panel == PANEL_ACTIONS ? 156 : 141;
    int footer = y + height - 20;
    if (!draw_theme_component(&app->themes.active, pixels, THEME_PANEL, 4, y, 248, height)) {
        draw_rectangle(pixels, 4, y + 3, 248, height, draw_rgb555(5, 8, 13));
        draw_rectangle(pixels, 4, y, 248, height, colors.surface);
        draw_rectangle(pixels, 4, y, 248, 2, colors.accent);
    }
    const char *title = app->panel == PANEL_QUICK     ? "QUICK CONTROLS"
                        : app->panel == PANEL_ACTIONS ? "ITEM ACTIONS"
                        : app->panel == PANEL_INFO    ? "ITEM DETAILS"
                                                      : "GLOAM SHELL NDX";
    draw_text(pixels, 14, y + 9, title, colors.accent, 30);
    if (app->panel == PANEL_MESSAGE) {
        draw_text_wrapped(pixels, 14, y + 29, app->message, colors.text, 36, 7);
        draw_text(pixels, 14, footer, "A / B  close", colors.muted, 30);
        return;
    }
    if (app->panel == PANEL_INFO) {
        const Entry *entry = app_selected(app);
        draw_text_wrapped(pixels, 14, y + 29, entry ? entry->path : "No selection", colors.text, 36,
                          5);
        draw_text(pixels, 14, y + 99, entry ? entry_type(entry) : "", colors.accent, 36);
        draw_text(pixels, 14, footer, "A / B  close", colors.muted, 30);
        return;
    }
    const char *quick[] = {
        "Accent",           "Reduced motion", "Scroll speed",        "Icon size",
        "Browser view",     "Open dashboard", "System applications", "Return to DSi System Menu",
        "Save preferences", "All settings"};
    const Entry *entry = app_selected(app);
    const char *actions[] = {
        entry && (entry->kind == ITEM_DIRECTORY || entry->kind == ITEM_CATEGORY ||
                  entry->kind == ITEM_INSTALLED_LIBRARY)
            ? "Open"
            : "Launch",
        entry && app_is_favorite(app, entry->path) ? "Remove favorite" : "Add favorite",
        "Inspect item / path",
        "Choose cover image",
        "Clear cover assignment",
        "Choose app for this file",
        "Clear application choice",
        "Edit file associations",
        "Title options",
        "Title cheats",
        "Locate missing target",
        "Remove shortcut",
        "Next folder batch",
        "Previous folder batch",
        "Choose browser icon",
        "Clear browser icon"};
    int count = app->panel == PANEL_QUICK ? QUICK_CONTROL_COUNT : ACTION_COUNT;
    for (int visible = 0; visible < UI_PANEL_VISIBLE_ROWS; visible++) {
        int i = app->panel_scroll + visible;
        if (i >= count)
            break;
        int row = y + 29 + visible * 15;
        if (i == app->panel_selection)
            draw_rectangle(pixels, 10, row - 2, 236, 14, colors.line);
        bool enabled = app->panel == PANEL_QUICK || app->action_enabled[i];
        draw_text_clipped(pixels, 16, row, app->panel == PANEL_QUICK ? quick[i] : actions[i],
                          enabled && i == app->panel_selection
                              ? themes_selection_text(&app->themes.active)
                              : colors.muted,
                          31);
        if (!enabled)
            draw_text(pixels, 239, row, "-", colors.muted, 1);
        if (app->panel == PANEL_QUICK && i == QUICK_ACCENT) {
            draw_text(pixels, 207, row,
                      *app->themes.active.id && !app->theme_accent_override
                          ? "Theme"
                          : themes_accent_name(app->accent_index),
                      colors.accent, 6);
        }
        if (app->panel == PANEL_QUICK && i == QUICK_REDUCED_MOTION)
            draw_text(pixels, 219, row, app->reduced_motion ? "On" : "Off", colors.accent, 4);
        if (app->panel == PANEL_QUICK && i == QUICK_SCROLL_SPEED) {
            const char *speeds[] = {"Slow", "Normal", "Fast"};
            draw_text(pixels, 200, row, speeds[app->scroll_speed], colors.accent, 6);
        }
        if (app->panel == PANEL_QUICK && i == QUICK_ICON_SIZE)
            draw_text(pixels, 200, row,
                      layout_has_icons(app) ? (app->icon_size == ICON_LARGE ? "Large" : "Small")
                                            : "Hidden",
                      layout_has_icons(app) ? colors.accent : colors.muted, 6);
        if (app->panel == PANEL_QUICK && i == QUICK_BROWSER_STYLE) {
            const char *views[] = {"Classic", "Extended", "List", "Grid", "Shelf", "Index"};
            draw_text(pixels, 186, row, views[app->browser_style], colors.accent, 8);
        }
    }
    if (app->panel == PANEL_QUICK || app->panel == PANEL_ACTIONS) {
        if (app->panel_scroll > 0)
            draw_text(pixels, 232, y + 9, "^", colors.muted, 1);
        if (app->panel_scroll + UI_PANEL_VISIBLE_ROWS < count)
            draw_text(pixels, 232, footer, "v", colors.muted, 1);
    }
    const char *hint = app->panel == PANEL_QUICK && app->panel_selection <= QUICK_BROWSER_STYLE
                           ? "< > change  A next  B close"
                           : "A select   B close";
    if (app->panel == PANEL_QUICK && app->panel_selection == QUICK_ICON_SIZE &&
        !layout_has_icons(app))
        hint = "Icons hidden in List view";
    draw_text(pixels, 14, footer, hint, colors.muted, 34);
}
static void draw_browser_icon(const App *app, uint16_t *pixels, int index, int x, int y, int size,
                              uint16_t color)
{
    const BrowserIcon *icon = icons_for_entry(app, index);
    if (icon) {
        const uint16_t *image = icon->pixels;
        if (icon->system_fallback && app->entries[index].kind == ITEM_SYSTEM) {
            const uint16_t *themed =
                themes_icon(&app->themes.active, PLACEHOLDER_COUNT + app->entries[index].target);
            if (themed)
                image = themed;
        }
        for (int row = 0; row < size; row++) {
            for (int column = 0; column < size; column++) {
                uint16_t pixel = image[(row * 32 / size) * 32 + column * 32 / size];
                if ((pixel & 0x8000) && x + column >= 0 && x + column < 256 && y + row >= 0 &&
                    y + row < 192)
                    pixels[(y + row) * 256 + x + column] = pixel;
            }
        }
        return;
    }
    draw_theme_icon(&app->themes.active, pixels, x, y, size,
                    entry_file_type(&app->entries[index]).icon, color);
}

/* Fixed-size jackets on a continuous shelf. Banners remain native icons,
 * rather than becoming oversized cover artwork once their preview resolves. */
static void draw_shelf(const App *app, uint16_t *pixels, Palette colors)
{
    const Location *location = &app->locations[app->category];
    int offset = app->shelf_offset_fixed / 256;
    int top = layout_shelf_top(app), base = top + UI_SHELF_CARD_HEIGHT;
    draw_rectangle(pixels, 10, base, 236, 2, colors.line);
    for (int cell = 0; cell < 3; cell++) {
        int index = location->selected - 1 + cell;
        if (index < 0 || index >= app->entry_count)
            continue;
        int x = 17 + cell * 80 + offset;
        bool selected = index == location->selected;
        const Artwork *art = artwork_shelf_for_entry(app, index);
        if (!draw_theme_component(&app->themes.active, pixels,
                                  selected ? THEME_CARD_SELECTED : THEME_CARD, x, top, 62,
                                  UI_SHELF_CARD_HEIGHT)) {
            draw_rectangle(pixels, x, top, 62, UI_SHELF_CARD_HEIGHT, colors.surface);
            draw_rectangle(pixels, x, top, 62, 1, colors.line);
        }
        if (app->show_covers && art && art->ready && strcmp(art->source, "NDS banner")) {
            int width = 56, height = art->height * width / art->width;
            if (height > UI_SHELF_CARD_HEIGHT - 8) {
                height = UI_SHELF_CARD_HEIGHT - 8;
                width = art->width * height / art->height;
            }
            if (width < 1)
                width = 1;
            if (height < 1)
                height = 1;
            int left = x + 4 + (56 - width) / 2, y = top + (UI_SHELF_CARD_HEIGHT - height) / 2;
            for (int row = 0; row < height; row++)
                for (int column = 0; column < width; column++) {
                    uint16_t color = art->pixels[(row * art->height / height) * art->width +
                                                 column * art->width / width];
                    if ((color & 0x8000) && left + column >= 0 && left + column < 256)
                        pixels[(y + row) * 256 + left + column] = color;
                }
        } else {
            int size = layout_shelf_icon_size(app);
            draw_browser_icon(app, pixels, index, x + (62 - size) / 2,
                              top + (UI_SHELF_CARD_HEIGHT - size) / 2, size,
                              selected ? colors.accent : colors.muted);
        }
        if (selected)
            draw_rectangle(pixels, x, base + 3, 62, 2, colors.accent);
    }
    char name[256], position[24];
    snprintf(position, sizeof position, "%d / %d", location->selected + 1, app->entry_count);
    draw_text(pixels, 12, top - 14, position, colors.muted, 24);
    browser_display_name(app, &app->entries[location->selected], name);
    draw_text_scrolling(pixels, 12, base + 11, name, colors.text, 230, app->selection_idle_frames,
                        !app->reduced_motion);
    draw_rectangle(pixels, 12, base + 28, 232, 1, colors.line);
    int progress = app->entry_count > 1 ? 224 * location->selected / (app->entry_count - 1) : 112;
    draw_rectangle(pixels, 12 + progress, base + 27, 8, 3, colors.accent);
}

static void draw_grid(const App *app, uint16_t *pixels, Palette colors)
{
    const Location *location = &app->locations[app->category];
    int height = layout_row_height(app), size = layout_icon_size(app);
    for (int cell = 0; cell < layout_visible_rows(app); cell++) {
        int index = location->scroll + cell;
        if (index >= app->entry_count)
            break;
        int x = 6 + (cell % layout_grid_columns(app)) * layout_grid_cell_width(app);
        int y = layout_list_top(app->category) + (cell / layout_grid_columns(app)) * height;
        bool selected = index == location->selected;
        bool skinned = draw_theme_component(&app->themes.active, pixels,
                                            selected ? THEME_CARD_SELECTED : THEME_CARD, x, y,
                                            layout_grid_cell_width(app) - 1, height - 1);
        if (selected && !skinned)
            draw_theme_selection(&app->themes.active, pixels, x, y, layout_grid_cell_width(app) - 1,
                                 height - 1);
        char name[256];
        browser_display_name(app, &app->entries[index], name);
        if (app->icon_size == ICON_SMALL) {
            draw_browser_icon(app, pixels, index, x + 4, y + 2, size,
                              selected ? colors.accent : colors.muted);
            draw_text_scrolling(pixels, x + 24, y + 4, name, colors.text, 13 * FONT_WIDTH,
                                app->selection_idle_frames, selected && !app->reduced_motion);
        } else {
            draw_browser_icon(app, pixels, index, x + (layout_grid_cell_width(app) - size) / 2,
                              y + 4, size, selected ? colors.accent : colors.muted);
            draw_text_scrolling(pixels, x + 5, y + height - 14, name, colors.text,
                                layout_grid_cell_width(app) - 10, app->selection_idle_frames,
                                selected && !app->reduced_motion);
        }
    }
}

void render_app(const App *app, uint16_t *top, uint16_t *bottom, const char *clock)
{
    if (app->storage_lost) {
        storage_render(app, top, bottom);
        return;
    }
    if (app->cheats) {
        cheats_render(app, top, bottom);
        return;
    }
    if (app->game_editor.open) {
        game_editor_render(app, top, bottom);
        return;
    }
    if (app->themes.open) {
        themes_render(app, top, bottom);
        return;
    }
    if (app->picker.open) {
        picker_render(app, top, bottom);
        return;
    }
    if (app->dashboard.open) {
        dashboard_render(app, top, bottom);
        return;
    }
    if (app->association_editor.open) {
        association_editor_render(app, top, bottom);
        return;
    }
    if (app->settings.open) {
        settings_render(app, top, bottom);
        return;
    }
    Palette colors = themes_palette(app);
    draw_top_screen(app, top, colors, clock);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_theme_chrome(&app->themes.active, bottom, layout_has_path(app->category) ? 34 : 22);
    draw_text_scrolling(bottom, 10, 7,
                        app->category == CATEGORY_HOME && app->collection_open >= 0
                            ? app->collections.groups[app->collection_open].label
                        : app->category == CATEGORY_SYSTEM && app->installed_open
                            ? ui_text(&app->language, UI_INSTALLED_TITLES)
                            : tabs_label(app, app->category),
                        colors.text, 161, app->frame_count - app->tab_text_started_frame,
                        !app->reduced_motion);
    draw_text_scrolling(bottom, 188, 7, ui_text(&app->language, UI_SETTINGS), colors.accent, 58,
                        app->frame_count, !app->reduced_motion);
    const Location *location = &app->locations[app->category];
    int list_top = layout_list_top(app->category);
    int visible_rows = layout_visible_rows(app);
    int row_height = layout_row_height(app);
    int icon_size = layout_icon_size(app);
    int list_height = layout_list_bottom(app) - list_top;
    if (layout_has_path(app->category)) {
        draw_text_scrolling(bottom, 10, 22,
                            app->scan_directory ? ui_text(&app->language, UI_READING_FOLDER)
                            : app->truncated    ? "512+ items / SELECT: folder batches"
                                                : location->path,
                            colors.muted, 231, app->selection_idle_frames, !app->reduced_motion);
    }
    if (!app->themes.active.minimal_chrome)
        draw_rectangle(bottom, 10, list_top - 5, 236, 1, colors.line);
    if (!app->entry_count) {
        draw_text_wrapped(bottom, 16, 65,
                          app->category == CATEGORY_SYSTEM && app->installed_open
                              ? (*app->installed_catalog.status ? app->installed_catalog.status
                                                                : "Reading NAND titles...")
                          : app->storage_ready
                              ? (app->scan_directory    ? ui_text(&app->language, UI_READING_FOLDER)
                                 : app->directory_error ? "Folder unavailable"
                                                        : ui_text(&app->language, UI_EMPTY_FOLDER))
                              : "SD card not mounted",
                          colors.text, 31, 3);
        draw_text(bottom, 16, 84, "B back   L/R change tab", colors.muted, 38);
    } else {
        if (app->browser_style == BROWSER_SHELF)
            draw_shelf(app, bottom, colors);
        else if (layout_is_grid(app))
            draw_grid(app, bottom, colors);
        else {
            int sy = list_top + app->selection_y_fixed / 256;
            draw_theme_selection(&app->themes.active, bottom, 6, sy, 244, row_height);
            for (int row = 0; row < visible_rows; row++) {
                int i = location->scroll + row;
                if (i >= app->entry_count)
                    break;
                const Entry *entry = &app->entries[i];
                int y = list_top + row * row_height;
                bool extended = app->browser_style == BROWSER_EXTENDED;
                if (extended && i != location->selected)
                    draw_rectangle(bottom, 10, y + row_height - 1, 236, 1, colors.line);
                if (layout_has_icons(app))
                    draw_browser_icon(app, bottom, i, 14, y + (row_height - icon_size) / 2,
                                      icon_size,
                                      i == location->selected ? colors.accent : colors.muted);
                int text_x = layout_has_icons(app) ? 14 + icon_size + 6 : 14;
                int text_y = extended ? y + (row_height - 26) / 2 : y + (row_height - 12) / 2;
                char display_name[256];
                browser_display_name(app, entry, display_name);
                draw_text_scrolling(bottom, text_x, text_y, display_name,
                                    i == location->selected
                                        ? themes_selection_text(&app->themes.active)
                                        : colors.muted,
                                    234 - text_x, app->selection_idle_frames,
                                    i == location->selected && !app->reduced_motion);
                if (extended) {
                    char metadata[64];
                    if (entry->kind != ITEM_FILE)
                        snprintf(metadata, sizeof metadata, "%s", entry_type(entry));
                    else if (entry->size < 1024)
                        snprintf(metadata, sizeof metadata, "%s / %lu B", entry_type(entry),
                                 (unsigned long)entry->size);
                    else if (entry->size < 1024 * 1024)
                        snprintf(metadata, sizeof metadata, "%s / %lu KB", entry_type(entry),
                                 (unsigned long)(entry->size / 1024));
                    else
                        snprintf(
                            metadata, sizeof metadata, "%s / %lu.%lu MB", entry_type(entry),
                            (unsigned long)(entry->size / (1024 * 1024)),
                            (unsigned long)((entry->size % (1024 * 1024)) * 10 / (1024 * 1024)));
                    draw_text_clipped(bottom, text_x, text_y + 13, metadata, colors.muted,
                                      (234 - text_x) / 7);
                }
                if (entry->kind == ITEM_DIRECTORY || entry->kind == ITEM_CATEGORY)
                    draw_text(bottom, 239, y + (row_height - 12) / 2, ">", colors.muted, 1);
                else if (app_is_favorite(app, entry->path))
                    draw_text(bottom, 239, y + (row_height - 12) / 2, "*", colors.accent, 1);
            }
        }
        if (app->entry_count > visible_rows && app->browser_style != BROWSER_SHELF) {
            int h = list_height * visible_rows / app->entry_count;
            if (h < 5)
                h = 5;
            draw_rectangle(bottom, 253, list_top, 2, list_height, colors.line);
            draw_rectangle(bottom, 253,
                           list_top +
                               (list_height - h) * location->selected / (app->entry_count - 1),
                           2, h, colors.accent);
        }
    }
    draw_rectangle(bottom, 10, 176, 236, 1, colors.line);
    draw_text(bottom, 10, 179,
              app->library_jump_held                ? "Y + Up/Down: jump initials"
              : app->rail_focus                     ? "Up/Down tabs  A choose  B/X back"
              : app->browser_style == BROWSER_INDEX ? "< > groups  A open  Y info  X tabs"
                                                    : ui_text(&app->language, UI_BROWSER_HINT),
              colors.muted, 40);
    if (app->startup_countdown) {
        char countdown[40];
        snprintf(countdown, sizeof countdown, "Auto launch in %u seconds",
                 (app->startup_countdown + 59) / 60);
        draw_rectangle(bottom, 10, 142, 236, 30, colors.surface);
        draw_rectangle(bottom, 10, 142, 236, 1, colors.accent);
        draw_text(bottom, 16, 145, countdown, colors.accent, 32);
        draw_text(bottom, 16, 158, "Any button cancels", colors.text, 32);
    }
    draw_overlay(app, bottom, colors);
}
