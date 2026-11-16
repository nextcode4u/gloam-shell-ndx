/* Japanese fallback metrics, UTF-16 titles and framebuffer-safe mixed text. */
#include "../arm9/source/ui/drawing.h"
#include "font_pack.h"
#include "script_fonts.h"
#include "text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FontPack body, heading;
static struct {
    unsigned before;
    uint16_t pixels[256 * 192];
    unsigned after;
} screen;
int main(int argc, char **argv)
{
    assert(argc == 2);
    script_fonts_configure(argv[1], true);
    script_fonts_prepare_text("本体設定");
    font_builtin(&body);
    heading = body;
    heading.height = 16;
    font_use(&body);
    const char *examples[] = {
        "本体設定", "うごくメモ帳", "ニンテンドーDSiカメラ", "ニンテンドーDSiブラウザー",
        "ﾆﾝﾃﾝﾄﾞｰ",  "日本語の表示"};
    for (unsigned sample = 0; sample < sizeof examples / sizeof *examples; sample++) {
        const char *cursor = examples[sample];
        while (*cursor) {
            unsigned cp = text_next(&cursor);
            if (cp < 0x2000)
                continue;
            unsigned width = cp >= 0xFF61 && cp <= 0xFF9F ? 6 : 12;
            assert(font_advance(&body, cp) == width);
            unsigned ink = 0;
            for (int row = 0; row < 12; row++) {
                uint32_t bits = font_glyph_row(&body, cp, row);
                assert(!(bits >> (width * 2)));
                ink |= bits;
                assert(font_glyph_row(&heading, cp, row + 4) == bits);
            }
            assert(ink); /* Every character in these real title names has a glyph. */
            for (int row = 0; row < 4; row++)
                assert(!font_glyph_row(&heading, cp, row));
        }
    }
    assert(font_measure(&body, "A本体B") == 2 * FONT_WIDTH + 24);
    assert(font_measure(&body, "ﾆﾝﾃﾝﾄﾞｰ") == 7 * 6);
    assert(font_advance(&body, 0x1F3AE) == FONT_WIDTH);
    for (int row = 0; row < 12; row++)
        assert(font_glyph_row(&body, 0x1F3AE, row) == font_glyph_row(&body, '?', row));
    unsigned char title[] = {0x2c, 0x67, 0x53, 0x4f, 0x2d, 0x8a, 0x9a, 0x5b, 0, 0};
    char decoded[64];
    text_from_utf16le(decoded, sizeof decoded, title, 5);
    assert(!strcmp(decoded, "本体設定"));
    char short_title[8];
    text_from_utf16le(short_title, sizeof short_title, title, 5);
    assert(!strcmp(short_title, "本体")); /* Never split a three-byte codepoint. */
    for (int font = 0; font < 2; font++) {
        font_use(font ? &heading : &body);
        for (int y = -20; y <= 200; y += 11) {
            screen.before = screen.after = 0xc0ffee;
            draw_text(screen.pixels, -8, y, "本体設定 カメラ DSi", 0xffff, 40);
            draw_text_wrapped(screen.pixels, 210, y, "うごくメモ帳の日本語表示", 0xffff, 8, 3);
            draw_text_scrolling(screen.pixels, 231, y, "日本語とDSi本体設定", 0xffff, 25, 500,
                                true);
            assert(screen.before == 0xc0ffee && screen.after == 0xc0ffee);
        }
    }
    puts("PASS: Japanese titles, kana/kanji/halfwidth metrics, heading fallback, UTF-16 "
         "boundaries, mixed text, clipping, wrapping and scrolling");
}
