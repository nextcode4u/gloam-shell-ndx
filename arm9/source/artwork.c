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
#include "top_content.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

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
    size_t size;
    bool valid = image_file_size(file, 1, 1024 * 1024, &size);
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
    struct stat metadata_info;
    bool sized = !fstat(fileno(f), &metadata_info) && S_ISREG(metadata_info.st_mode) &&
                 metadata_info.st_size >= 0 && (uint64_t)metadata_info.st_size <= UINT32_MAX;
    uint32_t size = sized ? (uint32_t)metadata_info.st_size : 0;
    if (!sized || offset < 0x160 || offset > size || size - offset < sizeof data) {
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

/* One optional source per step. A slow card must not pay for every missing
 * sidecar, folder cover and fallback in the same browser frame. */
static bool artwork_candidate(App *app, const Entry *entry, Artwork *artwork, bool *resolved,
                              bool metadata, unsigned stage)
{
    char path[PATH_CAP];
    const char *suffixes[] = {".png", ".jpg", ".jpeg"};
    if (!stage) {
        ItemPreferences preferences;
        if (item_preferences_display(app->root, entry->path, &preferences) && *preferences.cover &&
            prepare_artwork(artwork, resolved, preferences.cover, false)) {
            snprintf(artwork->source, sizeof artwork->source, "Assigned image");
            return true;
        }
    } else if (stage <= 3) {
        unsigned suffix = stage - 1;
        if (entry->kind == ITEM_FILE &&
            snprintf(path, sizeof path, "%s%s", entry->path, suffixes[suffix]) < (int)sizeof path &&
            prepare_artwork(artwork, resolved, path, false)) {
            snprintf(artwork->source, sizeof artwork->source,
                     suffix ? "Sidecar JPEG" : "Sidecar PNG");
            return true;
        }
    } else if (stage == 4) {
        if (entry->kind == ITEM_FILE && !strcasecmp(path_extension(entry->path), "mp3") &&
            prepare_artwork(artwork, resolved, entry->path, true)) {
            snprintf(artwork->source, sizeof artwork->source, "Embedded album art");
            return true;
        }
    } else if (stage <= 7) {
        char folder[PATH_CAP];
        path_copy(folder, entry->path, sizeof folder);
        if (entry->kind != ITEM_DIRECTORY) {
            char *slash = strrchr(folder, '/');
            if (slash)
                *slash = 0;
        }
        if (snprintf(path, sizeof path, "%s/cover%s", folder, suffixes[stage - 5]) <
                (int)sizeof path &&
            prepare_artwork(artwork, resolved, path, false)) {
            snprintf(artwork->source, sizeof artwork->source, "Folder cover");
            return true;
        }
    } else if (stage == 8) {
        const char *cover = entry->kind == ITEM_FILE ? association_cover(app, entry->path) : NULL;
        if (cover && prepare_artwork(artwork, resolved, cover, false)) {
            snprintf(artwork->source, sizeof artwork->source, "Association cover");
            return true;
        }
    } else {
        if (entry->kind == ITEM_FILE)
            load_nds_banner(app, entry, artwork, metadata);
        return true;
    }
    return false;
}

static void artwork_entry_step(App *app, const Entry *entry, Artwork *artwork, bool *resolved,
                               bool metadata, unsigned char *stage)
{
    artwork->ready = false;
    *resolved = true;
    if (!entry || !entry->path[0] || !app->storage_ready ||
        (entry->kind != ITEM_FILE && entry->kind != ITEM_DIRECTORY && entry->kind != ITEM_MISSING))
        return;
    if (!*stage)
        app->artwork_reads++;
    if (!artwork_candidate(app, entry, artwork, resolved, metadata, *stage)) {
        (*stage)++;
        *resolved = false;
    }
}

static void artwork_load_entry(App *app, const Entry *entry, Artwork *artwork, bool *resolved,
                               bool metadata)
{
    unsigned char stage = 0;
    do {
        unsigned char previous = stage;
        artwork_entry_step(app, entry, artwork, resolved, metadata, &stage);
        if (stage == previous)
            break; /* Resolved, or waiting for the shared image decoder. */
    } while (!*resolved);
}

void artwork_load(App *app)
{
    artwork_load_entry(app, app_selected(app), &app->artwork, &app->artwork_resolved, true);
}

void artwork_step(App *app)
{
    artwork_entry_step(app, app_selected(app), &app->artwork, &app->artwork_resolved, true,
                       &app->artwork_stage);
}

/* Three path-keyed slots survive selection changes. Explicit refreshes still
 * invalidate them, so editing a cover never leaves an old prepared image. */
static int shelf_slot(const App *app, const char *path)
{
    for (int slot = 0; slot < 3; slot++)
        if (!strcmp(app->shelf_paths[slot], path))
            return slot;
    return -1;
}

const Artwork *artwork_shelf_for_entry(const App *app, int index)
{
    if (index < 0 || index >= app->entry_count)
        return NULL;
    int slot = shelf_slot(app, app->entries[index].path);
    return slot >= 0 ? &app->shelf_artwork[slot] : NULL;
}

void artwork_shelf_tick(App *app)
{
    TopContentMode mode = top_content_mode(app);
    bool supplemental =
        mode == TOP_CONTENT_NEIGHBORS || mode == TOP_CONTENT_FOLDER || mode == TOP_CONTENT_ACTIVITY;
    if ((app->browser_style != BROWSER_SHELF && !supplemental) || !app->show_covers ||
        app->selection_idle_frames < 7 || (!app->artwork.ready && !app->artwork_resolved))
        return;
    int indexes[3];
    /* The bottom Shelf keeps its visible neighbors. Other top-screen cards
     * may reuse those images, or retain icons when they are outside the cache. */
    if (supplemental && app->browser_style != BROWSER_SHELF)
        top_content_samples(app, indexes);
    else
        for (int i = 0; i < 3; i++)
            indexes[i] = app->locations[app->category].selected + i - 1;
    for (int cell = 0; cell < 3; cell++) {
        int index = indexes[cell];
        if (index < 0 || index >= app->entry_count)
            continue;
        const char *path = app->entries[index].path;
        int slot = shelf_slot(app, path);
        if (slot < 0) {
            for (slot = 0; slot < 3; slot++) {
                bool visible = false;
                for (int i = 0; i < 3; i++)
                    if (indexes[i] >= 0 && indexes[i] < app->entry_count &&
                        !strcmp(app->shelf_paths[slot], app->entries[indexes[i]].path))
                        visible = true;
                if (!visible)
                    break;
            }
            if (slot == 3)
                return;
            path_copy(app->shelf_paths[slot], path, PATH_CAP);
            app->shelf_resolved[slot] = false;
            app->shelf_stage[slot] = 0;
            app->shelf_artwork[slot].ready = false;
        }
        if (app->shelf_resolved[slot])
            continue;
        if (index == app->locations[app->category].selected) {
            app->shelf_artwork[slot] = app->artwork;
            app->shelf_resolved[slot] = true;
        } else {
            artwork_entry_step(app, &app->entries[index], &app->shelf_artwork[slot],
                               &app->shelf_resolved[slot], false, &app->shelf_stage[slot]);
        }
        return;
    }
}
