#include "icons.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static BrowserIcon icon;
static bool measure;
static unsigned opens, end_seeks;
static size_t read_bytes;
FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (measure)
        opens++;
    return __real_fopen(path, mode);
}
int __real_fseek(FILE *, long, int);
int __wrap_fseek(FILE *file, long offset, int origin)
{
    if (measure && origin == SEEK_END)
        end_seeks++;
    return __real_fseek(file, offset, origin);
}
size_t __real_fread(void *, size_t, size_t, FILE *);
size_t __wrap_fread(void *data, size_t size, size_t count, FILE *file)
{
    if (measure)
        read_bytes += size * count;
    return __real_fread(data, size, count, file);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    /* A large sparse title must cost the same bounded banner read as a small
     * one, with one open and no walk to EOF on a FAT-backed device. */
    assert(!truncate(argv[1], 64 * 1024 * 1024));
    measure = true;
    assert(icon_load_title(argv[1], &icon) && icon.animation_count == 2);
    measure = false;
    assert(opens == 1 && !end_seeks && read_bytes < 6 * 1024);
    assert(icon_load_banner(argv[1], icon.pixels));
    assert(icon_load_animation(argv[1], &icon) && icon.animation_count == 2);
    assert(icon.pixels[0] == 0x801F && icon.pixels[1023] == 0x801F);
    icon_animate(&icon, false);
    assert(icon.animation_index == 0);
    icon_animate(&icon, false);
    assert(icon.animation_index == 1);
    assert(icon.pixels[0] == 0 && icon.pixels[1023] == 0xFC00);
    icon_animate(&icon, false);
    assert(icon.animation_index == 0 && icon.pixels[0] == 0x801F);
    icon_animate(&icon, false);
    icon_animate(&icon, false);
    assert(icon.animation_index == 1);
    icon_animate(&icon, true);
    assert(icon.animation_index == 0 && icon.pixels[0] == 0x801F);
    for (int i = 0; i < 200; i++)
        icon_animate(&icon, true);
    assert(icon.animation_index == 0);
    FILE *f = fopen(argv[1], "r+b");
    assert(f && !fseek(f, 0x200 + 8, SEEK_SET));
    fputc(0, f);
    fputc(0, f);
    assert(!fclose(f));
    assert(!icon_load_animation(argv[1], &icon) && !icon.animation_count);
    assert(icon_load_banner(argv[1],
                            icon.pixels)); /* A bad animation retains static-banner support. */
    assert(icon_load_title(argv[1], &icon) && !icon.animation_count);
    assert(!truncate(argv[1], 0x200 + 0x23bf));
    assert(icon_load_title(argv[1], &icon) && !icon.animation_count);
    assert(!truncate(argv[1], 0x200 + 0x83f));
    assert(!icon_load_title(argv[1], &icon) && !icon.animation_count);
    puts("PASS: native DSi animation CRC/bounds, durations, loop, palette/bitmap selection, flips, "
         "transparency and reduced-motion freeze");
}
