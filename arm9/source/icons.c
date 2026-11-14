/* Visible-row banner cache. Rendering never opens files. */
#include "icons.h"
#include "associations.h"
#include "image_decode.h"
#include "layout.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

bool icon_is_ds_title(const char *path)
{
    const char *extension = path_extension(path);
    return !strcasecmp(extension, "nds") || !strcasecmp(extension, "dsi") ||
           !strcasecmp(extension, "srl") || !strcasecmp(extension, "app") ||
           !strcasecmp(extension, "dsi_mode");
}

bool icon_load_png(const char *path, uint16_t pixels[32 * 32])
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    bool valid = !fseek(file, 0, SEEK_END);
    long bytes = valid ? ftell(file) : -1;
    valid = valid && bytes >= 24 && bytes <= 65536 && !fseek(file, 0, SEEK_SET);
    unsigned char *data = valid ? malloc((size_t)bytes) : NULL;
    valid = data && fread(data, 1, (size_t)bytes, file) == (size_t)bytes &&
            image_decode_icon(pixels, data, (size_t)bytes);
    free(data);
    fclose(file);
    return valid;
}

bool icon_load_banner(const char *path, uint16_t pixels[32 * 32])
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[0x6c], banner[0x240];
    bool valid = fread(header, 1, sizeof header, file) == sizeof header;
    uint32_t offset = valid ? header[0x68] | ((uint32_t)header[0x69] << 8) |
                                  ((uint32_t)header[0x6a] << 16) | ((uint32_t)header[0x6b] << 24)
                            : 0;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long size = ftell(file);
    valid = valid && size >= 0 && offset >= 0x160 && offset <= (uint32_t)size &&
            (uint32_t)size - offset >= 0x840 && fseek(file, (long)offset, SEEK_SET) == 0 &&
            fread(banner, 1, sizeof banner, file) == sizeof banner;
    fclose(file);
    if (!valid)
        return false;
    return icon_decode_banner(banner, sizeof banner, pixels);
}

bool icon_decode_banner(const void *data, size_t bytes, uint16_t pixels[32 * 32])
{
    if (!data || bytes < 0x240)
        return false;
    const unsigned char *banner = data;
    unsigned version = banner[0] | (banner[1] << 8);
    if (version != 1 && version != 2 && version != 3 && version != 0x103)
        return false;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int tile = (y / 8) * 4 + x / 8;
            int byte = 0x20 + tile * 32 + (y % 8) * 4 + (x % 8) / 2;
            unsigned index = (banner[byte] >> ((x & 1) * 4)) & 15;
            /* Palette index zero is transparent on any selection/background. */
            pixels[y * 32 + x] =
                index ? 0x8000 | banner[0x220 + index * 2] | (banner[0x221 + index * 2] << 8) : 0;
        }
    }
    return true;
}

void icons_reset(App *app)
{
    for (int index = 0; index < ICON_CACHE_CAP; index++)
        app->icons[index].occupied = false;
    for (int index = 0; index < ENTRY_CAP; index++)
        app->entry_icons[index] = -1;
    app->icons_idle_frames = 0;
}

const BrowserIcon *icons_for_entry(const App *app, int entry_index)
{
    if (entry_index < 0 || entry_index >= app->entry_count)
        return NULL;
    const Entry *entry = &app->entries[entry_index];
    if (entry->kind == ITEM_SYSTEM && entry->target == 6 && app->slot1.icon.ready)
        return &app->slot1.icon;
    int slot = app->entry_icons[entry_index];
    return slot >= 0 && slot < ICON_CACHE_CAP && app->icons[slot].ready ? &app->icons[slot] : NULL;
}

static bool load_path(App *app, int index, const char *path, bool png, int system_target)
{
    int slot = 0;
    for (int i = 0; i < ICON_CACHE_CAP; i++) {
        if (app->icons[i].occupied && !strcmp(app->icons[i].path, path)) {
            app->entry_icons[index] = i;
            app->icons[i].last_used = app->frame_count;
            return app->icons[i].ready;
        }
        if (!app->icons[i].occupied || app->icons[i].last_used < app->icons[slot].last_used)
            slot = i;
    }
    /* Unlink every old reference before reusing a slot. */
    for (int i = 0; i < app->entry_count; i++)
        if (app->entry_icons[i] == slot)
            app->entry_icons[i] = -1;
    BrowserIcon *icon = &app->icons[slot];
    path_copy(icon->path, path, sizeof icon->path);
    icon->occupied = true;
    icon->last_used = app->frame_count;
    icon->animation_count = 0;
    icon->ready = false;
    icon->system_fallback = false;
    if (strncmp(path, "system:", 7)) {
        app->icon_reads++;
        icon->ready =
            png ? icon_load_png(path, icon->pixels) : icon_load_banner(path, icon->pixels);
        if (icon->ready && !png)
            icon_load_animation(path, icon);
    }
    if (!icon->ready && system_target >= 0) {
        icon->ready = icon_system_fallback(system_target, icon->pixels);
        icon->system_fallback = icon->ready;
    }
    app->entry_icons[index] = slot;
    return icon->ready;
}
static void load_entry_icon(App *app, int index)
{
    const Entry *entry = &app->entries[index];
    char path[PATH_CAP];
    if (entry->kind == ITEM_INSTALLED_TITLE) {
        load_path(app, index, entry->path, false, -1);
        return;
    }
    if (entry->kind == ITEM_SYSTEM) {
        int target = entry->target;
        if (target >= 0 && target < 5 && app->system_catalog.titles[target].available &&
            *app->system_catalog.content_paths[target])
            path_copy(path, app->system_catalog.content_paths[target], sizeof path);
        else
            snprintf(path, sizeof path, "system:%d", target);
        load_path(app, index, path, false, target);
        return;
    }
    if (!association_icon_source(app, entry, path)) {
        app->entry_icons[index] = -2;
        return;
    }
    bool png = !strcasecmp(path_extension(path), "png");
    if (!load_path(app, index, path, png, -1) && png && entry->kind == ITEM_FILE &&
        association_icon_application(app, entry->path, path))
        load_path(app, index, path, false, -1);
}

void icons_tick(App *app)
{
    int selection = app->locations[app->category].selected;
    const Entry *selected =
        selection >= 0 && selection < app->entry_count ? &app->entries[selection] : NULL;
    bool system_selected =
        selected && (selected->kind == ITEM_SYSTEM || selected->kind == ITEM_INSTALLED_TITLE);
    /* Text-only lists still need the selected title's banner for the top preview. */
    bool title_preview =
        selected && selected->kind == ITEM_FILE && icon_is_ds_title(selected->path);
    if ((!app->storage_ready && !system_selected) ||
        (!layout_has_icons(app) && !system_selected && !title_preview && !app->detail_tab &&
         app->show_covers) ||
        ++app->icons_idle_frames < 2)
        return;
    const Location *location = &app->locations[app->category];
    int first = location->scroll, last = first + layout_visible_rows(app);
    if (!layout_has_icons(app)) {
        first = location->selected;
        last = first + 1;
    }
    if (last > app->entry_count)
        last = app->entry_count;
    bool animated[ICON_CACHE_CAP] = {false};
    for (int i = first; i < last; i++)
        if (app->entries[i].kind == ITEM_SYSTEM && app->entries[i].target == 6 &&
            app->slot1.icon.ready) {
            icon_animate(&app->slot1.icon, app->reduced_motion);
            break;
        }
    for (int i = first; i < last; i++) {
        int slot = app->entry_icons[i];
        if (slot >= 0 && !animated[slot]) {
            icon_animate(&app->icons[slot], app->reduced_motion);
            animated[slot] = true;
        }
    }
    /* Prioritize the selection, then fill the remaining visible rows. */
    for (int pass = -1; pass < last - first; pass++) {
        int index = pass < 0 ? location->selected : first + pass;
        if (index < first || index >= last)
            continue;
        if (app->entry_icons[index] >= 0) {
            app->icons[app->entry_icons[index]].last_used = app->frame_count;
            continue;
        }
        if (app->entry_icons[index] == -2 ||
            (app->entries[index].kind != ITEM_FILE && app->entries[index].kind != ITEM_DIRECTORY &&
             app->entries[index].kind != ITEM_SYSTEM &&
             app->entries[index].kind != ITEM_INSTALLED_TITLE))
            continue;
        load_entry_icon(app, index);
        return;
    }
}
