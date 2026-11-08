/* Original synthetic cartridge fixtures; no retail banner, key or ROM data. */
#include "icons.h"
#include "slot1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static App app;
static Slot1Probe probe;
static uint8_t header[512], banner[0x2400];
static bool present = true, modern, keys = true, fail_transfer;
static unsigned transfers, powered_on, powered_off, key_reads;
static uint32_t id;

static void put16(uint8_t *p, unsigned v)
{
    p[0] = v;
    p[1] = v >> 8;
}
static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++)
        p[i] = v >> (i * 8);
}
static void fixture(void)
{
    memset(&app, 0, sizeof app);
    memset(&probe, 0, sizeof probe);
    memset(header, 0, sizeof header);
    memset(banner, 0, sizeof banner);
    app.dsi_mode = true;
    app.category = CATEGORY_SYSTEM;
    app.entry_count = 1;
    app.title_language = 1;
    app.entries[0].kind = ITEM_SYSTEM;
    app.entries[0].target = 6;
    present = keys = true;
    fail_transfer = false;
    transfers = powered_on = powered_off = key_reads = 0;
    id = modern ? 0x80001FC2 : 0x00001FC2;
    memcpy(header + 12, "TEST", 4);
    put32(header + 0x60, 0x00586000);
    put32(header + 0x64, 0x001808F8);
    put32(header + 0x68, 0x8000);
    put16(header + 0x15E, slot1_crc16(header, 0x15E));
    put16(banner, 1);
    memset(banner + 0x20, 0x11, 512);
    put16(banner + 0x222, 31);
    const char *title = "Synthetic cartridge";
    for (unsigned i = 0; title[i]; i++)
        banner[0x340 + i * 2] = title[i];
    put16(banner + 2, slot1_crc16(banner + 0x20, 0x820));
    icons_reset(&app);
}
static bool is_present(void)
{
    return present;
}
static void power(bool on)
{
    if (on)
        powered_on++;
    else
        powered_off++;
}
static bool key_seed(uint32_t words[0x412])
{
    key_reads++;
    if (!keys)
        return false;
    for (unsigned i = 0; i < 0x412; i++)
        words[i] = 0x13579BDFu * (i + 1); // synthetic cipher input
    return true;
}
static void seed(uint32_t low, uint16_t high)
{
    assert(low && !high);
}
static bool transfer(const uint8_t wire[8], uint32_t flags, void *out, size_t bytes)
{
    transfers++;
    assert(flags & (1u << 31));
    if (fail_transfer)
        return false;
    if (wire[0] == 0 && bytes == 512) {
        memcpy(out, header, sizeof header);
    } else if (wire[0] == 0xB7 && bytes == 512) {
        uint32_t address =
            (uint32_t)wire[1] << 24 | (uint32_t)wire[2] << 16 | (uint32_t)wire[3] << 8 | wire[4];
        assert(address >= 0x8000 && address - 0x8000 <= sizeof banner - 512);
        memcpy(out, banner + address - 0x8000, 512);
    } else if (bytes == 4) {
        memcpy(out, &id, sizeof id);
    } else {
        assert(!bytes);
    }
    return true;
}
static const Slot1Operations operations = {is_present, power, transfer, seed, key_seed};
static void settle(void)
{
    for (unsigned i = 0; i < 120 && !probe.done; i++) {
        unsigned before = transfers;
        slot1_tick(&app, &probe, &operations);
        assert(transfers - before <= 1);
    }
    assert(probe.done);
}
int main(void)
{
    for (unsigned variant = 0; variant < 2; variant++) {
        modern = variant != 0;
        fixture();
        settle();
        assert(app.slot1.present && app.slot1.icon.ready);
        assert(!strcmp(app.slot1.title, "Synthetic cartridge"));
        assert(app.slot1.icon.pixels[0] == 0x801F);
        assert(icons_for_entry(&app, 0) == &app.slot1.icon);
        assert(powered_off == 1 && powered_on == 1 && key_reads == 1);
        unsigned reads = transfers;
        for (unsigned i = 0; i < 60; i++)
            slot1_tick(&app, &probe, &operations);
        assert(transfers == reads); // no continual reinitialization
        present = false;
        slot1_tick(&app, &probe, &operations);
        assert(!app.slot1.present && !app.slot1.icon.ready && !*app.slot1.title);
        present = true;
        put16(banner + 0x222, 0x3E0);
        put16(banner + 2, slot1_crc16(banner + 0x20, 0x820));
        settle();
        assert(app.slot1.icon.pixels[0] == 0x83E0 && powered_on == 2);
    }
    fixture();
    present = false;
    for (unsigned i = 0; i < 100; i++)
        slot1_tick(&app, &probe, &operations);
    assert(!transfers && !powered_off && !app.slot1.icon.ready);
    fixture();
    keys = false;
    settle();
    assert(!app.slot1.icon.ready && key_reads == 1);
    fixture();
    header[0] ^= 1;
    settle();
    assert(!app.slot1.icon.ready && !key_reads);
    fixture();
    put32(header + 0x68, UINT32_MAX);
    put16(header + 0x15E, slot1_crc16(header, 0x15E));
    settle();
    assert(!app.slot1.icon.ready && !key_reads);
    fixture();
    banner[0x220] ^= 1;
    settle();
    assert(!app.slot1.icon.ready && !*app.slot1.title);
    fixture();
    fail_transfer = true;
    settle();
    assert(!app.slot1.icon.ready && transfers == 1);
    fixture();
    app.entries[0].target = 0;
    for (unsigned i = 0; i < 100; i++)
        slot1_tick(&app, &probe, &operations);
    assert(!transfers && !powered_on);
    fixture();
    slot1_tick(&app, &probe, &operations);
    present = false;
    slot1_tick(&app, &probe, &operations);
    assert(!probe.phase && !app.slot1.icon.ready);

    fixture();
    put16(banner, 0x103);
    memset(banner + 0x1240, 0x11, 4096);
    put16(banner + 0x2242, 31);
    put16(banner + 0x2262, 0x3E0);
    put16(banner + 0x2340, 1);
    put16(banner + 0x2342, 0x0901);
    put16(banner + 8, slot1_crc16(banner + 0x1240, 0x1180));
    assert(slot1_publish_banner(&app, banner, 0x23C0));
    assert(app.slot1.icon.animation_count == 2);
    icon_animate(&app.slot1.icon, false);
    assert(app.slot1.icon.pixels[0] == 0x83E0);
    icon_animate(&app.slot1.icon, true);
    assert(app.slot1.icon.pixels[0] == 0x801F);
    banner[0x1240] ^= 1;
    assert(slot1_publish_banner(&app, banner, 0x23C0));
    assert(app.slot1.icon.ready && !app.slot1.icon.animation_count);
    puts("Slot-1 metadata: bounded reads, insertion/removal, replacement, validation and animation "
         "pass");
    return 0;
}
