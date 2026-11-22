#include "image_input.h"
#include "image_job.h"
#include <stdint.h>
#include <sys/stat.h>

bool image_file_size(FILE *file, size_t minimum, size_t maximum, size_t *bytes)
{
    if (!bytes)
        return false;
    *bytes = 0;
    struct stat info;
    if (!file || minimum > maximum || fstat(fileno(file), &info) || !S_ISREG(info.st_mode) ||
        info.st_size < 0 || (uint64_t)info.st_size < minimum || (uint64_t)info.st_size > maximum)
        return false;
    *bytes = (size_t)info.st_size;
    return true;
}

bool image_read_exact(FILE *file, void *destination, size_t bytes)
{
    unsigned char *output = destination;
    while (bytes) {
        if (image_job_cancelled())
            return false;
        size_t chunk = bytes < 4096 ? bytes : 4096;
        if (fread(output, 1, chunk, file) != chunk)
            return false;
        output += chunk;
        bytes -= chunk;
        image_decode_checkpoint();
    }
    return !image_job_cancelled();
}
