/* Visible-row banner cache. Rendering never opens files. */
#include "icons.h"
#include "associations.h"
#include "image_decode.h"
#include "image_input.h"
#include "item_preferences.h"
#include "layout.h"
#include "paths.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

bool icon_is_ds_title(const char *path)
{
    const char *extension = path_extension(path);
    return !strcasecmp(extension, "nds") || !strcasecmp(extension, "dsi") ||
           !strcasecmp(extension, "srl") || !strcasecmp(extension, "app") ||
           !strcasecmp(extension, "dsi_mode");
}

bool icon_load_png(const char *path, uint16_t pixels[32 * 32])
{
    return icon_load_png_coverage(path, pixels, NULL);
}

bool icon_load_png_coverage(const char *path, uint16_t pixels[32 * 32], uint8_t coverage[256])
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    size_t bytes;
    bool valid = image_file_size(file, 24, 65536, &bytes);
    unsigned char *data = valid ? malloc((size_t)bytes) : NULL;
    valid = data && fread(data, 1, (size_t)bytes, file) == (size_t)bytes &&
            image_decode_component(pixels, coverage, data, (size_t)bytes);
    free(data);
    fclose(file);
    return valid;
}

/* Size metadata avoids a full FAT cluster-chain walk just to reach EOF. */
static bool read_banner(FILE *file, unsigned char banner[0x240], uint32_t *offset,
                        uint64_t *available)
{
    struct stat metadata;
    unsigned char header[0x6c];
    if (fstat(fileno(file), &metadata) || !S_ISREG(metadata.st_mode) || metadata.st_size < 0x160 ||
        fread(header, 1, sizeof header, file) != sizeof header)
        return false;
    *offset = header[0x68] | (uint32_t)header[0x69] << 8 | (uint32_t)header[0x6a] << 16 |
              (uint32_t)header[0x6b] << 24;
    uint64_t size = (uint64_t)metadata.st_size;
    if (*offset < 0x160 || *offset > size || *offset > LONG_MAX || size - *offset < 0x840)
        return false;
    *available = size - *offset;
    return !fseek(file, (long)*offset, SEEK_SET) && fread(banner, 1, 0x240, file) == 0x240;
}
static bool load_title_data(const char *path, uint16_t pixels[32 * 32], BrowserIcon *animation)
{
    if (animation) {
        animation->animation_count = 0;
        animation->animation_pending = false;
    }
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char banner[0x240];
    uint32_t offset;
    uint64_t available;
    bool valid = read_banner(file, banner, &offset, &available) &&
                 icon_decode_banner(banner, sizeof banner, pixels);
    if (valid && animation && banner[0] == 3 && banner[1] == 1 && available >= 0x23c0 &&
        (uint64_t)offset + 0x1240 <= LONG_MAX) {
        unsigned char data[0x1180];
        if (!fseek(file, (long)offset + 0x1240, SEEK_SET) &&
            fread(data, 1, sizeof data, file) == sizeof data)
            icon_decode_animation(banner, data, sizeof data, animation);
    }
    fclose(file);
    return valid;
}
bool icon_load_banner(const char *path, uint16_t pixels[32 * 32])
{
    return load_title_data(path, pixels, NULL);
}
bool icon_load_title(const char *path, BrowserIcon *icon)
{
    return load_title_data(path, icon->pixels, icon);
}
bool icon_load_animation(const char *path, BrowserIcon *icon)
{
    return icon_load_title(path, icon) && icon->animation_count != 0;
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
    icons_rebind(app);
}

void icons_rebind(App *app)
{
    item_preferences_display_clear();
    for (int index = 0; index < ENTRY_CAP; index++) {
        app->entry_icons[index] = -1;
        app->banner_snapshots[index].ready = false;
    }
    app->icons_idle_frames = 0;
    /* An explicit refresh clears occupied flags. Ordinary tab/folder changes
     * can reconnect known assignments without reopening their INI or banner. */
    for (int entry = 0; entry < app->entry_count; entry++) {
        if (!*app->entries[entry].path)
            continue;
        for (int slot = 0; slot < ICON_CACHE_CAP; slot++) {
            BrowserIcon *icon = &app->icons[slot];
            if (icon->occupied && icon->ready &&
                !strcmp(icon->content_path, app->entries[entry].path)) {
                app->entry_icons[entry] = slot;
                break;
            }
        }
    }
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

static BannerSnapshot *entry_snapshot(App *app, int index, const char *path)
{
    const Entry *entry = &app->entries[index];
    bool direct = (entry->kind == ITEM_FILE || entry->kind == ITEM_INSTALLED_TITLE) &&
                  !strcmp(path, entry->path);
    bool system = entry->kind == ITEM_SYSTEM && entry->target >= 0 && entry->target < 5;
    return direct || system ? &app->banner_snapshots[index] : NULL;
}

/* Show the small static image first. Animation data gets a later idle turn,
 * rather than holding every visible icon behind its larger payload and CRC. */
static bool load_browser_title(App *app, int index, const char *path, BrowserIcon *icon)
{
    BannerSnapshot *snapshot = entry_snapshot(app, index, path);
    unsigned char banner[0x240];
    uint32_t offset;
    uint64_t size;
    if (snapshot && snapshot->ready) {
        memcpy(banner, snapshot->banner, sizeof banner);
        offset = snapshot->offset;
        size = snapshot->file_size;
        app->icon_snapshot_hits++;
    } else {
        app->icon_reads++;
        FILE *file = fopen(path, "rb");
        if (!file)
            return false;
        uint64_t available = 0;
        bool valid = read_banner(file, banner, &offset, &available);
        if (fclose(file))
            valid = false;
        if (!valid)
            return false;
        size = (uint64_t)offset + available;
    }
    if (!icon_decode_banner(banner, sizeof banner, icon->pixels))
        return false;
    if (snapshot && !snapshot->ready) {
        memcpy(snapshot->banner, banner, sizeof banner);
        snapshot->offset = offset;
        snapshot->file_size = size;
        snapshot->ready = true;
    }
    icon->banner_offset = offset;
    icon->source_size = size;
    icon->animation_crc = banner[8] | (uint16_t)banner[9] << 8;
    icon->animation_pending = banner[0] == 3 && banner[1] == 1 && size - offset >= 0x23c0 &&
                              (uint64_t)offset + 0x1240 <= LONG_MAX;
    return true;
}

static void load_browser_animation(App *app, BrowserIcon *icon)
{
    icon->animation_pending = false; /* A damaged/missing animation is not retried each frame. */
    app->icon_reads++;
    app->icon_animation_reads++;
    FILE *file = fopen(icon->path, "rb");
    if (!file)
        return;
    struct stat status;
    unsigned char data[0x1180], header[16] = {3, 1};
    bool valid = !fstat(fileno(file), &status) && S_ISREG(status.st_mode) && status.st_size >= 0 &&
                 (uint64_t)status.st_size == icon->source_size &&
                 !fseek(file, (long)icon->banner_offset + 0x1240, SEEK_SET) &&
                 fread(data, 1, sizeof data, file) == sizeof data && !ferror(file);
    if (fclose(file))
        valid = false;
    header[8] = icon->animation_crc & 255;
    header[9] = icon->animation_crc >> 8;
    if (valid)
        icon_decode_animation(header, data, sizeof data, icon);
}

static bool load_path(App *app, int index, const char *path, bool png, int system_target)
{
    int slot = 0;
    for (int i = 0; i < ICON_CACHE_CAP; i++) {
        if (app->icons[i].occupied && !strcmp(app->icons[i].path, path)) {
            app->entry_icons[index] = i;
            path_copy(app->icons[i].content_path, app->entries[index].path, PATH_CAP);
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
    path_copy(icon->content_path, app->entries[index].path, PATH_CAP);
    icon->occupied = true;
    icon->last_used = app->frame_count;
    icon->animation_count = 0;
    icon->animation_pending = false;
    icon->ready = false;
    icon->system_fallback = false;
    if (strncmp(path, "system:", 7)) {
        if (png) {
            app->icon_reads++;
            icon->ready = icon_load_png(path, icon->pixels);
        } else {
            icon->ready = load_browser_title(app, index, path, icon);
        }
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
    if (app->banner_snapshots[index].ready &&
        (entry->kind == ITEM_FILE || entry->kind == ITEM_INSTALLED_TITLE)) {
        load_path(app, index, entry->path, false, -1);
        return;
    }
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

bool icons_tick(App *app)
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
         app->show_covers))
        return false;
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
            app->icons[slot].last_used = app->frame_count;
            icon_animate(&app->icons[slot], app->reduced_motion);
            animated[slot] = true;
        }
    }
    if (app->navigation_held) {
        app->icons_idle_frames = 0;
        return false;
    }
    if (++app->icons_idle_frames < UI_ICON_IDLE_FRAMES)
        return false;
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
        return true;
    }
    if (!app->reduced_motion) {
        for (int pass = -1; pass < last - first; pass++) {
            int index = pass < 0 ? location->selected : first + pass;
            if (index < first || index >= last)
                continue;
            int slot = app->entry_icons[index];
            if (slot >= 0 && app->icons[slot].ready && app->icons[slot].animation_pending) {
                load_browser_animation(app, &app->icons[slot]);
                return true;
            }
        }
    }
    return false;
}
