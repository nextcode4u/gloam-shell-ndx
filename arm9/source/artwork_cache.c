/* Four prepared images in RAM, with optional bounded disposable disk thumbnails. */
#include "artwork.h"
#include "artwork_disk_cache.h"
#include "image_decode.h"
#include "image_job.h"
#include "paths.h"
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
    char path[PATH_CAP];
    off_t size;
    time_t modified;
    ino_t inode;
    bool occupied, embedded, pending;
    unsigned used;
    Artwork artwork;
} CachedArtwork;
static CachedArtwork cache[4];
static unsigned cache_tick, decodes, disk_hits, generation;
static unsigned job_generation, job_slot;
static bool asynchronous;

void artwork_cache_enable_async(bool enabled)
{
    asynchronous = enabled && image_job_supported();
}
bool artwork_cache_pending(void)
{
    return image_job_busy();
}
bool artwork_cache_update(void)
{
    image_job_step();
    const Artwork *result = image_job_take();
    if (!result)
        return false;
    if (job_generation != generation)
        return true; /* A rescan/cache clear invalidated this request. */
    CachedArtwork *entry = &cache[job_slot];
    entry->pending = false;
    entry->artwork = *result;
    if (result->ready) {
        struct stat status;
        if (!stat(entry->path, &status) && status.st_size == entry->size &&
            status.st_mtime == entry->modified && status.st_ino == entry->inode)
            artwork_disk_cache_write(result, entry->path, entry->embedded, &status);
        else
            entry->occupied = false;
    }
    return true;
}

void artwork_cache_cancel_pending(void)
{
    if (!image_job_busy())
        return;
    if (job_generation == generation)
        cache[job_slot].occupied = false;
    generation++;
    image_job_cancel();
}

void artwork_cache_clear(void)
{
    generation++;
    image_job_cancel();
    memset(cache, 0, sizeof cache);
    cache_tick = decodes = disk_hits = 0;
}
unsigned artwork_cache_decodes(void)
{
    return decodes;
}
unsigned artwork_cache_disk_hits(void)
{
    return disk_hits;
}
bool artwork_cached_read(Artwork *artwork, const char *path, bool embedded)
{
    struct stat status;
    if (stat(path, &status) || !S_ISREG(status.st_mode))
        return false;
    CachedArtwork *replacement = &cache[0];
    for (unsigned i = 0; i < sizeof cache / sizeof cache[0]; i++) {
        CachedArtwork *entry = &cache[i];
        if (entry->occupied && !strcmp(entry->path, path) && entry->embedded == embedded) {
            if (entry->size == status.st_size && entry->modified == status.st_mtime &&
                entry->inode == status.st_ino) {
                entry->used = ++cache_tick;
                if (entry->artwork.ready)
                    *artwork = entry->artwork;
                return entry->artwork.ready;
            }
            /* Replace this source in place, so an earlier revision cannot
             * survive elsewhere and become a false hit after another edit. */
            replacement = entry;
            break;
        }
        if (!entry->occupied || entry->used < replacement->used)
            replacement = entry;
    }
    if (image_job_busy())
        return false; /* Keep one owner of the decoder and its stack. */
    memset(replacement, 0, sizeof *replacement);
    path_copy(replacement->path, path, PATH_CAP);
    replacement->size = status.st_size;
    replacement->modified = status.st_mtime;
    replacement->inode = status.st_ino;
    replacement->embedded = embedded;
    replacement->occupied = true;
    replacement->used = ++cache_tick;
    if (artwork_disk_cache_read(&replacement->artwork, path, embedded, &status)) {
        disk_hits++;
        *artwork = replacement->artwork;
        return true;
    }
    decodes++;
    if (asynchronous) {
        job_slot = (unsigned)(replacement - cache);
        job_generation = generation;
        replacement->pending = image_job_begin(path, embedded);
        return false;
    }
    bool ready = embedded ? embedded_album_art(&replacement->artwork, path)
                          : artwork_decode_image(&replacement->artwork, path);
    replacement->artwork.ready = ready;
    if (ready) {
        artwork_disk_cache_write(&replacement->artwork, path, embedded, &status);
        *artwork = replacement->artwork;
    }
    return ready;
}
