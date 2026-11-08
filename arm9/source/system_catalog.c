/* Read-only, bounded discovery. Regional title IDs come from the mounted NAND,
 * not from another console or a guessed region. No NAND write APIs are used. */
#include "paths.h"
#include "system_routes.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static uint32_t be32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
bool native_title_content(const char *directory, uint32_t high, uint32_t low,
                          uint32_t *boot_content, bool allow_unsigned)
{
    char path[PATH_CAP];
    if (!path_join(path, directory, "content/title.tmd"))
        return false;
    FILE *file = fopen(path, "rb");
    unsigned char header[0x1E4], content[36];
    bool valid = file && fread(header, 1, sizeof header, file) == sizeof header;
    /* The supported DSi metadata layout has an RSA-2048 signature prefix.
     * This is an identity/size check, not Nintendo signature verification. */
    valid = valid && (be32(header) == 0x00010001 || (allow_unsigned && !be32(header))) &&
            be32(header + 0x18C) == high && be32(header + 0x190) == low;
    unsigned count = valid ? header[0x1DE] << 8 | header[0x1DF] : 0;
    unsigned boot_index = valid ? header[0x1E0] << 8 | header[0x1E1] : 0;
    bool found_boot = false;
    valid = valid && count > 0 && count <= 16;
    for (unsigned i = 0; valid && i < count; i++) {
        valid = fread(content, 1, sizeof content, file) == sizeof content;
        if (!valid)
            break;
        uint64_t bytes = (uint64_t)be32(content + 8) << 32 | be32(content + 12);
        char name[64];
        snprintf(name, sizeof name, "content/%08lx.app", (unsigned long)be32(content));
        struct stat status;
        valid = bytes >= 0x200 && bytes <= UINT32_MAX && path_join(path, directory, name) &&
                !stat(path, &status) && S_ISREG(status.st_mode) &&
                (uint64_t)status.st_size == bytes;
        if (valid && (unsigned)(content[4] << 8 | content[5]) == boot_index) {
            if (found_boot) {
                valid = false;
                break;
            }
            *boot_content = be32(content);
            found_boot = true;
        }
    }
    if (file)
        fclose(file);
    return valid && found_boot;
}
void system_catalog_scan(SystemCatalog *catalog, const char *nand_root)
{
    memset(catalog, 0, sizeof *catalog);
    catalog->attempted = true;
    static const uint32_t groups[] = {0x00030015, 0x00030005, 0x00030017};
    static const uint32_t prefixes[] = {0x484E4200, 0x484E4900, 0x484E4B00,
                                        0x484E4500, 0x484E4400, 0x484E4100};
    static const uint32_t categories[] = {0x00030015, 0x00030005, 0x00030005,
                                          0x00030005, 0x00030005, 0x00030017};
    unsigned readable = 0;
    bool ambiguous[6] = {false};
    for (unsigned group = 0; group < sizeof groups / sizeof groups[0]; group++) {
        char name[64], directory[PATH_CAP];
        snprintf(name, sizeof name, "title/%08lx", (unsigned long)groups[group]);
        if (!path_join(directory, nand_root, name))
            continue;
        DIR *folder = opendir(directory);
        if (!folder)
            continue;
        readable++;
        struct dirent *entry;
        unsigned scanned = 0;
        while ((entry = readdir(folder)) && scanned++ < 128) {
            if (strlen(entry->d_name) != 8)
                continue;
            char *end;
            unsigned long id = strtoul(entry->d_name, &end, 16);
            if (*end || id > UINT32_MAX)
                continue;
            for (int i = 0; i < 6; i++) {
                if (groups[group] != categories[i] || (id & 0xFFFFFF00) != prefixes[i])
                    continue;
                char title[PATH_CAP];
                uint32_t boot_content = 0;
                if (!path_join(title, directory, entry->d_name) ||
                    !native_title_content(title, groups[group], (uint32_t)id, &boot_content, false))
                    continue;
                NativeTitle *native = i < 5 ? &catalog->titles[i] : &catalog->launcher;
                if (native->available)
                    ambiguous[i] = true;
                *native = (NativeTitle){groups[group], (uint32_t)id, true, boot_content};
                char content[48];
                snprintf(content, sizeof content, "content/%08lx.app", (unsigned long)boot_content);
                if (i < 5)
                    path_join(catalog->content_paths[i], title, content);
            }
        }
        closedir(folder);
    }
    for (int i = 0; i < 5; i++)
        if (ambiguous[i]) {
            catalog->titles[i].available = false;
            catalog->content_paths[i][0] = 0;
        }
    if (ambiguous[5])
        catalog->launcher.available = false;
    catalog->ready = readable != 0;
    path_copy(catalog->status,
              readable ? "Title is missing, ambiguous or has unsupported metadata. Detected-title "
                         "warmboot is experimental; exit behavior depends on the boot environment."
                       : "NAND title directories are unavailable. Native system apps cannot be "
                         "launched from this environment.",
              sizeof catalog->status);
}
