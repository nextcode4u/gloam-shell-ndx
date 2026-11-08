#ifndef FRONTEND_SLOT1_H
#define FRONTEND_SLOT1_H
#include "types.h"

/* Each tick issues at most one bounded card command. Hardware stays in main.c. */
typedef struct {
    bool (*present)(void);
    void (*power)(bool enabled);
    bool (*transfer)(const uint8_t command[8], uint32_t flags, void *output, size_t bytes);
    void (*seed)(uint32_t low, uint16_t high);
    bool (*key_seed)(uint32_t words[0x412]);
} Slot1Operations;

typedef struct {
    unsigned phase, wait_frames, secure_step, counter;
    bool detected, done, repeat, modern;
    uint32_t chip_id, normal_flags, secure_flags, banner_offset, banner_bytes, banner_read;
    uint32_t key[0x412], keycode[3];
    uint32_t command_words[2];
    uint8_t header[512], banner[0x2400];
} Slot1Probe;

uint16_t slot1_crc16(const void *data, size_t bytes);
bool slot1_publish_banner(App *app, const void *banner, size_t bytes);
void slot1_tick(App *app, Slot1Probe *probe, const Slot1Operations *operations);
#endif
