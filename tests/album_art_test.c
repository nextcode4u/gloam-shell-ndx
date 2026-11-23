#include "artwork.h"
#include "image_decode.h"
#include "paths.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *watched_path;
static FILE *tag_file;
static bool short_read, close_failure, metadata_failure, watch_allocations;
static unsigned tag_allocations, end_seeks;
static size_t allocated_bytes, read_bytes;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
int __real_fstat(int, struct stat *);
int __real_fseek(FILE *, long, int);
void *__real_malloc(size_t);
size_t __real_fread(void *, size_t, size_t, FILE *);
#ifdef __GLIBC__
size_t __real___fread_chk(void *, size_t, size_t, size_t, FILE *);
#endif
FILE *__wrap_fopen(const char *path, const char *mode)
{
    FILE *file = __real_fopen(path, mode);
    if (watched_path && !strcmp(path, watched_path) && mode[0] == 'r')
        tag_file = file;
    return file;
}
int __wrap_fclose(FILE *file)
{
    bool fail = file == tag_file && close_failure;
    if (file == tag_file)
        tag_file = NULL;
    int result = __real_fclose(file);
    if (fail) {
        errno = EIO;
        return EOF;
    }
    return result;
}
int __wrap_fstat(int descriptor, struct stat *status)
{
    if (tag_file && fileno(tag_file) == descriptor && metadata_failure) {
        errno = EIO;
        return -1;
    }
    return __real_fstat(descriptor, status);
}
int __wrap_fseek(FILE *file, long offset, int whence)
{
    if (file == tag_file && whence == SEEK_END)
        end_seeks++;
    return __real_fseek(file, offset, whence);
}
void *__wrap_malloc(size_t size)
{
    if (watch_allocations) {
        tag_allocations++;
        allocated_bytes += size;
    }
    return __real_malloc(size);
}
size_t __wrap_fread(void *data, size_t size, size_t count, FILE *file)
{
    if (file == tag_file && short_read && count > 10)
        count--;
    size_t result = __real_fread(data, size, count, file);
    if (file == tag_file)
        read_bytes += result * size;
    return result;
}
#ifdef __GLIBC__
size_t __wrap___fread_chk(void *data, size_t capacity, size_t size, size_t count, FILE *file)
{
    if (file == tag_file && short_read && count > 10)
        count--;
    size_t result = __real___fread_chk(data, capacity, size, count, file);
    if (file == tag_file)
        read_bytes += result * size;
    return result;
}
#endif
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

static void truncated_tags(const char *root)
{
    char path[PATH_CAP];
    assert(path_join(path, root, "truncated-tag.mp3"));
    const unsigned sizes[] = {1024 * 1024 + 65536, 65536, 10};
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        unsigned char header[10] = {'I', 'D', '3', 3, 0, 0};
        for (unsigned byte = 0; byte < 4; byte++)
            header[6 + byte] = (sizes[i] >> (21 - 7 * byte)) & 127;
        FILE *file = fopen(path, "wb");
        assert(file && fwrite(header, 1, sizeof header, file) == sizeof header && !fclose(file));
        tag_allocations = 0;
        allocated_bytes = read_bytes = end_seeks = 0;
        watched_path = path;
        watch_allocations = true;
        assert(!embedded_album_art(&output.image, path));
        watch_allocations = false;
        watched_path = NULL;
        assert(!tag_allocations && !allocated_bytes && read_bytes == 10 && !end_seeks);
    }
}
static void read_failures(const char *path)
{
    Artwork original;
    memcpy(&original, &output.image, sizeof original);
    for (unsigned fault = 0; fault < 3; fault++) {
        watched_path = path;
        short_read = fault == 0;
        close_failure = fault == 1;
        metadata_failure = fault == 2;
        bool decoded = embedded_album_art(&output.image, path);
        watched_path = NULL;
        short_read = close_failure = metadata_failure = false;
        assert(!decoded && !memcmp(&original, &output.image, sizeof original));
    }
    assert(embedded_album_art(&output.image, path));
}
static void large_audio_payload(const char *root)
{
    char path[PATH_CAP];
    assert(path_join(path, root, "album-v3.mp3"));
    struct stat status;
    assert(!stat(path, &status));
    FILE *file = fopen(path, "r+b");
    assert(file && !ftruncate(fileno(file), 64 * 1024 * 1024) && !fclose(file));
    read_bytes = end_seeks = 0;
    watched_path = path;
    assert(embedded_album_art(&output.image, path));
    watched_path = NULL;
    assert(read_bytes == (size_t)status.st_size && !end_seeks);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    char path[PATH_CAP];
    output.before = output.after = 0xCAFE1234;
    truncated_tags(argv[1]);
    assert(artwork_decode_image(&output.image, "tests/fixtures/cover.jpg"));
    assert(output.image.ready && output.image.width <= 96 && output.image.height <= 104);
    for (int version = 3; version <= 4; version++) {
        char name[32];
        snprintf(name, sizeof name, "album-v%d.mp3", version);
        assert(path_join(path, argv[1], name));
        memset(&output.image, 0, sizeof output.image);
        assert(embedded_album_art(&output.image, path));
        assert(output.image.ready && output.image.width > 0);
        read_failures(path);
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
    large_audio_payload(argv[1]);
    reject_empty_jpeg("tests/fixtures/cover.jpg");
    reject_empty_jpeg("tests/fixtures/async-progressive.jpg");
    const unsigned char invalid[24] = {255, 216};
    assert(!image_decode(&output.image, invalid, sizeof invalid));
    puts("PASS: bounded JPEG decode, ID3v2.3/2.4 PNG/JPEG album artwork and malformed tag "
         "rejection");
}
