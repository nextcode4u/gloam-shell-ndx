#ifndef FRONTEND_CHEATS_H
#define FRONTEND_CHEATS_H
#include "types.h"
#define CHEAT_ENTRY_CAP 256
#define CHEAT_RECORD_CAP (256 * 1024)
#define CHEAT_PAYLOAD_CAP (16 * 1024)
typedef struct {
    char name[96], note[256];
    uint32_t offset, words;
    int group;
    bool folder, exclusive, selected, expanded;
} CheatEntry;
typedef struct CheatState {
    char content[PATH_CAP], database[PATH_CAP], status[160];
    uint32_t code, rom_crc, record_crc;
    unsigned char *record;
    size_t record_bytes;
    CheatEntry entries[CHEAT_ENTRY_CAP];
    int count, selected, scroll;
    bool open;
} CheatState;
uint32_t cheats_crc(const void *data, size_t bytes);
bool cheats_load(CheatState *state, const char *root, const char *content);
void cheats_free(CheatState *state);
bool cheats_save(const CheatState *state, const char *root);
bool cheats_payload(const CheatState *state, unsigned char *output, size_t *bytes);
void cheats_toggle(CheatState *state, int entry);
void cheats_open(App *app);
void cheats_move(App *app, int direction);
void cheats_activate(App *app);
bool cheats_close(App *app);
void cheats_touch(App *app, int x, int y);
void cheats_render(const App *app, uint16_t *top, uint16_t *bottom);
bool cheats_backend_available(const char *root, char error[160]);
bool cheats_backend_commit(const char *root, const char *content, char error[160]);
int cheats_visible(const CheatState *state, int indices[CHEAT_ENTRY_CAP]);
#endif
