/* Launcher warmboot request layout, also used by the reference launchers.
 * NAND paths are derived only from discovered metadata; no NAND writes or
 * Unlaunch configuration changes occur here. */
#include "system_routes.h"
#include <stdio.h>
#include <string.h>

static const char *const destinations[] = {
    "system:settings",      "system:camera", "system:sound", "system:pictochat",
    "system:download-play", "system:menu",   "system:slot1"};
static const char *const destination_names[] = {
    "DSi System Settings", "Nintendo DSi Camera",       "Nintendo DSi Sound", "PictoChat",
    "DS Download Play",    "Return to DSi System Menu", "Launch Slot-1 card"};
int system_destination(const char *uri)
{
    if (uri)
        for (unsigned i = 0; i < sizeof destinations / sizeof *destinations; i++)
            if (!strcmp(uri, destinations[i]))
                return (int)i;
    return -1;
}
const char *system_destination_uri(int target)
{
    return target >= 0 && target < 7 ? destinations[target] : "";
}
const char *system_destination_name(int target)
{
    return target >= 0 && target < 7 ? destination_names[target] : "System destination";
}

static void request_crc(uint32_t request[8])
{
    uint16_t crc = 0xFFFF;
    const unsigned char *data = (const unsigned char *)(request + 2);
    for (int i = 0; i < 24; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    request[1] |= (uint32_t)crc << 16;
}
void system_menu_request(uint32_t request[8])
{
    memset(request, 0, 8 * sizeof *request);
    request[0] = 0x434E4C54; /* TLNC, stored little-endian. */
    request[1] = 0x1801;
    /* A zero new-title ID requests Launcher; it takes precedence over type. */
    request[6] = 0x13;
    request_crc(request);
}
void system_card_request(uint32_t request[8])
{
    memset(request, 0, 8 * sizeof *request);
    request[0] = 0x434E4C54;
    request[1] = 0x1801;
    request[2] = request[4] = 0x43415254;
    request[6] = 0x13;
    request_crc(request);
}
bool system_title_request(uint32_t request[8], const NativeTitle *title)
{
    if (!title || !title->available || !title->low ||
        (title->high != 0x00030004 && title->high != 0x00030005 && title->high != 0x00030015 &&
         title->high != 0x00030017))
        return false;
    memset(request, 0, 8 * sizeof *request);
    request[0] = 0x434E4C54;
    request[1] = 0x1801;
    request[2] = request[4] = title->low;
    request[3] = request[5] = title->high;
    request[6] = 0x17;
    request_crc(request);
    return true;
}

static void autoload_path(unsigned char request[0x400], const char *path)
{
    memset(request, 0, 0x400);
    memcpy(request, "AutoLoadInfo", 12);
    request[0x0C] = 0xF0;
    request[0x0D] = 0x03;
    request[0x10] = 3; /* Load the path and use the specified background colors. */
    request[0x14] = request[0x16] = 0xFF;
    request[0x15] = request[0x17] = 0x7F;
    /* Generated paths are ASCII, represented as zero-terminated UTF-16LE. */
    for (size_t i = 0; path[i]; i++)
        request[0x38 + 2 * i] = (unsigned char)path[i];
    uint16_t crc = 0xFFFF;
    for (unsigned i = 0x10; i < 0x400; i++) {
        crc ^= request[i];
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    request[0x0E] = (unsigned char)crc;
    request[0x0F] = (unsigned char)(crc >> 8);
}

void system_frontend_autoload(unsigned char request[0x400])
{
    autoload_path(request, "sdmc:/BOOT.NDS");
}

void system_card_autoload(unsigned char request[0x400])
{
    autoload_path(request, "cart:");
}

bool system_title_autoload(unsigned char request[0x400], const NativeTitle *title)
{
    uint32_t native_request[8];
    memset(request, 0, 0x400);
    if (!system_title_request(native_request, title))
        return false;
    char path[96];
    snprintf(path, sizeof path, "nand:/title/%08lx/%08lx/content/%08lx.app",
             (unsigned long)title->high, (unsigned long)title->low,
             (unsigned long)title->boot_content);
    autoload_path(request, path);
    return true;
}
