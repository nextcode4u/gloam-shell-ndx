/* Recognize explicit SDK markers or structurally valid, ARM9-linked DLDI.
 * This is launch preflight only, never a browser-wide file scan. */
#include "game_options.h"
#include "launch.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

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
    struct stat info;
    valid = !fstat(fileno(file), &info) && S_ISREG(info.st_mode) && info.st_size >= 0 &&
            (uint64_t)info.st_size <= UINT32_MAX;
    uint32_t bytes = valid ? (uint32_t)info.st_size : 0;
    uint32_t offset = word(header + 0x20), destination = word(header + 0x28),
             length = word(header + 0x2C);
    valid = valid && offset >= 0x200 && !(offset & 3) && length >= 128 && length <= 0x380000 &&
            offset <= bytes && length <= bytes - offset && destination >= 0x02000000 &&
            destination < 0x02FF4000 && length <= 0x02FF4000 - destination;
    if (valid)
        valid = !fseek(file, offset, SEEK_SET);
    bool found = false;
    unsigned char block[1024 + 128];
    size_t retained = 0;
    /* Keep the look-ahead bytes instead of seeking backwards each KiB. On FAT,
     * those backwards seeks can repeatedly traverse the file's cluster chain. */
    for (uint32_t position = 0; valid && !found && position + 128 <= length; position += 1024) {
        size_t count = length - position;
        if (count > sizeof block)
            count = sizeof block;
        size_t next = count - retained;
        if (next && fread(block + retained, 1, next, file) != next)
            break;
        for (unsigned i = 0; i < 1024 && i + 128 <= count; i += 4) {
            if (linked_dldi(block + i, destination + position + i, length - position - i)) {
                found = true;
                break;
            }
        }
        retained = count > 1024 ? count - 1024 : 0;
        if (retained)
            memmove(block, block + 1024, retained);
    }
    fclose(file);
    return found;
}
