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
/* Keep valid JPEG headers/dimensions but remove every compressed scan. */
static void reject_empty_jpeg(const char *path)
{
    unsigned char jpeg[65536];
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t size = fread(jpeg, 1, sizeof jpeg, file);
    assert(!ferror(file) && feof(file) && !fclose(file));
    assert(size > 4 && jpeg[0] == 0xff && jpeg[1] == 0xd8);
    size_t offset = 2;
    while (offset + 4 < size && jpeg[offset] == 0xff && jpeg[offset + 1] != 0xda) {
        size_t length = (size_t)jpeg[offset + 2] * 256 + jpeg[offset + 3];
        assert(length >= 2 && length <= size - offset - 2);
        offset += 2 + length;
    }
    assert(offset + 2 < size && jpeg[offset] == 0xff && jpeg[offset + 1] == 0xda);
    jpeg[offset + 1] = 0xd9;
    assert(!image_decode(&output.image, jpeg, offset + 2));
    assert(output.before == 0xCAFE1234 && output.after == 0xCAFE1234);
}

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
    reject_empty_jpeg("tests/fixtures/cover.jpg");
    reject_empty_jpeg("tests/fixtures/async-progressive.jpg");
    const unsigned char invalid[24] = {255, 216};
    assert(!image_decode(&output.image, invalid, sizeof invalid));
    puts("PASS: bounded JPEG decode, ID3v2.3/2.4 PNG/JPEG album artwork and malformed tag "
         "rejection");
}
