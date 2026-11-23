/* On-demand index reader: bounded work and memory, no directory walking. */
#include "library_search.h"
#include "app.h"
#include "browser.h"
#include "directory_scan.h"
#include "inspection.h"
#include "paths.h"
#include "script_fonts.h"
#include "startup.h"
#include "tabs.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#define INDEX_LIMIT (64u * 1024 * 1024)
static const char keyboard[] = "qwertyuiopasdfghjkl-zxcvbnm_./0123456789";
static uint32_t le32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint32_t checksum(const unsigned char *data, size_t size, uint32_t hash)
{
    while (size--)
        hash = (hash ^ *data++) * 16777619u;
    return hash;
}
static bool stop(LibrarySearch *s)
{
    FILE *file = s->file;
    bool valid = !file || !ferror(file);
    if (file && fclose(file))
        valid = false;
    s->file = NULL;
    s->pending = false;
    return valid;
}
void library_search_close(App *app)
{
    if (app->search)
        stop(app->search);
    free(app->search);
    app->search = NULL;
}
void library_search_back(App *app)
{
    if (!app->search)
        return;
    Panel parent = app->search->parent;
    int selected = app->search->parent_selection, scroll = app->search->parent_scroll;
    library_search_close(app);
    if (parent == PANEL_QUICK || parent == PANEL_ACTIONS) {
        app_panel(app, parent);
        app->panel_selection = selected;
        app->panel_scroll = scroll;
    }
}
static void failed(LibrarySearch *s, const char *message)
{
    stop(s);
    s->count = 0;
    s->more = s->available = false;
    path_copy(s->status, message, sizeof s->status);
}
/* Block filters can skip payload reads. Recheck size before publishing a page
 * so a truncated index cannot masquerade as a successful empty search. */
static bool complete(LibrarySearch *s)
{
    FILE *file = s->file;
    struct stat metadata;
    bool valid = file && !fstat(fileno(file), &metadata) && S_ISREG(metadata.st_mode) &&
                 metadata.st_size == (off_t)s->bytes;
    if (!stop(s))
        valid = false;
    if (!valid)
        failed(s, "Search index changed or could not be read. Reopen search or rebuild it.");
    return valid;
}
static bool open_index(App *app)
{
    LibrarySearch *s = app->search;
    stop(s);
    char path[PATH_CAP];
    if (!app->storage_ready || app->storage_lost ||
        !path_preferences_file(path, app->root, "cache/library.gli")) {
        failed(s, "SD unavailable. Normal browsing remains available.");
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        failed(s, "No search index. Build one in desktop Setup > Library, then reopen search.");
        return false;
    }
    /* Inspect the opened file, not a pathname that may change before fopen. */
    struct stat metadata;
    if (fstat(fileno(file), &metadata) || !S_ISREG(metadata.st_mode) || metadata.st_size < 32 ||
        (uint64_t)metadata.st_size > INDEX_LIMIT) {
        fclose(file);
        failed(s, "Search index missing or invalid. Build it in desktop Setup > Library.");
        return false;
    }
    unsigned char header[32];
    bool valid = fread(header, 1, sizeof header, file) == sizeof header && !ferror(file) &&
                 (!memcmp(header, "GLIDX001", 8) || !memcmp(header, "GLIDX002", 8)) &&
                 checksum(header, 28, 2166136261u) == le32(header + 28);
    long bytes = (long)metadata.st_size;
    bool blocked = valid && !memcmp(header, "GLIDX002", 8);
    uint32_t blocks = valid ? le32(header + 24) : 0;
    uint64_t total = valid ? 32ULL + le32(header + 12) + (uint64_t)blocks * SEARCH_BLOCK_SIZE : 0;
    valid = valid && bytes >= 32 && (unsigned long)bytes <= INDEX_LIMIT &&
            total == (uint64_t)bytes && le32(header + 8) <= 1000000 &&
            le32(header + 8) <= le32(header + 12) / 9 &&
            (blocked ? blocks == (le32(header + 8) + 63) / 64 : blocks == 0);
    if (!valid) {
        fclose(file);
        failed(s, "Invalid search index. Rebuild it in desktop Setup > Library.");
        return false;
    }
    if (s->signature && s->signature != le32(header + 28))
        s->page = s->next_offset = s->next_record = 0;
    s->signature = le32(header + 28);
    s->records = le32(header + 8);
    s->blocked = blocked;
    s->blocks = blocks;
    s->record_start = 32 + blocks * SEARCH_BLOCK_SIZE;
    s->record_end = s->record_start + le32(header + 12);
    s->remaining = s->decoded = 0;
    s->position = s->record_start;
    s->bytes = (uint32_t)bytes;
    uint64_t stamp = le32(header + 16) | (uint64_t)le32(header + 20) << 32;
    time_t date = (time_t)stamp;
    struct tm tm;
    if (stamp <= INT64_MAX && (uint64_t)date == stamp && gmtime_r(&date, &tm))
        strftime(s->built, sizeof s->built, "%Y-%m-%d %H:%M UTC", &tm);
    else
        strcpy(s->built, "Date unavailable");
    s->file = file;
    s->available = true;
    return true;
}
bool library_search_open(App *app)
{
    library_search_close(app);
    if (!app->storage_ready || app->storage_lost)
        return false;
    LibrarySearch *s = calloc(1, sizeof *s);
    if (!s)
        return false;
    app->search = s;
    s->parent = app->panel;
    s->parent_selection = app->panel_selection;
    s->parent_scroll = app->panel_scroll;
    inspection_close(app);
    startup_cancel(app);
    s->editing = true;
    s->all = app->category == CATEGORY_HOME || app->category == CATEGORY_SYSTEM ||
             app->category == CATEGORY_FILES;
    path_copy(s->scope, tabs_browse_root(app, app->category), PATH_CAP);
    app->rail_focus = false;
    app->panel = PANEL_NONE;
    if (open_index(app)) {
        if (stop(s))
            strcpy(s->status, "Enter words, then Search. Empty query lists indexed files.");
        else
            failed(s, "Could not finish reading the search index. Reopen search.");
    }
    return true;
}
static void prepare_filter(const App *app);

static void begin(App *app)
{
    LibrarySearch *s = app->search;
    if (!s || !open_index(app))
        return;
    s->visited = s->count = s->selected = s->scroll = s->idle = 0;
    s->more = false;
    s->skip = s->page * SEARCH_PAGE_CAP;
    if (s->page && s->next_offset) {
        if (s->next_offset < s->record_start || s->next_offset >= s->record_end) {
            failed(s, "Search position unavailable. Reopen search.");
            return;
        }
        s->visited = s->next_record;
        s->position = s->next_offset;
        if (!s->blocked && fseek(s->file, s->position, SEEK_SET)) {
            failed(s, "Search position unavailable. Reopen search.");
            return;
        }
        s->skip = 0;
    }
    s->next_offset = s->next_record = 0;
    prepare_filter(app);
    s->editing = false;
    s->pending = true;
    strcpy(s->status, "Searching index... B cancels and returns.");
}
void library_search_start(App *app)
{
    if (!app->search)
        return;
    app->search->page = app->search->next_offset = app->search->next_record = 0;
    begin(app);
}
static uint32_t fold(uint32_t c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7) ||
        (c >= 0x391 && c <= 0x3AB && c != 0x3A2) || (c >= 0x410 && c <= 0x42F))
        return c + 32;
    return c == 0x401 ? 0x451 : c;
}
static void add_gram(unsigned char *filter, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t hash = ((2166136261u ^ a) * 16777619u ^ b) * 16777619u;
    hash = (hash ^ c) * 16777619u;
    unsigned first = hash & 2047, second = (hash >> 11) & 2047;
    filter[first / 8] |= 1u << (first % 8);
    filter[second / 8] |= 1u << (second % 8);
}
static void filter_text(unsigned char *filter, const char *text, bool words)
{
    uint32_t a = 0, b = 0;
    unsigned seen = 0;
    while (*text) {
        uint32_t c = fold(text_next(&text));
        if (words && c == ' ') {
            seen = 0;
            continue;
        }
        if (seen >= 2)
            add_gram(filter, a, b, c);
        a = b;
        b = c;
        seen++;
    }
}
static void prepare_filter(const App *app)
{
    LibrarySearch *s = app->search;
    memset(s->required, 0, sizeof s->required);
    filter_text(s->required, s->query, true);
    if (!s->all && path_is_within_root(s->scope, app->root)) {
        const char *relative = s->scope + strlen(app->root);
        while (*relative == '/')
            relative++;
        filter_text(s->required, relative, false);
    }
}
/* Returns -1 for invalid metadata, 0 for a skipped block, 1 for readable records. */
static int prepare_block(LibrarySearch *s)
{
    unsigned char data[SEARCH_BLOCK_SIZE];
    uint32_t index = s->visited / 64;
    if (index >= s->blocks ||
        (ftell(s->file) != (long)(32 + index * SEARCH_BLOCK_SIZE) &&
         fseek(s->file, 32 + index * SEARCH_BLOCK_SIZE, SEEK_SET)) ||
        fread(data, 1, sizeof data, s->file) != sizeof data ||
        checksum(data, SEARCH_BLOCK_SIZE - 4, 2166136261u) != le32(data + SEARCH_BLOCK_SIZE - 4))
        return -1;
    uint32_t offset = le32(data), first = le32(data + 4), count = le32(data + 8),
             bytes = le32(data + 12);
    uint32_t expected = s->records - index * 64;
    if (expected > 64)
        expected = 64;
    if (first != index * 64 || count != expected || offset < s->record_start ||
        offset > s->record_end || bytes > s->record_end - offset || bytes < count * 9 ||
        s->position < offset || s->position >= offset + bytes ||
        (s->visited == first && s->position != offset))
        return -1;
    s->block_end = offset + bytes;
    bool possible = true;
    for (unsigned i = 0; i < sizeof s->required; i++)
        if ((data[16 + i] & s->required[i]) != s->required[i]) {
            possible = false;
            break;
        }
    if (!possible) {
        s->visited = first + count;
        s->position = s->block_end;
        return 0;
    }
    s->remaining = first + count - s->visited;
    return fseek(s->file, s->position, SEEK_SET) ? -1 : 1;
}
static bool contains(const char *text, const char *word, size_t size)
{
    for (const char *start = text; *start; text_next(&start)) {
        const char *a = start, *b = word;
        bool equal = true;
        while ((size_t)(b - word) < size)
            if (!*a || fold(text_next(&a)) != fold(text_next(&b))) {
                equal = false;
                break;
            }
        if (equal)
            return true;
    }
    return false;
}
static bool query_matches(const char *path, const char *query)
{
    while (*query) {
        while (*query == ' ')
            query++;
        const char *end = strchr(query, ' ');
        size_t size = end ? (size_t)(end - query) : strlen(query);
        if (size && !contains(path, query, size))
            return false;
        query += size;
    }
    return true;
}
static bool hidden_folder(const App *app, const char *path, const char *root)
{
    if (!app->hide_save_folders && !app->hide_bios_folders)
        return false;
    const char *part = path + strlen(root);
    while (*part == '/')
        part++;
    DirectoryRecord folder = {.directory = true};
    const char *end;
    while ((end = strchr(part, '/'))) {
        size_t length = (size_t)(end - part);
        if (length < sizeof folder.name) {
            memcpy(folder.name, part, length);
            folder.name[length] = 0;
            if ((app->hide_save_folders && directory_scan_is_save_folder(&folder)) ||
                (app->hide_bios_folders && directory_scan_is_bios_folder(&folder)))
                return true;
        }
        part = end + 1;
    }
    return false;
}
static bool allowed(const App *app, const char *path, unsigned flags)
{
    if ((!app->show_hidden && (flags & 1)) || (!app->show_system && (flags & 2)))
        return false;
    bool root = false;
    for (int i = CATEGORY_GAMES; i < app->tab_count; i++)
        if (i != CATEGORY_SYSTEM && i != CATEGORY_FILES &&
            path_is_within_root(path, app->tabs[i].home) &&
            !hidden_folder(app, path, app->tabs[i].home))
            root = true;
    if (!root || (!app->search->all && (!path_is_within_root(path, app->search->scope) ||
                                        hidden_folder(app, path, app->search->scope))))
        return false;
    return !app->filter_recognized || browser_recognizes_file(app, path);
}
void library_search_step(App *app)
{
    LibrarySearch *s = app->search;
    if (!s)
        return;
    s->idle++;
    if (!s->pending)
        return;
    clock_t began = clock();
    size_t consumed = 0, metadata = 0;
    for (int slice = 0; slice < 64 && consumed < 4096 && metadata < 16384; slice++) {
        if (s->visited == s->records) {
            if (s->position != s->record_end) {
                failed(s, "Search index record count is invalid. Rebuild it.");
                return;
            }
            if (!complete(s))
                return;
            strcpy(s->status, s->count
                                  ? "A: launch  Y: locate in folder"
                                  : "No matching files. Refresh the index after adding files.");
            return;
        }
        if (slice && clock() - began > CLOCKS_PER_SEC / 1000)
            break;
        if (s->blocked && !s->remaining) {
            int block = prepare_block(s);
            metadata += SEARCH_BLOCK_SIZE;
            if (block < 0) {
                failed(s, "Damaged search block table. Rebuild the index.");
                return;
            }
            if (!block)
                continue;
        }
        long offset = ftell(s->file);
        unsigned char record[8];
        char relative[PATH_CAP], path[PATH_CAP];
        if (offset < 32 || fread(record, 1, 8, s->file) != 8) {
            failed(s, "Truncated search index. Rebuild it.");
            return;
        }
        unsigned size = record[0] | (unsigned)record[1] << 8;
        if (!size || size >= PATH_CAP || record[2] > 3 || record[3] ||
            (uint32_t)offset + 8 + size > (s->blocked ? s->block_end : s->record_end) ||
            fread(relative, 1, size, s->file) != size || memchr(relative, 0, size) ||
            checksum((unsigned char *)relative, size, checksum(record + 2, 2, 2166136261u)) !=
                le32(record + 4)) {
            failed(s, "Damaged search index record. Rebuild it.");
            return;
        }
        relative[size] = 0;
        bool valid = relative[0] != '/' && !strchr(relative, ':') && !strchr(relative, '\\');
        for (const char *p = relative; *p;) {
            const char *before = p;
            uint32_t c = text_next(&p);
            if (c < 32 || (c == '?' && *before != '?'))
                valid = false;
        }
        if (!valid || !path_resolve_sd(path, app->root, relative)) {
            failed(s, "Unsafe or invalid UTF-8 search path. Rebuild the index.");
            return;
        }
        consumed += 8 + size;
        s->visited++;
        s->decoded++;
        s->position = (uint32_t)offset + 8 + size;
        if (s->blocked && --s->remaining == 0 && s->position != s->block_end) {
            failed(s, "Search block length is invalid. Rebuild the index.");
            return;
        }
        if (query_matches(relative, s->query) && allowed(app, path, record[2])) {
            if (s->skip)
                s->skip--;
            else if (s->count == SEARCH_PAGE_CAP) {
                s->next_offset = (uint32_t)offset;
                s->next_record = s->visited - 1;
                s->more = true;
                if (!complete(s))
                    return;
                strcpy(s->status, "A launch  Y locate  R next page");
                return;
            } else {
                path_copy(s->results[s->count++], path, PATH_CAP);
                script_fonts_prepare_text(relative);
            }
        }
        if ((slice & 3) == 3 && clock() - began > CLOCKS_PER_SEC / 1000)
            break;
    }
}
void library_search_page(App *app, int direction)
{
    LibrarySearch *s = app->search;
    if (!s || s->pending || s->editing)
        return;
    if (direction > 0 && s->more)
        s->page++;
    else if (direction < 0 && s->page) {
        s->page--;
        s->next_offset = s->next_record = 0;
    } else
        return;
    begin(app);
}
void library_search_edit(App *app)
{
    if (!app->search)
        return;
    if (app->search->editing) {
        library_search_start(app);
        return;
    }
    stop(app->search);
    app->search->editing = true;
    strcpy(app->search->status, "Edit words, then select Search or press X.");
}
static void changed(App *app)
{
    LibrarySearch *s = app->search;
    stop(s);
    s->editing = true;
    s->page = s->count = s->selected = s->scroll = 0;
    s->next_offset = s->next_record = 0;
    s->more = false;
    strcpy(s->status, "Select Search to update the results.");
}
void library_search_erase(App *app)
{
    LibrarySearch *s = app->search;
    if (!s)
        return;
    size_t length = strlen(s->query);
    if (length) {
        do {
            length--;
        } while (length && ((unsigned char)s->query[length] & 0xC0) == 0x80);
        s->query[length] = 0;
    }
    changed(app);
}
void library_search_scope(App *app)
{
    if (!app->search)
        return;
    if (!strcmp(app->search->scope, app->root)) {
        strcpy(app->search->status, "This tab has no folder; search uses all libraries.");
        return;
    }
    app->search->all = !app->search->all;
    changed(app);
}
void library_search_move(App *app, int dx, int dy)
{
    LibrarySearch *s = app->search;
    if (!s)
        return;
    s->idle = 0;
    if (s->editing) {
        int row = s->cursor < 40 ? (int)s->cursor / 10 : 4;
        int column = row == 4 ? (int)s->cursor - 40 : (int)s->cursor % 10;
        int width = row == 4 ? 3 : 10;
        column = (column + dx + width) % width;
        if (dy) {
            if (row == 4)
                column = column * 4 + 1;
            row = (row + dy + 5) % 5;
            if (row == 4)
                column = column < 3 ? 0 : column < 7 ? 1 : 2;
        }
        s->cursor = row == 4 ? 40 + column : row * 10 + column;
    } else if (dx)
        library_search_page(app, dx);
    else if (s->count) {
        s->selected = (unsigned)(((int)s->selected + dy + (int)s->count) % (int)s->count);
        if (s->selected < s->scroll)
            s->scroll = s->selected;
        if (s->selected >= s->scroll + 6)
            s->scroll = s->selected - 5;
    }
}
void library_search_activate(App *app)
{
    LibrarySearch *s = app->search;
    if (!s)
        return;
    if (s->editing) {
        if (s->cursor == 41) {
            library_search_erase(app);
            return;
        }
        if (s->cursor == 42) {
            library_search_start(app);
            return;
        }
        size_t length = strlen(s->query);
        char c = s->cursor == 40 ? ' ' : keyboard[s->cursor];
        if (length + 1 < sizeof s->query && c) {
            s->query[length] = c;
            s->query[length + 1] = 0;
            changed(app);
        }
        return;
    }
    if (s->selected >= s->count || s->pending)
        return;
    struct stat st;
    if (stat(s->results[s->selected], &st) || !S_ISREG(st.st_mode)) {
        strcpy(s->status, "File moved or disappeared. Refresh the index in desktop Setup.");
        return;
    }
    app_launch_file(app, s->results[s->selected]);
}
void library_search_locate(App *app)
{
    LibrarySearch *s = app->search;
    if (!s || s->editing || s->pending || s->selected >= s->count)
        return;
    char path[PATH_CAP];
    path_copy(path, s->results[s->selected], PATH_CAP);
    struct stat st;
    if (stat(path, &st) || !S_ISREG(st.st_mode)) {
        strcpy(s->status, "File moved or disappeared. Refresh the index in desktop Setup.");
        return;
    }
    library_search_close(app);
    browser_reveal_file(app, path);
}

void library_search_touch(App *app, int x, int y)
{
    LibrarySearch *s = app->search;
    if (!s || x < 8 || x >= 248 || y < 0)
        return;
    if (y < 24) {
        if (x >= 176)
            library_search_scope(app);
        return;
    }
    if (y < 45) {
        if (!s->editing)
            library_search_edit(app);
        return;
    }
    if (y >= 170) {
        library_search_back(app);
        return;
    }
    if (s->editing) {
        if (y >= 53 && y < 141) {
            s->cursor = (unsigned)((y - 53) / 22 * 10 + (x - 8) / 24);
            library_search_activate(app);
        } else if (y >= 145 && y < 166) {
            s->cursor = x < 88 ? 40 : x < 168 ? 41 : 42;
            library_search_activate(app);
        }
    } else if (y >= 48 && y < 156) {
        unsigned index = s->scroll + (y - 48) / 18;
        if (index < s->count) {
            if (index == s->selected)
                library_search_activate(app);
            else {
                s->selected = index;
                s->idle = 0;
            }
        }
    } else if (y >= 156 && y < 170)
        library_search_page(app, x < 128 ? -1 : 1);
}
