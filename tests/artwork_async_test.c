/* Exercise actual cooperative contexts separately from ASan, whose stack
 * instrumentation does not support makecontext/swapcontext completely. */
#include "artwork.h"
#include "artwork_disk_cache.h"
#include "image_job.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Artwork expected, actual;

static unsigned finish(void)
{
    unsigned steps = 0;
    while (artwork_cache_pending()) {
        artwork_cache_update();
        assert(++steps < 20000);
    }
    return steps;
}
int main(int argc, char **argv)
{
    assert(argc == 3);
    artwork_disk_cache_configure(NULL, false);
    assert(image_job_supported());
    for (int i = 1; i < argc; i++) {
        artwork_cache_enable_async(false);
        artwork_cache_clear();
        assert(artwork_cached_read(&expected, argv[i], false));
        artwork_cache_enable_async(true);
        artwork_cache_clear();
        memset(&actual, 0, sizeof actual);
        assert(!artwork_cached_read(&actual, argv[i], false));
        assert(artwork_cache_pending());
        /* A second source cannot replace the decoder's path/stack. */
        assert(!image_job_begin(argv[i], false));
        assert(finish() > 1);
        assert(image_job_yields() > 0);
        assert(artwork_cached_read(&actual, argv[i], false));
        assert(actual.width == expected.width && actual.height == expected.height);
        assert(!memcmp(actual.pixels, expected.pixels,
                       actual.width * actual.height * sizeof actual.pixels[0]));
        /* Clear/rescan during decode: finish cleanup but discard its output. */
        artwork_cache_clear();
        assert(!artwork_cached_read(&actual, argv[i], false));
        artwork_cache_clear();
        finish();
        unsigned decodes = artwork_cache_decodes();
        assert(!artwork_cached_read(&actual, argv[i], false));
        assert(artwork_cache_decodes() == decodes + 1);
        finish();
        assert(artwork_cached_read(&actual, argv[i], false));
    }
    assert(!image_job_begin("", false));
    assert(!image_job_take());
    puts("PASS: cooperative PNG/JPEG slices match synchronous pixels; single ownership, "
         "cache-clear invalidation and completion cleanup");
}
