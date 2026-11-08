/* Large FAT directories must not restart a pathname search for every entry. */
#include "directory_scan.h"
#include "paths.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifndef HOST_PREVIEW
#include <blocksds_version.h>
#if BLOCKSDS_VERSION_EQUALS(1, 25, 0)
#define NATIVE_DIRECTORY_METADATA
#endif
#endif

bool directory_scan_is_save_folder(const DirectoryRecord *record)
{
    return record->directory &&
           (!strcasecmp(record->name, "saves") || !strcasecmp(record->name, "savestates"));
}

bool directory_scan_is_bios_folder(const DirectoryRecord *record)
{
    return record->directory && !strcasecmp(record->name, "bios");
}

#ifdef NATIVE_DIRECTORY_METADATA
/* These exact headers match the FatFs linked by the pinned SDK. */
#define f_opendir libnds_f_opendir
#define f_readdir libnds_f_readdir
#define f_closedir libnds_f_closedir
#include "../../third_party/fatfs-headers/ff.h"

void *directory_scan_open(const char *path)
{
    DIR *directory = calloc(1, sizeof *directory);
    if (directory && f_opendir(directory, path) != FR_OK) {
        free(directory);
        return NULL;
    }
    return directory;
}

bool directory_scan_read(void *directory, DirectoryRecord *record)
{
    FILINFO info = {0};
    if (f_readdir(directory, &info) != FR_OK) {
        errno = EIO;
        return false;
    }
    if (!info.fname[0])
        return false;
    memset(record, 0, sizeof *record);
    path_copy(record->name, info.fname, sizeof record->name);
    record->valid = true;
    record->directory = (info.fattrib & AM_DIR) != 0;
    record->regular = !record->directory;
    record->hidden = (info.fattrib & AM_HID) != 0;
    record->system = (info.fattrib & AM_SYS) != 0;
    record->size = record->directory ? 0 : info.fsize;
    /* Match the SDK's stat() timestamp conversion, including invalid dates. */
    struct tm date = {0};
    date.tm_year = (info.fdate >> 9) + 80;
    date.tm_mon = ((info.fdate >> 5) & 15) - 1;
    date.tm_mday = info.fdate & 31;
    date.tm_hour = info.ftime >> 11;
    date.tm_min = (info.ftime >> 5) & 63;
    date.tm_sec = (info.ftime & 31) * 2;
    record->modified = mktime(&date);
    if (record->modified == (time_t)-1)
        record->modified = 0;
    return true;
}

void directory_scan_close(void *directory)
{
    if (directory) {
        f_closedir(directory);
        free(directory);
    }
}
#else
#include <dirent.h>
#include <sys/stat.h>
#ifndef HOST_PREVIEW
#include <fat.h>
#endif

typedef struct {
    DIR *handle;
    char path[PATH_CAP];
} PosixDirectory;

void *directory_scan_open(const char *path)
{
    PosixDirectory *directory = calloc(1, sizeof *directory);
    if (!directory)
        return NULL;
    directory->handle = opendir(path);
    if (!directory->handle) {
        free(directory);
        return NULL;
    }
    path_copy(directory->path, path, sizeof directory->path);
    return directory;
}

bool directory_scan_read(void *iterator, DirectoryRecord *record)
{
    PosixDirectory *directory = iterator;
    struct dirent *entry = readdir(directory->handle);
    if (!entry)
        return false;
    memset(record, 0, sizeof *record);
    path_copy(record->name, entry->d_name, sizeof record->name);
    char path[PATH_CAP];
    struct stat status;
    if (!path_join(path, directory->path, entry->d_name) || stat(path, &status))
        return true;
    record->valid = true;
    record->directory = S_ISDIR(status.st_mode);
    record->regular = S_ISREG(status.st_mode);
    record->size = S_ISREG(status.st_mode) ? (uint32_t)status.st_size : 0;
    record->modified = status.st_mtime;
#ifndef HOST_PREVIEW
    int attributes = FAT_getAttr(path);
    record->hidden = attributes >= 0 && (attributes & ATTR_HIDDEN);
    record->system = attributes >= 0 && (attributes & ATTR_SYSTEM);
#endif
    return true;
}

void directory_scan_close(void *iterator)
{
    PosixDirectory *directory = iterator;
    if (directory) {
        closedir(directory->handle);
        free(directory);
    }
}
#endif
