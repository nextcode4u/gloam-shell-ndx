/* Optional content presets share navigation, theme skins and cached artwork. */
#include "artwork.h"
#include "browser.h"
#include "collections.h"
#include "drawing.h"
#include "file_types.h"
#include "themes.h"
#include "top_content.h"
#include <stdio.h>
#include <string.h>

static void line(const App *app, uint16_t *pixels, int x, int y, int width, const char *text,
                 uint16_t color)
{
    draw_text_scrolling(pixels, x, y, text, color, width, app->selection_idle_frames,
                        !app->reduced_motion);
}
static void card(const App *app, uint16_t *pixels, int index, int x, int y, int width, int height,
                 bool selected, TopContentIcon icon)
{
    if (index < 0 || index >= app->entry_count)
        return;
    const ThemePackage *theme = &app->themes.active;
    ThemePalette p = themes_palette(app);
    if (!draw_theme_component(theme, pixels, selected ? THEME_CARD_SELECTED : THEME_CARD, x, y,
                              width, height))
        draw_rectangle(pixels, x, y, width, height, p.surface);
    /* Keep source art inside the widest supported authored nine-slice border. */
    int inset = (int)theme->component_border + 2;
    if (inset < 6)
        inset = 6;
    int w = width - 2 * inset, h = height - 2 * inset;
    if (w <= 0 || h <= 0)
        return;
    const Artwork *art = selected ? &app->artwork : artwork_shelf_for_entry(app, index);
    if (app->show_covers && art && art->ready && art->width > 0 && art->height > 0 &&
        strcmp(art->source, "NDS banner")) {
        int scaled_h = art->height * w / art->width;
        if (scaled_h > h) {
            w = art->width * h / art->height;
        } else
            h = scaled_h;
        int left = x + (width - w) / 2, top = y + (height - h) / 2;
        for (int yy = 0; yy < h; yy++)
            for (int xx = 0; xx < w; xx++)
                pixels[(top + yy) * 256 + left + xx] =
                    art->pixels[(yy * art->height / h) * art->width + xx * art->width / w];
    } else {
        int size = w < h ? w : h;
        icon(app, pixels, index, x + (width - size) / 2, y + (height - size) / 2, size, p.accent);
    }
}

static void context(const App *app, uint16_t *pixels, TopContentIcon icon)
{
    const Entry *entry = app_selected(app);
    const TopContentState *state = &app->top;
    ThemePalette p = themes_palette(app);
    if (!entry) {
        line(app, pixels, 14, 65, 228, "Choose an item to see its details", p.muted);
        return;
    }
    int selected = app->locations[app->category].selected;
    card(app, pixels, selected, 10, 51, 64, 60, true, icon);
    const char *handler = *state->handler ? state->handler : entry_type(entry);
    char native_route[96] = "";
    if (app->info_header_valid && !strcmp(state->handler, "DS/DSi title")) {
        handler = app->info_unit == 0   ? "Nintendo DS"
                  : app->info_unit == 2 ? "DSi enhanced"
                  : app->info_unit == 3 ? "DSi only"
                                        : "DS/DSi title";
        snprintf(native_route, sizeof native_route, "%s / rev. %u", app->info_code,
                 app->info_revision);
    } else if (entry->kind == ITEM_SYSTEM) {
        snprintf(native_route, sizeof native_route, "%s / Unlaunch",
                 entry->target == 6 ? "Slot-1" : "NAND");
    } else if (entry->kind == ITEM_INSTALLED_TITLE && entry->target >= 0 &&
               (unsigned)entry->target < app->installed_catalog.count) {
        const NativeTitle *title = &app->installed_catalog.titles[entry->target].title;
        snprintf(native_route, sizeof native_route, "%08lX/%08lX", (unsigned long)title->high,
                 (unsigned long)title->low);
    }
    line(app, pixels, 82, 54, 160, handler, p.accent);
    if (app->detail_tab && app->info_header_valid) {
        char facts[80];
        snprintf(facts, sizeof facts, "Code: %s / revision %u", app->info_code, app->info_revision);
        line(app, pixels, 82, 70, 160, facts, p.text);
        line(app, pixels, 82, 86, 160, app->info_title, p.muted);
    } else {
        line(app, pixels, 82, 70, 160, *native_route ? native_route : state->route, p.text);
        line(app, pixels, 82, 86, 160, state->options, p.muted);
    }
    line(app, pixels, 82, 102, 160, state->save, p.muted);
    char credits[224];
    snprintf(credits, sizeof credits, "%s%s%s%s%s", state->developer,
             *state->developer && *state->genre ? " / " : "", state->genre,
             (*state->developer || *state->genre) && *state->year ? " / " : "", state->year);
    line(app, pixels, 12, 117, 232, *credits ? credits : app->info_title, p.text);
    line(app, pixels, 12, 131, 232,
         state->metadata_invalid ? "Invalid optional metadata; launch is unaffected"
         : *state->description   ? state->description
         : app->detail_tab       ? "Y: overview"
                                 : "Y: details",
         p.muted);
}

static void neighbors(const App *app, uint16_t *pixels, TopContentIcon icon)
{
    ThemePalette p = themes_palette(app);
    int selected = app->locations[app->category].selected;
    for (int i = 0; i < 3; i++) {
        int index = selected + i - 1;
        card(app, pixels, index, i == 1 ? 89 : 12 + i * 82, i == 1 ? 50 : 60, i == 1 ? 78 : 66,
             i == 1 ? 79 : 61, i == 1, icon);
    }
    char position[64];
    snprintf(position, sizeof position, "%d / %d%s", app->entry_count ? selected + 1 : 0,
             app->entry_count,
             app->truncated || app->locations[app->category].batch ? " in this batch" : "");
    line(app, pixels, 12, 132, 232, position, p.muted);
}

static void folder(const App *app, uint16_t *pixels, TopContentIcon icon)
{
    const TopContentState *state = &app->top;
    ThemePalette p = themes_palette(app);
    char counts[80];
    snprintf(counts, sizeof counts, "%u files / %u folders%s", state->files, state->folders,
             app->truncated || app->locations[app->category].batch ? " / batch only" : "");
    line(app, pixels, 12, 52, 232, counts, p.text);
    char types[128] = "";
    for (unsigned type = 0; type < PLACEHOLDER_COUNT; type++)
        if (state->types[type]) {
            size_t used = strlen(types);
            snprintf(types + used, sizeof types - used, "%s%s %u", used ? " / " : "",
                     placeholder_label(type), state->types[type]);
        }
    line(app, pixels, 12, 68, 232, types, p.muted);
    for (int i = 0; i < 3; i++)
        card(app, pixels, state->stage ? state->samples[i] : -1, 18 + i * 78, 87, 64, 45,
             state->stage && state->samples[i] == app->locations[app->category].selected, icon);
    line(app, pixels, 12, 132, 232, "Current folder / no recursive scan", p.muted);
}

static void activity(const App *app, uint16_t *pixels, TopContentIcon icon)
{
    ThemePalette p = themes_palette(app);
    char summary[96];
    snprintf(summary, sizeof summary, "%d pins / %d collections / %d requests", app->favorite_count,
             app->collections.count, app->dashboard.recent_count);
    line(app, pixels, 12, 52, 232, summary, p.text);
    int indexes[3];
    top_content_samples(app, indexes);
    for (int i = 0; i < 3; i++) {
        card(app, pixels, indexes[i], 18 + i * 78, 72, 64, 48,
             indexes[i] == app->locations[app->category].selected, icon);
        if (indexes[i] >= 0)
            line(app, pixels, 18 + i * 78, 118, 64, app->entries[indexes[i]].name, p.muted);
    }
    line(app, pixels, 12, 132, 232, "Recent requests do not measure playtime", p.muted);
}

void top_content_render(const App *app, uint16_t *pixels, TopContentIcon icon)
{
    const Entry *entry = app_selected(app);
    ThemePalette p = themes_palette(app);
    TopContentMode mode = top_content_mode(app);
    const char *title = mode == TOP_CONTENT_FOLDER     ? "Folder overview"
                        : mode == TOP_CONTENT_ACTIVITY ? "Home activity"
                        : *app->top.title              ? app->top.title
                        : entry                        ? entry->name
                                                       : "Library";
    if (mode == TOP_CONTENT_FOLDER)
        title = app->locations[app->category].path;
    else if (entry && entry->kind == ITEM_SYSTEM && entry->target == 6 && *app->slot1.title)
        title = app->slot1.title;
    draw_heading_scrolling(&app->themes.active, pixels, 12, 31, title, p.text, 232,
                           app->selection_idle_frames, !app->reduced_motion);
    if (mode == TOP_CONTENT_NEIGHBORS)
        neighbors(app, pixels, icon);
    else if (mode == TOP_CONTENT_FOLDER)
        folder(app, pixels, icon);
    else if (mode == TOP_CONTENT_ACTIVITY)
        activity(app, pixels, icon);
    else
        context(app, pixels, icon);
}
