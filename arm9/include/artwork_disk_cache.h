/* Disposable thumbnails. Source artwork is never modified. */
#ifndef ARTWORK_DISK_CACHE_H
#define ARTWORK_DISK_CACHE_H
#include "types.h"
#include <sys/stat.h>

void artwork_disk_cache_configure(const char *root, bool enabled);
bool artwork_disk_cache_read(Artwork *artwork, const char *source, bool embedded,
                             const struct stat *status);
void artwork_disk_cache_write(const Artwork *artwork, const char *source, bool embedded,
                              const struct stat *status);
bool artwork_disk_cache_clear(void);
#endif
