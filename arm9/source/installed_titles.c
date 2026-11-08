/* Read-only installed-title discovery. Data/save directories are never opened. */
#include "installed_titles.h"
#include "paths.h"
#include "system_routes.h"
#include "title_metadata.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static bool title_directory(const InstalledCatalog *catalog, uint32_t id, char path[PATH_CAP])
{
    char relative[40];
    snprintf(relative, sizeof relative, "title/00030004/%08lx", (unsigned long)id);
    return path_join(path, catalog->root, relative);
}
bool installed_title_path(const InstalledCatalog *catalog, unsigned index, char path[PATH_CAP])
{
    if (index >= catalog->count)
        return false;
    char directory[PATH_CAP], relative[40];
    const NativeTitle *title = &catalog->titles[index].title;
    snprintf(relative, sizeof relative, "content/%08lx.app", (unsigned long)title->boot_content);
    return title_directory(catalog, title->low, directory) && path_join(path, directory, relative);
}
static uint32_t le32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static bool app_identity(const char *path, const NativeTitle *title, uint32_t *size)
{
    FILE *file = fopen(path, "rb");
    unsigned char header[0x240];
    bool valid = file && fread(header, 1, sizeof header, file) == sizeof header &&
                 (header[0x12] == 2 || header[0x12] == 3) && le32(header + 0x230) == title->low &&
                 le32(header + 0x234) == title->high && !fseek(file, 0, SEEK_END);
    long bytes = valid ? ftell(file) : -1;
    valid = valid && bytes >= (long)sizeof header && (uint64_t)bytes <= UINT32_MAX;
    for (unsigned cpu = 0; valid && cpu < 2; cpu++) {
        unsigned at = 0x20 + cpu * 16;
        uint32_t start = le32(header + at), length = le32(header + at + 12);
        valid = start >= 0x200 && !(start & 3) && length >= 4 && start <= (uint32_t)bytes &&
                length <= (uint32_t)bytes - start;
    }
    if (valid)
        *size = (uint32_t)bytes;
    if (file)
        fclose(file);
    return valid;
}
bool installed_title_revalidate(const InstalledCatalog *catalog, unsigned index)
{
    if (!catalog->ready || index >= catalog->count)
        return false;
    const InstalledTitle *item = &catalog->titles[index];
    char directory[PATH_CAP], path[PATH_CAP];
    uint32_t boot = 0, bytes = 0;
    return item->title.available && title_directory(catalog, item->title.low, directory) &&
           native_title_content(directory, 0x00030004, item->title.low, &boot, true) &&
           boot == item->title.boot_content && installed_title_path(catalog, index, path) &&
           app_identity(path, &item->title, &bytes) && bytes == item->bytes;
}
void installed_catalog_cancel(InstalledCatalog *catalog)
{
    if (catalog->directory)
        closedir(catalog->directory);
    catalog->directory = NULL;
    catalog->scanning = false;
}
static int compare_titles(const void *a, const void *b)
{
    const InstalledTitle *left = a, *right = b;
    int order = strcasecmp(left->name, right->name);
    return order ? order
                 : (left->title.low > right->title.low) - (left->title.low < right->title.low);
}
static void finish(InstalledCatalog *catalog)
{
    installed_catalog_cancel(catalog);
    qsort(catalog->titles, catalog->count, sizeof *catalog->titles, compare_titles);
    catalog->ready = true;
    snprintf(catalog->status, sizeof catalog->status, "%u installed title%s%s%s", catalog->count,
             catalog->count == 1 ? "" : "s", catalog->truncated ? "; scan limit reached" : "",
             catalog->skipped ? "; unsupported entries skipped" : "");
}
void installed_catalog_begin(InstalledCatalog *catalog, const char *nand_root, int language)
{
    installed_catalog_cancel(catalog);
    memset(catalog, 0, sizeof *catalog);
    catalog->attempted = true;
    catalog->language = language;
    path_copy(catalog->root, nand_root, sizeof catalog->root);
    char directory[PATH_CAP];
    if (path_join(directory, nand_root, "title/00030004"))
        catalog->directory = opendir(directory);
    catalog->scanning = catalog->directory != NULL;
    path_copy(catalog->status,
              catalog->scanning ? "Reading installed NAND titles..."
                                : "No readable installed DSiWare directory.",
              sizeof catalog->status);
}
bool installed_catalog_step(InstalledCatalog *catalog)
{
    if (!catalog->scanning)
        return false;
    struct dirent *entry = readdir(catalog->directory);
    if (!entry || catalog->scanned++ >= 4096) {
        catalog->truncated |= entry != NULL;
        finish(catalog);
        return true;
    }
    if (strlen(entry->d_name) != 8)
        return false;
    for (int i = 0; i < 8; i++)
        if (!isxdigit((unsigned char)entry->d_name[i]))
            return false;
    if (catalog->count == INSTALLED_TITLE_CAP) {
        catalog->truncated = true;
        finish(catalog);
        return true;
    }
    uint32_t id = (uint32_t)strtoul(entry->d_name, NULL, 16), boot = 0;
    char directory[PATH_CAP], path[PATH_CAP];
    InstalledTitle *item = &catalog->titles[catalog->count];
    if (!id || !title_directory(catalog, id, directory) ||
        !native_title_content(directory, 0x00030004, id, &boot, true)) {
        catalog->skipped++;
        return false;
    }
    item->title = (NativeTitle){0x00030004, id, true, boot};
    /* Include this slot temporarily so the same path builder checks every use. */
    catalog->count++;
    bool valid = installed_title_path(catalog, catalog->count - 1, path) &&
                 app_identity(path, &item->title, &item->bytes);
    if (!valid) {
        catalog->count--;
        memset(item, 0, sizeof *item);
        catalog->skipped++;
        return false;
    }
    if (!title_metadata_read(path, catalog->language, item->name))
        snprintf(item->name, sizeof item->name, "DSiWare %08lx", (unsigned long)id);
    return false;
}
