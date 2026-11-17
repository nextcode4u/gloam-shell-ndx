#ifndef FRONTEND_LIBRARY_SEARCH_H
#define FRONTEND_LIBRARY_SEARCH_H
#include "types.h"
#define SEARCH_PAGE_CAP 16
#define SEARCH_QUERY_CAP 64
#define SEARCH_FILTER_BYTES 256
#define SEARCH_BLOCK_SIZE 276
typedef struct LibrarySearch {
    void *file;
    char query[SEARCH_QUERY_CAP], scope[PATH_CAP], status[160], built[24];
    char results[SEARCH_PAGE_CAP][PATH_CAP];
    uint32_t records, visited, bytes, signature, next_offset, next_record, skip;
    uint32_t record_start, record_end, position, blocks, block_end, remaining, decoded;
    unsigned char required[SEARCH_FILTER_BYTES];
    bool blocked;
    unsigned page, count, selected, scroll, cursor, idle;
    bool editing, all, pending, more, available;
    Panel parent;
    int parent_selection, parent_scroll;
} LibrarySearch;
bool library_search_open(App *app);
void library_search_close(App *app);
void library_search_back(App *app);
void library_search_start(App *app);
void library_search_step(App *app);
void library_search_move(App *app, int dx, int dy);
void library_search_activate(App *app);
void library_search_locate(App *app);
void library_search_edit(App *app);
void library_search_erase(App *app);
void library_search_scope(App *app);
void library_search_page(App *app, int direction);
void library_search_touch(App *app, int x, int y);
void library_search_render(const App *app, uint16_t *top, uint16_t *bottom);
#endif
