/* Reproducible malformed-input coverage; no external ROMs or network needed. */
#include "font_pack.h"
#include "icons.h"
#include "image_decode.h"
#include "paths.h"
#include "ui_strings.h"
#include "update_checker.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned random_state = 0x474c4f41;
static unsigned next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
static void write_bytes(const char *path, const void *data, size_t size)
{
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(data, 1, size, file) == size && !fclose(file));
}
static BrowserIcon icon;
static Artwork artwork;
static UiLanguage language;
static unsigned char data[16384], png[16384];
static void mutate(unsigned char *bytes, size_t size, unsigned iteration)
{
    for (unsigned change = 0; change < 1 + iteration % 12; change++)
        bytes[next_random() % size] ^= (unsigned char)(1 + next_random() % 255);
}
int main(int argc, char **argv)
{
    assert(argc == 2 && path_prepare_preferences_directory(argv[1]));
    FILE *file = fopen("tests/fixtures/cover-example.png", "rb");
    assert(file);
    size_t png_size = fread(png, 1, sizeof png, file);
    assert(png_size > 32 && !ferror(file) && !fclose(file));
    const char *json = "HTTP/1.0 200 OK\r\n\r\n[{\"tag_name\":\"v1.2.3\",\"draft\":false}]";
    char path[PATH_CAP], tag[UPDATE_TAG_CAP];
    assert(path_preferences_file(path, argv[1], "language.ini"));
    for (unsigned iteration = 0; iteration < 4000; iteration++) {
        size_t size = iteration % 3 == 0 ? strlen(json) : png_size;
        memcpy(data, iteration % 3 == 0 ? (const void *)json : png, size);
        mutate(data, size, iteration);
        if (iteration % 4 == 0)
            size = next_random() % size;
        data[size] = 0;
        if (iteration % 3 == 0)
            update_response_tag((char *)data, size, tag);
        else
            image_decode(&artwork, data, size);
        icon_decode_banner(data, size, icon.pixels);
        /* The fixed-size header is separate from the length-bounded animation. */
        unsigned char header[32] = {3, 1};
        icon_decode_animation(header, data, size, &icon);
        if (iteration % 20 == 0) {
            write_bytes(path, data, size);
            FontPack font;
            font_pack_load(&font, path);
            ui_language_load(&language, argv[1], true);
        }
    }
    /* Keep valid format seeds so mutation reaches beyond signature rejection. */
    unsigned char font_seed[2296] = "DSFONT01\7\14\40\137";
    for (size_t i = 16; i < sizeof font_seed; i += 2)
        font_seed[i] = 1;
    FontPack font;
    write_bytes(path, font_seed, sizeof font_seed);
    assert(font_pack_load(&font, path));
    const char *catalog = "version=1\ngames=Juegos\nvalue.off=No\ndialog.title=Opciones\n";
    write_bytes(path, catalog, strlen(catalog));
    ui_language_load(&language, argv[1], true);
    assert(language.loaded && !strcmp(ui_translate(&language, "value.off", "Off"), "No"));
    for (unsigned iteration = 0; iteration < 400; iteration++) {
        size_t size = iteration % 2 ? strlen(catalog) : sizeof font_seed;
        memcpy(data, iteration % 2 ? (const void *)catalog : font_seed, size);
        mutate(data, size, iteration);
        if (iteration % 4 == 0)
            size = next_random() % size;
        write_bytes(path, data, size);
        if (iteration % 2) {
            ui_language_load(&language, argv[1], true);
            ui_translate(&language, "dialog.title", "Options");
        } else
            font_pack_load(&font, path);
    }
    puts("PASS: 4400 deterministic truncated/mutated HTTP, PNG, banner, animation, font and "
         "catalog inputs");
}
