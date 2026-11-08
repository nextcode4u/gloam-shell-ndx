#include "drawing.h"
#include "file_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    uint32_t before;
    uint16_t pixels[256 * 192];
    uint32_t after;
} screen;
int main(void)
{
    static const struct {
        const char *name;
        PlaceholderIcon icon;
    } cases[] = {{"Song.MP3", PLACEHOLDER_AUDIO},
                 {"music.it", PLACEHOLDER_AUDIO},
                 {"movie.rvid", PLACEHOLDER_VIDEO},
                 {"clip.mp4", PLACEHOLDER_VIDEO},
                 {"book.epub", PLACEHOLDER_BOOK},
                 {"manual.pdf", PLACEHOLDER_BOOK},
                 {"notes.txt", PLACEHOLDER_TEXT},
                 {"readme.md", PLACEHOLDER_TEXT},
                 {"settings.TOML", PLACEHOLDER_CONFIG},
                 {"config.ini", PLACEHOLDER_CONFIG},
                 {"game.sav", PLACEHOLDER_SAVE},
                 {"game.pub", PLACEHOLDER_SAVE},
                 {"photo.JPEG", PLACEHOLDER_IMAGE},
                 {"image.webp", PLACEHOLDER_IMAGE},
                 {"collection.7z", PLACEHOLDER_ARCHIVE},
                 {"archive.zip", PLACEHOLDER_ARCHIVE},
                 {"title.gba", PLACEHOLDER_GAME},
                 {"title.dsi", PLACEHOLDER_GAME},
                 {"title.col", PLACEHOLDER_GAME},
                 {"disk.d64", PLACEHOLDER_GAME},
                 {"emulator.elf", PLACEHOLDER_APPLICATION},
                 {"shortcut.argv", PLACEHOLDER_SHORTCUT},
                 {"unknown.bin", PLACEHOLDER_FILE},
                 {"extensionless", PLACEHOLDER_FILE}};
    Entry entry = {.kind = ITEM_FILE};
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++) {
        strcpy(entry.name, cases[i].name);
        assert(entry_file_type(&entry).icon == cases[i].icon);
    }
    strcpy(entry.name, "title.md");
    strcpy(entry.path, "sd:/ROMs/Genesis/title.md");
    assert(entry_file_type(&entry).icon == PLACEHOLDER_GAME);
    strcpy(entry.path, "sd:/media/readme.md");
    assert(entry_file_type(&entry).icon == PLACEHOLDER_TEXT);
    entry.kind = ITEM_DIRECTORY;
    strcpy(entry.name, "music.mp3");
    assert(entry_file_type(&entry).icon == PLACEHOLDER_FOLDER);
    entry.kind = ITEM_MISSING;
    assert(entry_file_type(&entry).icon == PLACEHOLDER_MISSING);
    entry.kind = ITEM_CATEGORY;
    entry.target = CATEGORY_APPS;
    assert(entry_file_type(&entry).icon == PLACEHOLDER_APPLICATION);
    assert(!strcmp(entry_file_type(&entry).label, "LINK"));
    assert(!strcmp(placeholder_label((PlaceholderIcon)-1), "FILE"));
    screen.before = 0x12345678;
    screen.after = 0x87654321;
    for (int icon = 0; icon < PLACEHOLDER_COUNT; icon++) {
        assert(*placeholder_label(icon));
        for (int size = 12; size <= 64; size += 4) {
            memset(screen.pixels, 0, sizeof screen.pixels);
            draw_placeholder_icon(screen.pixels, 10, 10, size, icon, 0xFFFF);
            bool ink = false;
            for (int y = 0; y < 192; y++)
                for (int x = 0; x < 256; x++) {
                    if (x < 10 || x >= 10 + size || y < 10 || y >= 10 + size)
                        assert(!screen.pixels[y * 256 + x]);
                    ink |= screen.pixels[y * 256 + x] != 0;
                }
            assert(ink);
            draw_placeholder_icon(screen.pixels, -8, -8, size, icon, 0xFFFF);
            draw_placeholder_icon(screen.pixels, 250, 187, size, icon, 0xFFFF);
            assert(screen.before == 0x12345678 && screen.after == 0x87654321);
        }
    }
    puts("PASS: common file types, ambiguous Markdown, kind priority and clipped placeholder "
         "glyphs");
}
