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
#include "ui_strings.h"

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
              app->truncated || app->locations[app->category].batch
                  ? ui_translate(&app->language, "dialog.current_batch", "Current batch only")
                  : ui_translate(&app->language, "dialog.group_hint", "Left/right: groups"),
              colors.muted, 22);
}

/* Facts use cached entries/metadata only: no file access in rendering. */
static void draw_item_information(const App *app, uint16_t *pixels, Palette colors, int offset)
{
    const Entry *entry = app_selected(app);
    int index = app->locations[app->category].selected;
    char size[32] = "", extra[48] = "", facts[80] = "";
    const char *type = entry ? entry_type(entry)
                             : ui_translate(&app->language, "dialog.no_selection", "No selection");
    const char *storage = "SD card";
    const char *title =
        entry ? entry->name : ui_translate(&app->language, "dialog.choose_item", "Choose an item");
    const char *hint = ui_translate(&app->language, "dialog.open_selection", "A: open selection");
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
            hint = ui_translate(&app->language, "dialog.check_launch", "A: check and launch");
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
                             count > 1 ? ui_translate(&app->language, "dialog.choose_on_launch",
                                                      "Choose app on launch")
                                       : ui_translate(&app->language, "dialog.no_association",
                                                      "No file association"));
            }
        } else if (entry->kind == ITEM_INSTALLED_TITLE && entry->target >= 0 &&
                   (unsigned)entry->target < app->installed_catalog.count) {
            const NativeTitle *native = &app->installed_catalog.titles[entry->target].title;
            storage = "NAND";
            snprintf(facts, sizeof facts, "ID %08lX%08lX", (unsigned long)native->high,
                     (unsigned long)native->low);
            hint = ui_translate(&app->language, "dialog.launch_nand", "A: launch from NAND");
        } else if (entry->kind == ITEM_SYSTEM) {
            storage = entry->target == 6 ? "Slot-1" : "NAND";
            hint =
                entry->target == 5 ? "A: Nintendo Menu"
                : entry->target == 6
                    ? ui_translate(&app->language, "dialog.launch_card", "A: launch card")
                    : ui_translate(&app->language, "dialog.launch_system", "A: launch system app");
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
              app->browser_style == BROWSER_INDEX
                  ? ui_translate(&app->language, "dialog.library_index", "Y: library index")
              : app->show_covers
                  ? "Y: artwork"
                  : ui_translate(&app->language, "dialog.covers_disabled", "Covers disabled"),
              colors.accent, 22);
}

/* Tab presentation is independent of navigation state and the browser layout. */
static PlaceholderIcon tab_icon(int index)
{
    static const PlaceholderIcon icons[] = {PLACEHOLDER_SHORTCUT,    PLACEHOLDER_GAME,
                                            PLACEHOLDER_APPLICATION, PLACEHOLDER_AUDIO,
                                            PLACEHOLDER_SYSTEM,      PLACEHOLDER_FOLDER};
    return index >= 0 && index < CATEGORY_COUNT ? icons[index] : PLACEHOLDER_FOLDER;
}

static void draw_tab_icon(const ThemePackage *theme, uint16_t *pixels, int x, int y, int size,
                          int index, uint16_t color)
{
    int asset = THEME_TAB_ICON_FIRST + (index < CATEGORY_COUNT ? index : CATEGORY_COUNT);
    if (!draw_theme_icon_asset(theme, pixels, x, y, size, asset))
        draw_theme_icon(theme, pixels, x, y, size, tab_icon(index), color);
}

static uint16_t draw_tab_highlight(const ThemePackage *theme, uint16_t *pixels, Palette colors,
                                   int x, int y, int width, int height)
{
    if (theme->tab_highlight == 1) {
        draw_rectangle(pixels, x, y + height - 2, width, 2, colors.accent);
    } else if (theme->tab_highlight == 2) {
        draw_rectangle(pixels, x, y, width, 1, colors.accent);
        draw_rectangle(pixels, x, y + height - 1, width, 1, colors.accent);
        draw_rectangle(pixels, x, y, 1, height, colors.accent);
        draw_rectangle(pixels, x + width - 1, y, 1, height, colors.accent);
    } else {
        if (!draw_theme_component(theme, pixels, THEME_TAB_SELECTED, x, y, width, height))
            draw_rectangle(pixels, x, y, width, height, colors.accent);
        return themes_tab_text(theme);
    }
    return colors.text;
}

static int tab_window_start(const App *app, int focus, int visible)
{
    int start = focus - visible / 2;
    if (start > app->tab_count - visible)
        start = app->tab_count - visible;
    return start < 0 ? 0 : start;
}

static void draw_context_tabs(const App *app, uint16_t *pixels, Palette colors)
{
    const ThemePackage *theme = &app->themes.active;
    int focus = app->rail_focus ? app->rail_selected : app->category;
    unsigned elapsed = app->frame_count - app->tab_text_started_frame;
    if (theme->tab_style == 3) {
        uint16_t color = draw_tab_highlight(theme, pixels, colors, 30, 155, 196, 18);
        draw_text(pixels, 10, 158, "<", colors.accent, 1);
        draw_text(pixels, 239, 158, ">", colors.accent, 1);
        draw_text_scrolling(pixels, 38, 158, tabs_label(app, focus), color, 135, elapsed,
                            !app->reduced_motion);
        char position[16];
        snprintf(position, sizeof position, "%d/%d", focus + 1, app->tab_count);
        draw_text(pixels, 177, 158, position, color, 6);
        return;
    }
    int visible = theme->tab_visible, gap = theme->tab_gap;
    int width = (242 - gap * (visible - 1)) / visible;
    int start = tab_window_start(app, focus, visible);
    for (int cell = 0; cell < visible; cell++) {
        int index = start + cell;
        if (index >= app->tab_count)
            break;
        int x = 7 + cell * (width + gap);
        bool active = index == focus;
        int y = theme->tab_style == 1 ? 144 : 155;
        uint16_t color =
            active ? draw_tab_highlight(theme, pixels, colors, x, y, width, 18) : colors.muted;
        if (theme->tab_style == 1) {
            draw_tab_icon(theme, pixels, x + (width - 16) / 2, y + 1, 16, index, color);
        } else {
            int inset = 4;
            if (theme->tab_style == 2) {
                char number[16];
                snprintf(number, sizeof number, "%02d", index + 1);
                draw_text(pixels, x + 3, 158, number, color, 2);
                inset = 20;
            }
            draw_text_scrolling(pixels, x + inset, 158, tabs_label(app, index), color,
                                width - inset - 4, elapsed, active && !app->reduced_motion);
        }
    }
    if (theme->tab_style == 1)
        draw_text_scrolling(pixels, 12, 164, tabs_label(app, focus), colors.text, 232, elapsed,
                            !app->reduced_motion);
}

static void draw_side_tabs(const App *app, uint16_t *pixels, Palette colors, int rail)
{
    const ThemePackage *theme = &app->themes.active;
    int focus = app->rail_focus ? app->rail_selected : app->category;
    unsigned elapsed = app->frame_count - app->tab_text_started_frame;
    if (theme->tab_style == 3) {
        uint16_t color = draw_tab_highlight(theme, pixels, colors, rail + 182, 60, 65, 76);
        draw_tab_icon(theme, pixels, rail + 202, 68, 24, focus, color);
        draw_text_scrolling(pixels, rail + 186, 101, tabs_label(app, focus), color, 57, elapsed,
                            !app->reduced_motion);
        char position[16];
        snprintf(position, sizeof position, "%d/%d", focus + 1, app->tab_count);
        draw_text(pixels, rail + 193, 119, position, color, 7);
        draw_text(pixels, rail + 188, 40, "< L/R >", colors.muted, 7);
        return;
    }
    int start = tab_window_start(app, focus, TAB_VISIBLE_COUNT);
    for (int row = 0; row < TAB_VISIBLE_COUNT; row++) {
        int index = start + row;
        if (index >= app->tab_count)
            break;
        bool focused = index == focus;
        if (theme->tab_style == 1) {
            int x = rail + 181 + (row % 2) * 35, y = 38 + (row / 2) * 34;
            uint16_t color =
                focused ? draw_tab_highlight(theme, pixels, colors, x, y, 32, 32) : colors.muted;
            draw_tab_icon(theme, pixels, x + 6, y + 6, 20, index, color);
        } else {
            int y = 38 + row * 20;
            uint16_t color =
                focused ? draw_tab_highlight(theme, pixels, colors, rail + 182, y - 2, 65, 16)
                        : colors.muted;
            int inset = 188;
            if (theme->tab_style == 2) {
                char number[16];
                snprintf(number, sizeof number, "%02d", index + 1);
                draw_text(pixels, rail + 185, y, number, color, 2);
                inset = 205;
            }
            draw_text_scrolling(pixels, rail + inset, y, tabs_label(app, index), color, 244 - inset,
                                elapsed, focused && !app->reduced_motion);
        }
    }
    if (theme->tab_style == 1)
        draw_text_scrolling(pixels, rail + 188, 144, tabs_label(app, focus), colors.text, 56,
                            elapsed, !app->reduced_motion);
    if (start > 0)
        draw_text(pixels, rail + 209, 24, "^", colors.muted, 1);
    if (start + TAB_VISIBLE_COUNT < app->tab_count)
        draw_text(pixels, rail + 209, 158, "v", colors.muted, 1);
}

/* Banner fallbacks are icons, not cover art. Keeping them on the icon path
 * preserves the themed card while asynchronous artwork metadata resolves. */
static bool preview_has_cover(const App *app)
{
    return app->artwork.ready && strcmp(app->artwork.source, "NDS banner") != 0;
}

static void draw_poster(const App *app, uint16_t *pixels, Palette colors)
{
    const ThemePackage *theme = &app->themes.active;
    const Entry *entry = app_selected(app);
    /* Icon docks begin at y=144; include the two-pixel artwork frame
     * when reserving space above them. Other tab styles begin at y=155. */
    int artwork_height = theme->tab_style == 1 ? 92 : 104;
    int selected = app->locations[app->category].selected;
    bool native = entry && (entry->kind == ITEM_SYSTEM || entry->kind == ITEM_INSTALLED_TITLE) &&
                  icons_for_entry(app, selected);
    if (!native && preview_has_cover(app)) {
        int width = 108, height = app->artwork.height * 108 / app->artwork.width;
        if (height > artwork_height) {
            height = artwork_height;
            width = app->artwork.width * artwork_height / app->artwork.height;
        }
        if (width < 1)
            width = 1;
        if (height < 1)
            height = 1;
        int x = 10 + (108 - width) / 2, y = 48 + (artwork_height - height) / 2;
        draw_theme_component(theme, pixels, THEME_ARTWORK_FRAME, x - 2, y - 2, width + 4,
                             height + 4);
        for (int yy = 0; yy < height; yy++)
            for (int xx = 0; xx < width; xx++)
                pixels[(y + yy) * 256 + x + xx] =
                    app->artwork.pixels[(yy * app->artwork.height / height) * app->artwork.width +
                                        xx * app->artwork.width / width];
    } else {
        draw_theme_component(theme, pixels, THEME_CARD, 10, 48, 108, artwork_height);
        if (icons_for_entry(app, selected))
            draw_browser_icon(app, pixels, selected, 24, 48 + (artwork_height - 80) / 2, 80,
                              colors.accent);
        else
            draw_theme_icon(theme, pixels, 24, 48 + (artwork_height - 80) / 2, 80,
                            entry_file_type(entry).icon, colors.accent);
    }
    draw_text_wrapped(pixels, 130, 53, entry ? entry_type(entry) : "Browse", colors.accent, 16, 2);
    char size[32] = "";
    if (entry && entry->size)
        snprintf(size, sizeof size, "%lu KiB", (unsigned long)(entry->size / 1024));
    draw_text(pixels, 130, 86, size, colors.text, 16);
    draw_text(pixels, 130, 105, app->dsi_mode ? "Nintendo DSi" : "Nintendo DS", colors.muted, 16);
    draw_text(pixels, 130, 131, ui_translate(&app->language, "dialog.details_hint", "Y: details"),
              colors.muted, 16);
    draw_context_tabs(app, pixels, colors);
}

/* Theme illustration remains visible above a compact, live item inspector. */
static void draw_scene(const App *app, uint16_t *pixels, Palette colors, const char *name)
{
    const ThemePackage *theme = &app->themes.active;
    const Entry *entry = app_selected(app);
    themes_background_scene_draw(theme, pixels);
    int box_x = theme->scene_cover_x, box_y = theme->scene_cover_y;
    int box_width = theme->scene_cover_width, box_height = theme->scene_cover_height;
    if (entry && app->show_covers) {
        if (preview_has_cover(app)) {
            int width = box_width, height = app->artwork.height * width / app->artwork.width;
            if (height > box_height) {
                height = box_height;
                width = app->artwork.width * height / app->artwork.height;
            }
            if (width < 1)
                width = 1;
            if (height < 1)
                height = 1;
            int x = box_x + (box_width - width) / 2, y = box_y + (box_height - height) / 2;
            draw_theme_component(theme, pixels, THEME_ARTWORK_FRAME, x - 2, y - 2, width + 4,
                                 height + 4);
            for (int yy = 0; yy < height; yy++)
                for (int xx = 0; xx < width; xx++)
                    pixels[(y + yy) * 256 + x + xx] =
                        app->artwork
                            .pixels[(yy * app->artwork.height / height) * app->artwork.width +
                                    xx * app->artwork.width / width];
        } else {
            int size = box_width < box_height ? box_width : box_height;
            int x = box_x + (box_width - size) / 2, y = box_y + (box_height - size) / 2;
            draw_theme_component(theme, pixels, THEME_CARD, x - 2, y - 2, size + 4, size + 4);
            draw_browser_icon(app, pixels, app->locations[app->category].selected, x, y, size,
                              colors.accent);
        }
    }
    draw_rectangle(pixels, 6, 119, 244, 34, colors.bg);
    draw_text_scrolling(pixels, 12, 121, name, colors.text, 232, app->selection_idle_frames,
                        !app->reduced_motion);
    char facts[80];
    if (entry && entry->size)
        snprintf(facts, sizeof facts, "%s / %lu KiB", entry_type(entry),
                 (unsigned long)(entry->size / 1024));
    else
        snprintf(facts, sizeof facts, "%s / Y: details", entry ? entry_type(entry) : "Browse");
    if (theme->tab_style != 1)
        draw_text_scrolling(pixels, 12, 138, facts, colors.muted, 232, app->selection_idle_frames,
                            !app->reduced_motion);
    draw_context_tabs(app, pixels, colors);
}

static void draw_top_screen(const App *app, uint16_t *pixels, Palette colors, const char *clock)
{
    const ThemePackage *theme = &app->themes.active;
    int offset = theme->top_layout == 1 ? 78 : 0;
    int rail = theme->top_layout == 1 ? -178 : 0;
    bool scene =
        theme->top_layout == 3 && app->detail_tab == 0 && app->browser_style != BROWSER_INDEX;
    bool poster = theme->top_layout == 2 && app->detail_tab == 0 && app->show_covers &&
                  app->browser_style != BROWSER_INDEX;
    themes_background_draw(&app->themes.active, 0, pixels);
    draw_theme_chrome(&app->themes.active, pixels, 22);
    draw_text_scrolling(pixels, 10, 7, app->recovery_mode ? "RECOVERY" : theme->header, colors.text,
                        138, app->frame_count, !app->reduced_motion);
    draw_theme_status(app, pixels);
    char tab_position[16];
    snprintf(tab_position, sizeof tab_position, "%d/%d", app->category + 1, app->tab_count);
    draw_text(pixels, 205, 7, tab_position, colors.muted, 6);
    if (!theme->minimal_chrome)
        draw_rectangle(pixels, 10, 23, 236, 1, colors.line);
    if (!poster && !scene && !theme->minimal_chrome)
        draw_rectangle(pixels, offset ? 78 : 174, 33, 1, 133, colors.line);
    const Entry *entry = app_selected(app);
    const char *name =
        entry ? entry->name
        : app->storage_ready
            ? ui_translate(&app->language, "dialog.nothing_here", "Nothing here yet")
            : ui_translate(&app->language, "dialog.sd_unavailable", "SD card unavailable");
    if (entry && entry->kind == ITEM_SYSTEM && entry->target == 6 && app->slot1.icon.ready &&
        *app->slot1.title)
        name = app->slot1.title;
    if (!scene)
        draw_text_scrolling(pixels, offset + 12, 31, name, colors.text, poster ? 232 : 154,
                            app->selection_idle_frames, !app->reduced_motion);
    if (scene) {
        draw_scene(app, pixels, colors, name);
    } else if (poster) {
        draw_poster(app, pixels, colors);
    } else if (app->browser_style == BROWSER_INDEX && app->detail_tab == 0) {
        draw_library_index(app, pixels, colors, offset);
    } else if (app->detail_tab == 0 && app->show_covers) {
        int x = offset + 15, y = 53, w = 96, h = 104;
        int selected = app->locations[app->category].selected;
        if (entry && (entry->kind == ITEM_SYSTEM || entry->kind == ITEM_INSTALLED_TITLE ||
                      (entry->kind == ITEM_FILE && icon_is_ds_title(entry->path) &&
                       !preview_has_cover(app)))) {
            draw_theme_component(theme, pixels, THEME_CARD, x, y, w, h);
            draw_browser_icon(app, pixels, selected, offset + 31, 73, 64, colors.accent);
            draw_text(pixels, offset + 24, 149,
                      entry->kind == ITEM_INSTALLED_TITLE           ? "NAND TITLE"
                      : entry->kind == ITEM_FILE                    ? "TITLE ICON"
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
            if (!draw_theme_component(theme, pixels, THEME_ARTWORK_FRAME, x - 2, y - 2, w + 4,
                                      h + 4))
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
    if (!poster && !scene)
        draw_side_tabs(app, pixels, colors, rail);
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
    const char *title =
        app->panel == PANEL_QUICK
            ? ui_translate(&app->language, "dialog.quick_controls", "QUICK CONTROLS")
        : app->panel == PANEL_ACTIONS
            ? ui_translate(&app->language, "dialog.item_actions", "ITEM ACTIONS")
        : app->panel == PANEL_INFO
            ? ui_translate(&app->language, "dialog.item_details", "ITEM DETAILS")
            : "GLOAM SHELL NDX";
    draw_text(pixels, 14, y + 9, title, colors.accent, 30);
    if (app->panel == PANEL_MESSAGE) {
        draw_text_wrapped(pixels, 14, y + 29, app->message, colors.text, 36, 7);
        draw_text(pixels, 14, footer,
                  ui_translate(&app->language, "dialog.close_hint", "A / B  close"), colors.muted,
                  30);
        return;
    }
    if (app->panel == PANEL_INFO) {
        const Entry *entry = app_selected(app);
        draw_text_wrapped(pixels, 14, y + 29,
                          entry
                              ? entry->path
                              : ui_translate(&app->language, "dialog.no_selection", "No selection"),
                          colors.text, 36, 5);
        draw_text(pixels, 14, y + 99, entry ? entry_type(entry) : "", colors.accent, 36);
        draw_text(pixels, 14, footer,
                  ui_translate(&app->language, "dialog.close_hint", "A / B  close"), colors.muted,
                  30);
        return;
    }
    const char *quick[] = {
        ui_translate(&app->language, "setting.accent.label", "Accent"),
        ui_translate(&app->language, "setting.motion.label", "Reduced motion"),
        ui_translate(&app->language, "setting.speed.label", "Scroll speed"),
        ui_translate(&app->language, "setting.icons.label", "Icon size"),
        ui_translate(&app->language, "setting.view.label", "Browser view"),
        ui_translate(&app->language, "dialog.open_dashboard", "Open dashboard"),
        ui_translate(&app->language, "dialog.system_apps", "System applications"),
        ui_translate(&app->language, "dialog.return_menu", "Return to DSi System Menu"),
        ui_translate(&app->language, "dialog.save_preferences", "Save preferences"),
        ui_translate(&app->language, "dialog.all_settings", "All settings")};
    const Entry *entry = app_selected(app);
    const char *actions[] = {
        entry && (entry->kind == ITEM_DIRECTORY || entry->kind == ITEM_CATEGORY ||
                  entry->kind == ITEM_INSTALLED_LIBRARY)
            ? "Open"
            : "Launch",
        entry && app_is_favorite(app, entry->path)
            ? ui_translate(&app->language, "dialog.remove_favorite", "Remove favorite")
            : ui_translate(&app->language, "dialog.add_favorite", "Add favorite"),
        ui_translate(&app->language, "dialog.inspect_item", "Inspect item / path"),
        ui_translate(&app->language, "dialog.choose_cover", "Choose cover image"),
        ui_translate(&app->language, "dialog.clear_cover", "Clear cover assignment"),
        ui_translate(&app->language, "dialog.choose_handler", "Choose app for this file"),
        ui_translate(&app->language, "dialog.clear_handler", "Clear application choice"),
        ui_translate(&app->language, "dialog.edit_associations", "Edit file associations"),
        ui_translate(&app->language, "dialog.title_options", "Title options"),
        ui_translate(&app->language, "dialog.title_cheats", "Title cheats"),
        ui_translate(&app->language, "dialog.locate_target", "Locate missing target"),
        ui_translate(&app->language, "dialog.remove_shortcut", "Remove shortcut"),
        ui_translate(&app->language, "dialog.next_batch", "Next folder batch"),
        ui_translate(&app->language, "dialog.previous_batch", "Previous folder batch"),
        ui_translate(&app->language, "dialog.choose_icon", "Choose browser icon"),
        ui_translate(&app->language, "dialog.clear_icon", "Clear browser icon")};
    int count = app->panel == PANEL_QUICK ? QUICK_CONTROL_COUNT : ACTION_COUNT;
    for (int visible = 0; visible < UI_PANEL_VISIBLE_ROWS; visible++) {
        int i = app->panel_scroll + visible;
        if (i >= count)
            break;
        int row = y + 29 + visible * 15;
        if (i == app->panel_selection)
            draw_rectangle(pixels, 10, row - 2, 236, 14, colors.line);
        bool enabled = app->panel == PANEL_QUICK || app->action_enabled[i];
        bool selected = i == app->panel_selection;
        bool has_value = app->panel == PANEL_QUICK && i <= QUICK_BROWSER_STYLE;
        draw_text_scrolling(
            pixels, 16, row, app->panel == PANEL_QUICK ? quick[i] : actions[i],
            enabled && selected ? themes_selection_text(&app->themes.active) : colors.muted,
            has_value ? 162 : 217, app->selection_idle_frames, selected && !app->reduced_motion);
        if (!enabled)
            draw_text(pixels, 239, row, "-", colors.muted, 1);
        if (has_value) {
            const char *value = "";
            switch (i) {
            case QUICK_ACCENT:
                value = *app->themes.active.id && !app->theme_accent_override
                            ? ui_translate(&app->language, "value.theme", "Theme")
                            : themes_accent_name(app->accent_index);
                break;
            case QUICK_REDUCED_MOTION:
                value = app->reduced_motion ? ui_translate(&app->language, "value.on", "On")
                                            : ui_translate(&app->language, "value.off", "Off");
                break;
            case QUICK_SCROLL_SPEED: {
                const char *values[] = {ui_translate(&app->language, "value.slow", "Slow"),
                                        ui_translate(&app->language, "value.normal", "Normal"),
                                        ui_translate(&app->language, "value.fast", "Fast")};
                value = values[app->scroll_speed];
                break;
            }
            case QUICK_ICON_SIZE:
                value = !layout_has_icons(app)
                            ? ui_translate(&app->language, "value.hidden", "Hidden")
                        : app->icon_size == ICON_LARGE
                            ? ui_translate(&app->language, "value.large", "Large")
                            : ui_translate(&app->language, "value.small", "Small");
                break;
            case QUICK_BROWSER_STYLE: {
                const char *values[] = {ui_translate(&app->language, "value.classic", "Classic"),
                                        ui_translate(&app->language, "value.extended", "Extended"),
                                        ui_translate(&app->language, "value.list", "List"),
                                        ui_translate(&app->language, "value.grid", "Grid"),
                                        ui_translate(&app->language, "value.shelf", "Shelf"),
                                        ui_translate(&app->language, "value.index", "Index")};
                value = values[app->browser_style];
                break;
            }
            }
            draw_text_scrolling(pixels, 186, row, value,
                                i == QUICK_ICON_SIZE && !layout_has_icons(app) ? colors.muted
                                                                               : colors.accent,
                                57, app->selection_idle_frames, selected && !app->reduced_motion);
        }
    }
    if (app->panel == PANEL_QUICK || app->panel == PANEL_ACTIONS) {
        if (app->panel_scroll > 0)
            draw_text(pixels, 232, y + 9, "^", colors.muted, 1);
        if (app->panel_scroll + UI_PANEL_VISIBLE_ROWS < count)
            draw_text(pixels, 232, footer, "v", colors.muted, 1);
    }
    const char *hint =
        app->panel == PANEL_QUICK && app->panel_selection <= QUICK_BROWSER_STYLE
            ? ui_translate(&app->language, "dialog.settings_adjust", "< > change  A next  B close")
            : ui_translate(&app->language, "dialog.select_close", "A select   B close");
    if (app->panel == PANEL_QUICK && app->panel_selection == QUICK_ICON_SIZE &&
        !layout_has_icons(app))
        hint = ui_translate(&app->language, "dialog.icons_hidden", "Icons hidden in List view");
    draw_text(pixels, 14, footer, hint, colors.muted, 34);
}
static void draw_browser_icon(const App *app, uint16_t *pixels, int index, int x, int y, int size,
                              uint16_t color)
{
    draw_theme_component(&app->themes.active, pixels, THEME_ICON_FRAME, x - 2, y - 2, size + 4,
                         size + 4);
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

static void render_content(const App *app, uint16_t *top, uint16_t *bottom, const char *clock)
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
        draw_text_wrapped(
            bottom, 16, 65,
            app->category == CATEGORY_SYSTEM && app->installed_open
                ? (*app->installed_catalog.status
                       ? app->installed_catalog.status
                       : ui_translate(&app->language, "dialog.nand_reading",
                                      "Reading NAND titles..."))
            : app->storage_ready
                ? (app->scan_directory ? ui_text(&app->language, UI_READING_FOLDER)
                   : app->directory_error
                       ? ui_translate(&app->language, "dialog.folder_unavailable",
                                      "Folder unavailable")
                       : ui_text(&app->language, UI_EMPTY_FOLDER))
                : ui_translate(&app->language, "dialog.sd_unmounted", "SD card not mounted"),
            colors.text, 31, 3);
        draw_text(
            bottom, 16, 84,
            ui_translate(&app->language, "dialog.empty_navigation", "B back   L/R change tab"),
            colors.muted, 38);
    } else {
        if (app->browser_style == BROWSER_SHELF)
            draw_shelf(app, bottom, colors);
        else if (layout_is_grid(app))
            draw_grid(app, bottom, colors);
        else {
            /* A moving ornamental border can cross the stationary label in
             * another row. Keep framed selections anchored to their content. */
            int sy = themes_component(&app->themes.active, THEME_ROW_SELECTED)
                         ? list_top + (location->selected - location->scroll) * row_height
                         : list_top + app->selection_y_fixed / 256;
            for (int row = 0; row < visible_rows && location->scroll + row < app->entry_count;
                 row++)
                draw_theme_component(&app->themes.active, bottom, THEME_ROW, 6,
                                     list_top + row * row_height, 244, row_height);
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
        draw_text(bottom, 16, 158,
                  ui_translate(&app->language, "dialog.cancel_any", "Any button cancels"),
                  colors.text, 32);
    }
    draw_overlay(app, bottom, colors);
}

void render_app(const App *app, uint16_t *top, uint16_t *bottom, const char *clock)
{
    render_content(app, top, bottom, clock);
    if (!app->system_notice_active)
        return;
    Palette colors = themes_palette(app);
    draw_rectangle(bottom, 10, 108, 236, 66, colors.surface);
    draw_rectangle(bottom, 10, 108, 236, 1, colors.accent);
    draw_text(bottom, 16, 113, "Language-patched menu", colors.accent, 30);
    draw_text_wrapped(bottom, 16, 130, "Press A shortly after launch to continue.", colors.text, 30,
                      2);
    draw_text(bottom, 16, 159, "B: cancel", colors.muted, 30);
}
