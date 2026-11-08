/* Read-only cartridge metadata. Protocol/KEY1 formulas follow GBATEK; no ROM or BIOS
 * data is compiled in. The hardware adapter supplies the console's runtime seed. */
#include "slot1.h"
#include "icons.h"
#include "layout.h"
#include "text.h"
#include <string.h>

#define CARD_START (1u << 31)
#define CARD_RESET_HIGH (1u << 29)
#define CARD_LARGE (1u << 28)
#define CARD_SLOW (1u << 27)
#define CARD_BLOCK(n) ((uint32_t)(n) << 24)
#define CARD_SECURITY ((1u << 14) | (1u << 13))
#define CARD_TIMING (CARD_SLOW | 0x003F1FFFu)
#define RAW_FLAGS (CARD_START | CARD_RESET_HIGH | CARD_SLOW)

enum { POWER_OFF = 1, POWER_ON, DUMMY, HEADER, CHIP_ID, KEY_SETUP, KEY_START, SECURE, BANNER };

static uint32_t le32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static unsigned le16(const uint8_t *p)
{
    return p[0] | (unsigned)p[1] << 8;
}
uint16_t slot1_crc16(const void *data, size_t bytes)
{
    const uint8_t *p = data;
    uint16_t crc = 0xFFFF;
    while (bytes--) {
        crc ^= *p++;
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    return crc;
}

/* Native cartridge commands use the modified Blowfish word order documented for
 * NTR KEY1. Only a temporary expanded key lives here; it is never written to SD. */
static void encrypt(const uint32_t key[0x412], uint32_t words[2])
{
    uint32_t x = words[1], y = words[0];
    for (unsigned i = 0; i < 16; i++) {
        uint32_t z = key[i] ^ x;
        x = key[0x12 + (z >> 24)] + key[0x112 + ((z >> 16) & 255)];
        x = (x ^ key[0x212 + ((z >> 8) & 255)]) + key[0x312 + (z & 255)];
        x ^= y;
        y = z;
    }
    words[0] = x ^ key[16];
    words[1] = y ^ key[17];
}
static uint32_t swap32(uint32_t n)
{
    return n << 24 | (n & 0xFF00) << 8 | (n >> 8 & 0xFF00) | n >> 24;
}
static void expand_key(Slot1Probe *probe)
{
    uint32_t game = le32(probe->header + 12);
    probe->keycode[0] = game;
    probe->keycode[1] = game / 2;
    probe->keycode[2] = game * 2;
    for (unsigned round = 0; round < 2; round++) {
        uint32_t scratch[2] = {0};
        encrypt(probe->key, probe->keycode + 1);
        encrypt(probe->key, probe->keycode);
        for (unsigned i = 0; i < 18; i++)
            probe->key[i] ^= swap32(probe->keycode[i % 2]);
        for (unsigned i = 0; i < 0x412; i += 2) {
            encrypt(probe->key, scratch);
            probe->key[i] = scratch[1];
            probe->key[i + 1] = scratch[0];
        }
    }
}
static void wire_command(uint64_t value, uint8_t wire[8])
{
    for (unsigned i = 0; i < 8; i++)
        wire[i] = value >> (56 - i * 8);
}
static bool command(const Slot1Operations *ops, uint64_t value, uint32_t flags, void *out,
                    size_t bytes)
{
    uint8_t wire[8];
    wire_command(value, wire);
    return ops->transfer(wire, flags, out, bytes);
}
static bool secure_command(Slot1Probe *probe, const Slot1Operations *ops)
{
    static const uint8_t operations[] = {0x40, 0x10, 0xA0};
    if (!probe->repeat) {
        uint64_t value = (uint64_t)operations[probe->secure_step] << 56 | (uint64_t)4 << 44 |
                         (uint64_t)4 << 32 | (uint64_t)4 << 20 | probe->counter++;
        probe->command_words[0] = value;
        probe->command_words[1] = value >> 32;
        encrypt(probe->key, probe->command_words);
    }
    uint8_t wire[8];
    wire_command((uint64_t)probe->command_words[1] << 32 | probe->command_words[0], wire);
    if (probe->modern && !probe->repeat) {
        if (!ops->transfer(wire, probe->secure_flags, NULL, 0))
            return false;
        probe->repeat = true;
        /* 1T-ROM repeats require the header's secure-read delay, in 256-cycle
         * units. Wait in the normal frame loop instead of borrowing a timer. */
        unsigned cycles = ((le16(probe->header + 0x6E) & 0x3FFF) + 3) * 256;
        probe->wait_frames = (cycles + 560189) / 560190;
        return true;
    }
    uint32_t id = 0;
    bool chip_id = probe->secure_step == 1;
    if (!ops->transfer(wire, probe->secure_flags | (chip_id ? CARD_BLOCK(7) : 0),
                       chip_id ? &id : NULL, chip_id ? sizeof id : 0))
        return false;
    if (chip_id && id != probe->chip_id)
        return false;
    if (!probe->secure_step) {
        static const uint8_t seed_bytes[] = {0xE8, 0x4D, 0x5A, 0xB1, 0x17, 0x8F, 0x99, 0xD5};
        ops->seed(seed_bytes[probe->header[0x13] & 7] | (4u << 15) | (4u << 27) | 0x6000, 0);
        probe->secure_flags |= CARD_SECURITY;
    }
    probe->repeat = false;
    if (++probe->secure_step == 3)
        probe->phase = BANNER;
    return true;
}

bool slot1_publish_banner(App *app, const void *data, size_t bytes)
{
    const uint8_t *banner = data;
    BrowserIcon *icon = &app->slot1.icon;
    icon->ready = false;
    icon->animation_count = 0;
    app->slot1.title[0] = 0;
    if (!banner || bytes < 0x840 || slot1_crc16(banner + 0x20, 0x820) != le16(banner + 2) ||
        !icon_decode_banner(banner, bytes, icon->pixels))
        return false;
    icon->ready = true;
    if (bytes >= 0x23C0)
        icon_decode_animation(banner, banner + 0x1240, 0x1180, icon);
    unsigned version = le16(banner);
    unsigned languages = version == 1 ? 6 : version == 2 ? 7 : 8;
    int choices[] = {app->title_language, 1, 0};
    for (unsigned i = 0; i < sizeof choices / sizeof choices[0]; i++) {
        int choice = choices[i];
        if (choice < 0 || (unsigned)choice >= languages ||
            bytes < 0x240 + ((unsigned)choice + 1) * 256)
            continue;
        text_from_utf16le(app->slot1.title, sizeof app->slot1.title, banner + 0x240 + choice * 256,
                          128);
        if (*app->slot1.title)
            break;
    }
    return true;
}
static bool visible(const App *app)
{
    int first = app->locations[app->category].scroll;
    int last = first + layout_visible_rows(app);
    if (last > app->entry_count)
        last = app->entry_count;
    for (int i = first; i < last; i++)
        if (app->entries[i].kind == ITEM_SYSTEM && app->entries[i].target == 6)
            return true;
    return false;
}
static bool read_step(App *app, Slot1Probe *p, const Slot1Operations *ops)
{
    switch (p->phase) {
    case POWER_OFF:
        ops->power(true);
        p->wait_frames = 20;
        p->phase = POWER_ON;
        return true;
    case POWER_ON:
        p->phase = DUMMY;
        return true;
    case DUMMY:
        p->phase = HEADER;
        return command(ops, UINT64_C(0x9F00000000000000), RAW_FLAGS | CARD_BLOCK(5), NULL, 0);
    case HEADER:
        if (!command(ops, 0, RAW_FLAGS | CARD_BLOCK(1) | 0x003F1FFF, p->header, 512) ||
            slot1_crc16(p->header, 0x15E) != le16(p->header + 0x15E) || p->header[0x14] > 12)
            return false;
        p->banner_offset = le32(p->header + 0x68);
        if (p->banner_offset < 0x8000 ||
            p->banner_offset > (0x20000u << p->header[0x14]) - sizeof p->banner)
            return false;
        p->normal_flags =
            (le32(p->header + 0x60) & (CARD_TIMING | 0x00506000)) | CARD_START | CARD_RESET_HIGH;
        p->secure_flags = CARD_START | CARD_RESET_HIGH | (le32(p->header + 0x60) & CARD_SLOW) |
                          ((le32(p->header + 0x64) & (CARD_SLOW | 0x1FFF)) +
                           ((le32(p->header + 0x64) >> 16) & 63));
        p->phase = CHIP_ID;
        return true;
    case CHIP_ID:
        if (!command(ops, UINT64_C(0x9000000000000000), RAW_FLAGS | CARD_BLOCK(7), &p->chip_id,
                     sizeof p->chip_id) ||
            !p->chip_id || p->chip_id == UINT32_MAX)
            return false;
        p->modern = (p->chip_id >> 31) != 0;
        if (!p->modern)
            p->secure_flags |= CARD_LARGE;
        p->phase = KEY_SETUP;
        return true;
    case KEY_SETUP:
        if (!ops->key_seed(p->key))
            return false;
        expand_key(p);
        p->counter = 4;
        p->phase = KEY_START;
        return true;
    case KEY_START:
        p->phase = SECURE;
        return command(ops, UINT64_C(0x3C00400400000400),
                       CARD_START | CARD_RESET_HIGH | (le32(p->header + 0x60) & CARD_SLOW), NULL,
                       0);
    case SECURE:
        return secure_command(p, ops);
    case BANNER: {
        uint32_t address = p->banner_offset + p->banner_read;
        if (!command(ops, (uint64_t)0xB7 << 56 | (uint64_t)address << 24,
                     p->normal_flags | CARD_BLOCK(1), p->banner + p->banner_read, 512))
            return false;
        p->banner_read += 512;
        if (p->banner_read == 512) {
            unsigned version = le16(p->banner);
            p->banner_bytes = version == 1       ? 0x840
                              : version == 2     ? 0x940
                              : version == 3     ? 0xA40
                              : version == 0x103 ? 0x23C0
                                                 : 0;
            if (!p->banner_bytes)
                return false;
        }
        if (p->banner_read >= p->banner_bytes) {
            p->done = true;
            return slot1_publish_banner(app, p->banner, p->banner_bytes);
        }
        return true;
    }
    default:
        return false;
    }
}
void slot1_tick(App *app, Slot1Probe *probe, const Slot1Operations *ops)
{
    bool present = app->dsi_mode && ops->present();
    if (!present) {
        if (probe->detected || app->slot1.icon.ready) {
            memset(probe, 0, sizeof *probe);
            memset(&app->slot1, 0, sizeof app->slot1);
        }
        return;
    }
    app->slot1.present = true;
    if (app->launch_requested || app->slot1_requested || !visible(app))
        return;
    if (!probe->detected) {
        probe->detected = true;
        probe->phase = POWER_OFF;
        probe->wait_frames = 4;
        ops->power(false);
        return;
    }
    if (probe->done)
        return;
    if (probe->wait_frames) {
        probe->wait_frames--;
        return;
    }
    if (!read_step(app, probe, ops)) {
        probe->done = true;
        app->slot1.icon.ready = false;
        app->slot1.title[0] = 0;
    }
}
