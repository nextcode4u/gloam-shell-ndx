#include "paths.h"
#include "script_fonts.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void put32(unsigned char *p, unsigned n)
{
    for (int i = 0; i < 4; i++)
        p[i] = n >> (i * 8);
}
static void write_pack(const char *path, unsigned char *data, size_t bytes)
{
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(data, 1, bytes, file) == bytes && !fclose(file));
}
int main(int argc, char **argv)
{
    assert(argc == 3);
    script_fonts_configure(argv[2], true);
    script_fonts_prepare_text("English menu");
    assert(!script_fonts_attempted() && !script_fonts_bytes());
    unsigned advance;
    uint32_t bits;
    assert(!script_fonts_glyph(0x672c, 1, &advance, &bits));
    script_fonts_prepare_text("本体設定 Ω");
    assert(script_fonts_attempted() == 2 && script_fonts_count() > 6817);
    assert(script_fonts_glyph(0x672c, 1, &advance, &bits) && advance == 12);
    size_t resident = script_fonts_bytes();
    for (int i = 0; i < 100; i++)
        script_fonts_prepare_text("本体設定 Ω");
    assert(script_fonts_bytes() == resident);
    script_fonts_configure(argv[2], false);
    script_fonts_prepare_text("本体設定");
    assert(!script_fonts_bytes() && !script_fonts_attempted());
    assert(!script_fonts_glyph(0x672c, 1, &advance, &bits));

    char directory[PATH_CAP], pack[PATH_CAP];
    assert(path_join(directory, argv[1], "_nds"));
    assert(!mkdir(directory, 0777));
    assert(path_join(directory, argv[1], "_nds/dsi-frontend"));
    assert(!mkdir(directory, 0777));
    assert(path_join(directory, argv[1], "_nds/dsi-frontend/fonts"));
    assert(!mkdir(directory, 0777));
    assert(path_join(pack, directory, "japanese.gsf"));
    unsigned char data[49] = "GSFONT01";
    put32(data + 8, 1);
    data[16] = 12;
    data[17] = 1;
    put32(data + 20, 0x672c);
    data[24] = 12;
    data[25] = 1;
    for (int bad = 0; bad < 7; bad++) {
        unsigned char changed[sizeof data];
        memcpy(changed, data, sizeof changed);
        if (bad == 1)
            put32(changed + 8, 16385);
        if (bad == 2)
            changed[17] = 3;
        if (bad == 3)
            changed[24] = 17;
        if (bad == 4)
            put32(changed + 20, 0xd800);
        if (bad == 5)
            changed[26] = 0x80;
        uint32_t hash = 2166136261u;
        for (unsigned i = 20; i < sizeof changed; i++)
            hash = (hash ^ changed[i]) * 16777619u;
        put32(changed + 12, bad == 0 ? hash ^ 1 : hash);
        write_pack(pack, changed, sizeof changed - (bad == 6));
        script_fonts_configure(argv[1], true);
        script_fonts_prepare_text("本体設定");
        assert(!script_fonts_bytes() && script_fonts_attempted() == 1);
    }
    uint32_t hash = 2166136261u;
    for (unsigned i = 20; i < sizeof data; i++)
        hash = (hash ^ data[i]) * 16777619u;
    put32(data + 12, hash);
    write_pack(pack, data, sizeof data);
    script_fonts_configure(argv[1], true);
    script_fonts_prepare_text("本");
    assert(script_fonts_count() == 1);
    assert(script_fonts_glyph(0x672c, 0, &advance, &bits) && bits == 3 && advance == 12);
    assert(script_fonts_glyph(0x672c, -1, &advance, &bits) && !bits);
    script_fonts_configure("", false);
    assert(!script_fonts_bytes());
    puts("PASS: lazy external script fonts, cached metrics, recovery, malformed "
         "bounds/checksum/rows and cleanup");
}
