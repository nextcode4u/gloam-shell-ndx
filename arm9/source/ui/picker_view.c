/* Dedicated two-screen picker view; all file access belongs to picker.c. */
#include "drawing.h"
#include "picker.h"
#include "themes.h"
#include <stdio.h>
#include <string.h>

static void render_handlers(const App *app, uint16_t *top, uint16_t *bottom)
{
    const PickerState *picker = &app->picker;
    ThemePalette p = themes_palette(app);
    themes_background_draw(&app->themes.active, 0, top);
    draw_theme_chrome(&app->themes.active, top, 22);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_theme_chrome(&app->themes.active, bottom, 22);
    draw_text(top, 10, 8, "OPEN WITH", p.accent, 32);
    const char *name = strrchr(picker->target, '/');
    draw_text_scrolling(top, 10, 31, name ? name + 1 : picker->target, p.text, 231,
                        picker->idle_frames, !app->reduced_motion);
    draw_text(top, 10, 57, "Selected application", p.muted, 32);
    if (picker->count && picker->selected < picker->count) {
        const Entry *entry = &picker->entries[picker->selected];
        draw_text_scrolling(top, 10, 77, entry->name, p.accent, 231, picker->idle_frames,
                            !app->reduced_motion);
        draw_text_scrolling(top, 10, 98, entry->path, p.text, 231, picker->idle_frames,
                            !app->reduced_motion);
        const Association *profile = &app->associations[entry->target];
        draw_text(top, 10, 120, "Preferred folder", p.muted, 32);
        draw_text_scrolling(top, 10, 135,
                            *profile->content_root ? profile->content_root : "Any folder", p.text,
                            231, picker->idle_frames, !app->reduced_motion);
    }
    draw_text_wrapped(top, 10, 155,
                      *picker->status
                          ? picker->status
                          : "Several apps handle this file. Your choice applies to this launch.",
                      p.muted, 33, 3);
    draw_text(bottom, 10, 7, "Choose application", p.text, 32);
    draw_text(bottom, 10, 24, "Folder rules could not select one", p.muted, 33);
    for (int row = 0; row < PICKER_VISIBLE_ROWS; row++) {
        int index = picker->scroll + row;
        if (index >= picker->count)
            break;
        int y = PICKER_TOP + row * PICKER_ROW_HEIGHT;
        bool selected = index == picker->selected;
        if (selected) {
            draw_theme_selection(&app->themes.active, bottom, 6, y, 244, PICKER_ROW_HEIGHT);
        }
        draw_theme_icon(&app->themes.active, bottom, 14, y + 3, 12, PLACEHOLDER_APPLICATION,
                        selected ? p.accent : p.muted);
        draw_text_scrolling(bottom, 36, y + 3, picker->entries[index].name,
                            selected ? themes_selection_text(&app->themes.active) : p.muted, 203,
                            picker->idle_frames, selected && !app->reduced_motion);
    }
    draw_text(bottom, 10, 176, "A launch  B / START cancel", p.muted, 33);
}

void picker_render(const App *app, uint16_t *top, uint16_t *bottom)
{
    if (app->picker.kind == PICKER_HANDLER) {
        render_handlers(app, top, bottom);
        return;
    }
    const PickerState *picker = &app->picker;
    ThemePalette palette = themes_palette(app);
    uint16_t surface = palette.surface, text = palette.text;
    uint16_t muted = palette.muted, accent = palette.accent;
    themes_background_draw(&app->themes.active, 0, top);
    draw_theme_chrome(&app->themes.active, top, 22);
    themes_background_draw(&app->themes.active, 1, bottom);
    draw_theme_chrome(&app->themes.active, bottom, 22);
    draw_text(top, 10, 8,
              (picker->kind == PICKER_TOP_BACKGROUND || picker->kind == PICKER_BOTTOM_BACKGROUND)
                  ? "WALLPAPER"
              : picker->kind == PICKER_ICON          ? "BROWSER ICON"
              : picker->kind == PICKER_CONFIGURATION ? "IMPORT CONFIGURATION"
              : picker->kind == PICKER_REPAIR        ? "LOCATE SHORTCUT"
              : picker->kind == PICKER_AUDIO         ? "LAUNCHER MUSIC"
              : picker->kind == PICKER_COVER         ? "COVER ASSIGNMENT"
                                                     : "APPLICATION FOR FILE",
              accent, 33);
    const char *target = strrchr(picker->target, '/');
    draw_text_scrolling(top, 10, 31, target ? target + 1 : picker->target, text, 231,
                        picker->idle_frames, !app->reduced_motion);
    if (picker->preview.ready) {
        int x = 10 + (96 - picker->preview.width) / 2, y = 54;
        for (int row = 0; row < picker->preview.height; row++)
            memcpy(top + (y + row) * 256 + x, picker->preview.pixels + row * picker->preview.width,
                   picker->preview.width * sizeof *top);
    } else {
        draw_rectangle(top, 10, 54, 96, 104, surface);
        PlaceholderIcon icon =
            picker->kind == PICKER_AUDIO           ? PLACEHOLDER_AUDIO
            : picker->kind == PICKER_CONFIGURATION ? PLACEHOLDER_CONFIG
            : picker->kind == PICKER_REPAIR        ? PLACEHOLDER_SHORTCUT
            : picker->kind == PICKER_APPLICATION || picker->kind == PICKER_EXTENSION_APPLICATION
                ? PLACEHOLDER_APPLICATION
                : PLACEHOLDER_IMAGE;
        draw_theme_icon(&app->themes.active, top, 34, 76, 48, icon, accent);
    }
    draw_text_wrapped(
        top, 116, 55,
        (picker->kind == PICKER_TOP_BACKGROUND || picker->kind == PICKER_BOTTOM_BACKGROUND)
            ? "Pick a PNG or JPEG. Center-cropped and dimmed to keep text readable."
        : picker->kind == PICKER_ICON
            ? "Choose a 32x32 PNG icon, up to 64 KiB. Browser icons and cover art stay separate."
        : picker->kind == PICKER_CONFIGURATION
            ? "Check a .dsconfig archive, then A again to import. Current settings are backed up. "
              "Apps and saves stay in place."
        : picker->kind == PICKER_REPAIR
            ? "Choose a replacement file, or Y for the current folder. No files are moved."
        : picker->kind == PICKER_AUDIO ? "Pick a mono PCM16 WAV track. Music loops in the launcher."
        : picker->kind == PICKER_COVER ? "Pick a PNG or JPEG for this item. Images stay in place."
        : picker->kind == PICKER_EXTENSION_APPLICATION
            ? "Choose the app for this extension in every folder. Overrides folder rules."
            : "Pick a homebrew app. This file becomes argv[1].",
        text, 18, 8);
    draw_text_wrapped(top, 10, 163, "START cancels. B opens the parent folder.", muted, 33, 2);
    draw_text(bottom, 10, 7,
              (picker->kind == PICKER_TOP_BACKGROUND || picker->kind == PICKER_BOTTOM_BACKGROUND)
                  ? "Choose wallpaper"
              : picker->kind == PICKER_ICON          ? "Choose browser icon"
              : picker->kind == PICKER_CONFIGURATION ? "Choose configuration"
              : picker->kind == PICKER_REPAIR        ? "Locate replacement"
              : picker->kind == PICKER_AUDIO         ? "Choose music"
              : picker->kind == PICKER_COVER         ? "Choose cover"
                                                     : "Choose application",
              text, 30);
    draw_text(bottom, 210, 7, "< >", muted, 5);
    draw_text_scrolling(bottom, 10, 24, picker->path, muted, 231, picker->idle_frames,
                        !app->reduced_motion);
    for (int row = 0; row < PICKER_VISIBLE_ROWS; row++) {
        int index = picker->scroll + row;
        if (index >= picker->count)
            break;
        int y = PICKER_TOP + row * PICKER_ROW_HEIGHT;
        bool selected = index == picker->selected;
        if (selected) {
            draw_theme_selection(&app->themes.active, bottom, 6, y, 244, PICKER_ROW_HEIGHT);
        }
        const Entry *entry = &picker->entries[index];
        draw_theme_icon(&app->themes.active, bottom, 14, y + 3, 12, entry_file_type(entry).icon,
                        selected ? accent : muted);
        draw_text_scrolling(bottom, 36, y + 3, entry->name,
                            selected ? themes_selection_text(&app->themes.active) : muted, 203,
                            picker->idle_frames, selected && !app->reduced_motion);
    }
    if (!picker->count)
        draw_text(bottom, 14, 62,
                  picker->directory ? "Reading folder..."
                  : picker->failed  ? "Folder unavailable"
                                    : "No matching files",
                  muted, 30);
    char batch[64];
    snprintf(batch, sizeof batch, "Batch %u  L/R batches  A choose", picker->batch + 1);
    draw_text_clipped(bottom, 10, 174,
                      *picker->status                        ? picker->status
                      : (picker->batch || picker->truncated) ? batch
                      : picker->kind == PICKER_REPAIR        ? "A open/file  Y folder  B parent"
                                                             : "A choose  B parent  START cancel",
                      muted, 33);
}
