#ifndef FRONTEND_ARTWORK_H
#define FRONTEND_ARTWORK_H
#include "types.h"
/* Resolve artwork for the current selection. Called after selection settles. */
bool artwork_decode_image(Artwork *artwork, const char *path);
void artwork_load(App *app);
void artwork_shelf_tick(App *app);
/* Four prepared images in RAM, validated against source file metadata. */
bool artwork_cached_read(Artwork *artwork, const char *path, bool embedded);
void artwork_cache_clear(void);
void artwork_cache_cancel_pending(void);
/* Native frame loop resumes one bounded decoder slice. Source files are
 * read in chunks between decoder slices; stale jobs cannot publish after a
 * cache clear, and cancelled readers consume no further source bytes. */
void artwork_cache_enable_async(bool enabled);
bool artwork_cache_update(void);
bool artwork_cache_pending(void);
unsigned artwork_cache_decodes(void);
unsigned artwork_cache_disk_hits(void);
#endif
