/* Recognize explicit SDK markers or structurally valid, ARM9-linked DLDI.
 * This is launch preflight only, never a browser-wide file scan. */
#include "game_options.h"
#include "launch.h"
#include <stdio.h>
#include <string.h>

static uint32_t word(const unsigned char *bytes)
{
    return bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}
static bool linked_dldi(const unsigned char *header, uint32_t address, uint32_t available)
{
    static const unsigned char signature[12] = {0xED, 0xA5, 0x8D, 0xBF, ' ', 'C',
                                                'h',  'i',  's',  'h',  'm', 0};
    if (memcmp(header, signature, sizeof signature) || header[12] != 1 || header[13] < 7 ||
        header[13] > 15 || header[15] < header[13] || header[15] > 15 || (header[14] & ~15))
        return false;
    uint32_t size = 1u << header[13], start = word(header + 0x40), end = word(header + 0x44);
    if (size > available || (1u << header[15]) > available || start != address ||
        end <= start + 128 || end - start > size)
        return false;
    for (unsigned offset = 0x68; offset < 0x80; offset += 4) {
        uint32_t function = word(header + offset) & ~1u;
        if (function < start + 128 || function >= end)
            return false;
    }
    return true;
}
bool launch_is_homebrew_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned char header[0x200];
    bool valid = fread(header, 1, sizeof header, file) == sizeof header;
    if (valid && launch_is_homebrew_header(header)) {
        fclose(file);
        return true;
    }
    bool retail = false;
    if (valid)
        game_save_size(word(header + 12), &retail);
    if (!valid || retail) {
        fclose(file);
        return false;
    }
    /* Small applications may not need a filesystem driver. Accept the exact
     * SDK default title with an unset maker, after excluding known retail IDs.
     * Normal binary-range validation still runs before any direct handoff. */
    static const char default_title[12] = "HOMEBREW";
    if (!memcmp(header, default_title, sizeof default_title) && !header[0x10] && !header[0x11]) {
        fclose(file);
        return true;
    }
    if (fseek(file, 0, SEEK_END)) {
        fclose(file);
        return false;
    }
    long bytes = ftell(file);
    uint32_t offset = word(header + 0x20), destination = word(header + 0x28),
             length = word(header + 0x2C);
    valid = bytes >= 0 && offset >= 0x200 && !(offset & 3) && length >= 128 && length <= 0x380000 &&
            offset <= (uint32_t)bytes && length <= (uint32_t)bytes - offset &&
            destination >= 0x02000000 && destination < 0x02FF4000 &&
            length <= 0x02FF4000 - destination;
    bool found = false;
    unsigned char block[1024 + 128];
    for (uint32_t position = 0; valid && !found && position + 128 <= length; position += 1024) {
        size_t count = length - position;
        if (count > sizeof block)
            count = sizeof block;
        if (fseek(file, offset + position, SEEK_SET) || fread(block, 1, count, file) != count)
            break;
        for (unsigned i = 0; i < 1024 && i + 128 <= count; i += 4) {
            if (linked_dldi(block + i, destination + position + i, length - position - i)) {
                found = true;
                break;
            }
        }
    }
    fclose(file);
    return found;
}
