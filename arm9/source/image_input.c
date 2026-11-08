#include "image_input.h"
#include "image_job.h"

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
