/* Read-only directory metadata, independent of the native filesystem ABI. */
#ifndef DIRECTORY_SCAN_H
#define DIRECTORY_SCAN_H
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    char name[256];
    uint32_t size;
    time_t modified;
    bool valid, directory, regular, hidden, system;
} DirectoryRecord;

void *directory_scan_open(const char *path);
bool directory_scan_read(void *directory, DirectoryRecord *record);
bool directory_scan_is_save_folder(const DirectoryRecord *record);
bool directory_scan_is_bios_folder(const DirectoryRecord *record);
void directory_scan_close(void *directory);
#endif
