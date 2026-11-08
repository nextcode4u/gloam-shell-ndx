#include "../arm9/source/ui/drawing.h"
#include "font_pack.h"
#include "text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FontPack font;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} screen;
static uint16_t expected[256 * 192];

static void check_scroll(unsigned frames, int offset)
{
    const char *label = "\xC3\xA9"
                        "ABCDEFG";
    for (unsigned i = 0; i < 256 * 192; i++)
        screen.pixels[i] = expected[i] = 0x8421;
    draw_text(expected, 20 - offset, 30, label, 0xFFFF, 8);
    draw_text(expected, 20 - offset + 77, 30, label, 0xFFFF, 8);
    draw_text_scrolling(screen.pixels, 20, 30, label, 0xFFFF, 21, frames, true);
    for (int y = 0; y < 192; y++)
        for (int x = 0; x < 256; x++)
            assert(screen.pixels[y * 256 + x] ==
                   (x >= 20 && x < 41 ? expected[y * 256 + x] : 0x8421));
    assert(screen.before == 0xDEADBEEF && screen.after == 0xDEADBEEF);
}
int main(void)
{
    assert(text_length("Caf\xC3\xA9") == 4);
    assert(text_prefix_bytes("\xC3\xA9Z", 1) == 2);
    const char *emoji = "\xF0\x9F\x8E\xAE";
    assert(text_next(&emoji) == 0x1F3AE && !*emoji);
    const char *bad[] = {"\xF0",     "\xE0\x80\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80",
                         "\xC0\xAF", "\xFF"};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        const char *p = bad[i];
        while (*p) {
            const char *before = p;
            assert(text_next(&p) == '?' && p > before);
        }
    }
    unsigned char utf16[] = {'C', 0, 'a', 0, 'f', 0, 0xE9, 0, 0, 0};
    char out[32];
    text_from_utf16le(out, sizeof out, utf16, 5);
    assert(!strcmp(out, "Caf\xC3\xA9"));
    text_from_utf16le(out, 5, utf16, 5);
    assert(!strcmp(out, "Caf"));
    font_builtin(&font);
    font_use(&font);
    bool ink = false;
    for (int row = 0; row < 12; row++)
        ink |= font_row(0xE9, row) != 0;
    assert(ink);
    assert(font_pack_load(&font, "sdroot/_nds/dsi-frontend/themes/copper/fonts/latin.dsfont") &&
           font.glyph_count == 191);
    font_use(&font);
    screen.before = screen.after = 0xDEADBEEF;
    draw_text_clipped(screen.pixels, 6, 20, "Caf\xC3\xA9 No\xC3\xABl", 0xFFFF, 7);
    draw_text_wrapped(screen.pixels, 10, 40,
                      "Pok\xC3\xA9mon and a long accented title Caf\xC3\xA9 No\xC3\xABl", 0xFFFF,
                      12, 5);
    draw_text(screen.pixels, -8, 185, "\xF0\x9F\x8E\xAE \xC3\x98", 0xFFFF, 10);
    assert(screen.before == 0xDEADBEEF && screen.after == 0xDEADBEEF);
    assert(font_pack_load(&font, "sdroot/_nds/dsi-frontend/themes/copper/fonts/ui.dsfont") &&
           font.glyph_count == 95);
    font_use(&font);
    ink = false;
    for (int row = 0; row < 12; row++)
        ink |= font_row(0xE9, row) != 0;
    assert(ink); /* Safe builtin fallback for older packs. */
    /* The next UTF-8 beginning follows the tail across a three-cell gap.
     * The modulo transition must advance naturally, with no repeated pause. */
    check_scroll(0, 0);
    check_scroll(2, 0);
    check_scroll(3, 1); /* First movement within 50 ms at 60 Hz. */
    check_scroll(105, 35);
    check_scroll(108, 36);
    check_scroll(189, 63);
    check_scroll(210, 70);
    check_scroll(228, 76);
    check_scroll(231, 0);
    check_scroll(234, 1);
    memset(screen.pixels, 0, sizeof screen.pixels);
    memset(expected, 0, sizeof expected);
    draw_text_clipped(expected, 20, 30, "Long settings label", 0xFFFF, 3);
    draw_text_scrolling(screen.pixels, 20, 30, "Long settings label", 0xFFFF, 21, UINT32_MAX,
                        false);
    assert(!memcmp(expected, screen.pixels, sizeof expected));
    draw_text_scrolling(screen.pixels, -8, 185,
                        "\xC3\xA9"
                        "ABCDEFG",
                        0xFFFF, 21, UINT32_MAX, true);
    draw_text_scrolling(screen.pixels, 250, -4, "Long settings label", 0xFFFF, 30, 160, true);
    draw_text_scrolling(screen.pixels, 250, -4, NULL, 0xFFFF, 30, 160, true);
    draw_text_scrolling(screen.pixels, 250, -4, "Nothing", 0xFFFF, 0, 160, true);
    assert(screen.before == 0xDEADBEEF && screen.after == 0xDEADBEEF);
    puts("PASS: strict UTF-8/UTF-16 boundaries, codepoint clipping/wrapping, Latin-1 font "
         "packs/fallback and framebuffer bounds");
}
