#include "artwork.h"
#include "image_decode.h"
#include "paths.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    unsigned before;
    Artwork image;
    unsigned after;
} output;
int main(int argc, char **argv)
{
    assert(argc == 2);
    char path[PATH_CAP];
    output.before = output.after = 0xCAFE1234;
    assert(artwork_decode_image(&output.image, "tests/fixtures/cover.jpg"));
    assert(output.image.ready && output.image.width <= 96 && output.image.height <= 104);
    for (int version = 3; version <= 4; version++) {
        char name[32];
        snprintf(name, sizeof name, "album-v%d.mp3", version);
        assert(path_join(path, argv[1], name));
        memset(&output.image, 0, sizeof output.image);
        assert(embedded_album_art(&output.image, path));
        assert(output.image.ready && output.image.width > 0);
        artwork_cache_clear();
        assert(artwork_cached_read(&output.image, path, true));
        assert(artwork_cache_decodes() == 1);
        assert(artwork_cached_read(&output.image, path, true));
        assert(artwork_cache_decodes() == 1);
        assert(output.before == 0xCAFE1234 && output.after == 0xCAFE1234);
        snprintf(name, sizeof name, "invalid-v%d.mp3", version);
        assert(path_join(path, argv[1], name));
        assert(!embedded_album_art(&output.image, path));
    }
    const unsigned char invalid[24] = {255, 216};
    assert(!image_decode(&output.image, invalid, sizeof invalid));
    puts("PASS: bounded JPEG decode, ID3v2.3/2.4 PNG/JPEG album artwork and malformed tag "
         "rejection");
}
