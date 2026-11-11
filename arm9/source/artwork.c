#include "artwork.h"
#include "app.h"
#include "associations.h"
#include "browser.h"
#include "icons.h"
#include "image_decode.h"
#include "image_input.h"
#include "item_preferences.h"
#include "paths.h"
#include "text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint16_t pack_rgb555(unsigned r, unsigned g, unsigned b)
{
    return 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
}
static uint32_t read_le32(const unsigned char *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
bool artwork_decode_image(Artwork *artwork, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    bool valid = !fseek(file, 0, SEEK_END);
    long size = valid ? ftell(file) : -1;
    valid = size > 0 && size <= 1024 * 1024 && !fseek(file, 0, SEEK_SET);
    unsigned char *data = valid ? malloc((size_t)size) : NULL;
    valid = valid && data && image_read_exact(file, data, (size_t)size);
    fclose(file);
    bool decoded = valid && image_decode(artwork, data, (size_t)size);
    free(data);
    return decoded;
}
static bool load_nds_banner(App *app, const Entry *entry, Artwork *artwork, bool metadata)
{
    if (!icon_is_ds_title(entry->path))
        return false;
    FILE *f = fopen(entry->path, "rb");
    if (!f)
        return false;
    unsigned char data[0x840], header[0x160];
    if (fread(header, 1, sizeof header, f) != sizeof header) {
        fclose(f);
        return false;
    }
    if (metadata) {
        memcpy(app->code, header + 12, 4);
        app->code[4] = 0;
    }
    uint32_t offset = read_le32(header + 0x68);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (offset < 0x160 || offset > (uint32_t)size || size - (long)offset < (long)sizeof data) {
        fclose(f);
        return false;
    }
    fseek(f, (long)offset, SEEK_SET);
    bool ok = fread(data, 1, sizeof data, f) == sizeof data;
    fclose(f);
    if (!ok)
        return false;
    if (metadata)
        text_from_utf16le(app->title, sizeof app->title, data + 0x340, 128);
    for (int y = 0; y < 32; y++)
        for (int x2 = 0; x2 < 32; x2++) {
            int i = (y / 8) * 4 + x2 / 8, byte = 0x20 + i * 32 + (y % 8) * 4 + (x2 % 8) / 2;
            unsigned idx = (data[byte] >> ((x2 & 1) * 4)) & 15;
            artwork->pixels[y * 32 + x2] =
                idx ? (uint16_t)(data[0x220 + idx * 2] | (data[0x221 + idx * 2] << 8) | 0x8000)
                    : pack_rgb555(19, 24, 34);
        }
    artwork->width = artwork->height = 32;
    artwork->ready = true;
    snprintf(artwork->source, sizeof artwork->source, "NDS banner");
    return true;
}
static bool prepare_artwork(Artwork *artwork, bool *resolved, const char *path, bool embedded)
{
    if (artwork_cached_read(artwork, path, embedded))
        return true;
    if (artwork_cache_pending()) {
        *resolved = false;
        return true;
    }
    return false;
}

static void artwork_load_entry(App *app, const Entry *entry, Artwork *artwork, bool *resolved,
                               bool metadata)
{
    artwork->ready = false;
    *resolved = true;
    if (!entry || !entry->path[0] || !app->storage_ready)
        return;
    if (entry->kind == ITEM_SYSTEM || entry->kind == ITEM_CATEGORY ||
        entry->kind == ITEM_INSTALLED_TITLE || entry->kind == ITEM_INSTALLED_LIBRARY)
        return;
    char path[PATH_CAP];
    app->artwork_reads++;
    ItemPreferences preferences;
    if (item_preferences_load(app->root, entry->path, &preferences) && *preferences.cover &&
        prepare_artwork(artwork, resolved, preferences.cover, false)) {
        snprintf(artwork->source, sizeof artwork->source, "Assigned image");
        return;
    }
    if (entry->kind == ITEM_FILE &&
        snprintf(path, sizeof path, "%s.png", entry->path) < (int)sizeof path &&
        prepare_artwork(artwork, resolved, path, false)) {
        snprintf(artwork->source, sizeof artwork->source, "Sidecar PNG");
        return;
    }
    if (entry->kind == ITEM_FILE) {
        const char *suffixes[] = {".jpg", ".jpeg"};
        for (int i = 0; i < 2; i++)
            if (snprintf(path, sizeof path, "%s%s", entry->path, suffixes[i]) < (int)sizeof path &&
                prepare_artwork(artwork, resolved, path, false)) {
                snprintf(artwork->source, sizeof artwork->source, "Sidecar JPEG");
                return;
            }
        if (!strcasecmp(path_extension(entry->path), "mp3") &&
            prepare_artwork(artwork, resolved, entry->path, true)) {
            snprintf(artwork->source, sizeof artwork->source, "Embedded album art");
            return;
        }
    }
    snprintf(path, sizeof path, "%s", entry->path);
    if (entry->kind != ITEM_DIRECTORY) {
        char *slash = strrchr(path, '/');
        if (slash)
            *slash = 0;
    }
    size_t len = strlen(path);
    if (len + 11 < sizeof path) {
        strcat(path, "/cover.png");
        if (prepare_artwork(artwork, resolved, path, false)) {
            snprintf(artwork->source, sizeof artwork->source, "Folder cover");
            return;
        }
    }
    const char *covers[] = {"/cover.jpg", "/cover.jpeg"};
    char folder[PATH_CAP];
    path_copy(folder, entry->path, sizeof folder);
    if (entry->kind != ITEM_DIRECTORY) {
        char *slash = strrchr(folder, '/');
        if (slash)
            *slash = 0;
    }
    for (int i = 0; i < 2; i++)
        if (snprintf(path, sizeof path, "%s%s", folder, covers[i]) < (int)sizeof path &&
            prepare_artwork(artwork, resolved, path, false)) {
            snprintf(artwork->source, sizeof artwork->source, "Folder cover");
            return;
        }
    if (entry->kind == ITEM_FILE) {
        const char *cover = association_cover(app, entry->path);
        if (cover && prepare_artwork(artwork, resolved, cover, false)) {
            snprintf(artwork->source, sizeof artwork->source, "Association cover");
            return;
        }
        load_nds_banner(app, entry, artwork, metadata);
    }
}

void artwork_load(App *app)
{
    artwork_load_entry(app, app_selected(app), &app->artwork, &app->artwork_resolved, true);
}

/* Prepare at most one shelf entry per tick, after the selected preview. The
 * existing bounded decoder and four-image cache are shared with the top screen. */
void artwork_shelf_tick(App *app)
{
    if (app->browser_style != BROWSER_SHELF || !app->show_covers ||
        app->selection_idle_frames < 7 || (!app->artwork.ready && !app->artwork_resolved))
        return;
    const Location *location = &app->locations[app->category];
    for (int cell = 0; cell < 3; cell++) {
        int index = location->scroll + cell;
        if (index >= app->entry_count)
            break;
        if (app->shelf_resolved[cell])
            continue;
        if (index == location->selected) {
            app->shelf_artwork[cell] = app->artwork;
            app->shelf_resolved[cell] = true;
        } else {
            artwork_load_entry(app, &app->entries[index], &app->shelf_artwork[cell],
                               &app->shelf_resolved[cell], false);
        }
        return;
    }
}
