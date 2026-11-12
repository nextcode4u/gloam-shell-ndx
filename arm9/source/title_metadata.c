/* Internal banner names are optional, lazy and bounded to the visible page. */
#include "title_metadata.h"
#include "browser.h"
#include "icons.h"
#include "layout.h"
#include "text.h"
#include <stdio.h>
#include <string.h>

bool title_metadata_read(const char *path, int language, char title[128])
{
    title[0] = 0;
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[0x6C], version[2], text[256];
    bool valid =
        fread(header, 1, sizeof header, file) == sizeof header && !fseek(file, 0, SEEK_END);
    long bytes = valid ? ftell(file) : -1;
    uint32_t offset = valid ? header[0x68] | (uint32_t)header[0x69] << 8 |
                                  (uint32_t)header[0x6A] << 16 | (uint32_t)header[0x6B] << 24
                            : 0;
    valid = valid && bytes >= 0 && offset >= 0x160 && offset <= (uint32_t)bytes &&
            (uint32_t)bytes - offset >= 0x840 && !fseek(file, (long)offset, SEEK_SET) &&
            fread(version, 1, sizeof version, file) == sizeof version;
    unsigned banner_version = valid ? version[0] | version[1] << 8 : 0;
    int languages = banner_version == 1                              ? 6
                    : banner_version == 2                            ? 7
                    : banner_version == 3 || banner_version == 0x103 ? 8
                                                                     : 0;
    int choices[] = {language, 1, 0};
    for (unsigned i = 0; valid && i < sizeof choices / sizeof choices[0]; i++) {
        int choice = choices[i];
        if (choice < 0 || choice >= languages)
            continue;
        uint64_t position = (uint64_t)offset + 0x240 + (unsigned)choice * 256;
        if (position > (uint32_t)bytes || (uint32_t)bytes - position < sizeof text)
            continue;
        if (fseek(file, (long)position, SEEK_SET) ||
            fread(text, 1, sizeof text, file) != sizeof text)
            break;
        text_from_utf16le(title, 128, text, 128);
        if (*title)
            break;
    }
    fclose(file);
    return *title != 0;
}
void title_metadata_tick(App *app)
{
    if (!app->storage_ready || app->selection_idle_frames < 2)
        return;
    /* Selected details are independent of cover decoding and list-name settings. */
    const Entry *selected = app_selected(app);
    if (!app->info_loaded && (app->detail_tab || !app->show_covers) && selected &&
        selected->kind == ITEM_FILE && icon_is_ds_title(selected->path)) {
        app->info_loaded = true;
        unsigned char header[0x160];
        FILE *file = fopen(selected->path, "rb");
        if (file) {
            if (fread(header, 1, sizeof header, file) == sizeof header) {
                app->info_header_valid = true;
                app->info_unit = header[0x12];
                bool printable = true;
                for (int i = 0; i < 4; i++)
                    if (header[12 + i] < 33 || header[12 + i] > 126)
                        printable = false;
                if (printable) {
                    memcpy(app->info_code, header + 12, 4);
                    app->info_code[4] = 0;
                }
            }
            fclose(file);
        }
        title_metadata_read(selected->path, app->title_language, app->info_title);
        return;
    }
    if (!app->internal_titles)
        return;
    const Location *location = &app->locations[app->category];
    int first = location->scroll, last = first + layout_visible_rows(app);
    if (last > app->entry_count)
        last = app->entry_count;
    for (int pass = -1; pass < last - first; pass++) {
        int i = pass < 0 ? location->selected : first + pass;
        if (i < first || i >= last)
            continue;
        Entry *entry = &app->entries[i];
        if (entry->title_loaded || entry->kind != ITEM_FILE || !icon_is_ds_title(entry->path))
            continue;
        title_metadata_read(entry->path, app->title_language, entry->internal_title);
        entry->title_loaded = true;
        app->library_index_dirty = true;
        return; /* At most one title read per frame. */
    }
}
