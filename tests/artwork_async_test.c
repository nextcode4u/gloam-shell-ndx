/* Exercise actual cooperative contexts separately from ASan, whose stack
 * instrumentation does not support makecontext/swapcontext completely. */
#include "artwork.h"
#include "artwork_disk_cache.h"
#include "image_job.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static Artwork expected, actual;
static unsigned metadata_checks;
int __real_stat(const char *, struct stat *);
int __wrap_stat(const char *path, struct stat *status)
{
    metadata_checks++;
    return __real_stat(path, status);
}

static unsigned finish_read(const char *path)
{
    unsigned steps = 0;
    while (artwork_cache_pending()) {
        unsigned before = metadata_checks;
        assert(!artwork_cached_read(&actual, path, false));
        assert(metadata_checks == before); /* Waiting must not reopen the source directory. */
        artwork_cache_update();
        assert(++steps < 20000);
    }
    return steps;
}

static unsigned finish(void)
{
    unsigned steps = 0;
    while (artwork_cache_pending()) {
        artwork_cache_update();
        assert(++steps < 20000);
    }
    return steps;
}
static void check_changed_source(const char *source)
{
    char path[] = "/tmp/gloam-art-pending-XXXXXX";
    int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    FILE *out = fdopen(descriptor, "wb"), *in = fopen(source, "rb");
    assert(out && in);
    unsigned char data[4096];
    size_t count;
    while ((count = fread(data, 1, sizeof data, in)))
        assert(fwrite(data, 1, count, out) == count);
    assert(!ferror(in));
    assert(!fclose(in) && !fclose(out));
    artwork_cache_clear();
    assert(!artwork_cached_read(&actual, path, false) && artwork_cache_pending());
    /* Completion must reject the old decode when its source changed while pending. */
    assert(!truncate(path, 0));
    finish_read(path);
    memset(&actual, 0, sizeof actual);
    assert(!artwork_cached_read(&actual, path, false) && !actual.ready);
    finish();
    artwork_cache_clear();
    finish();
    assert(!unlink(path));
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
        unsigned before = metadata_checks;
        assert(!artwork_cached_read(&actual, argv[i], false));
        assert(metadata_checks == before + 1);
        assert(artwork_cache_pending());
        /* A second source cannot replace the decoder's path/stack. */
        assert(!image_job_begin(argv[i], false));
        assert(finish_read(argv[i]) > 1);
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
    check_changed_source(argv[1]);
    assert(!image_job_begin("", false));
    assert(!image_job_take());
    puts("PASS: cooperative PNG/JPEG slices match synchronous pixels; single ownership, "
         "read-free pending polls, changed-source rejection, cache-clear invalidation and "
         "completion cleanup");
}
