#include "target.h"
#include <string.h>

static uint32_t le32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
bool forwarder_target_valid(const unsigned char *header, size_t bytes, uint64_t file_size)
{
    if (bytes < 0x240 || (header[0x12] != 2 && header[0x12] != 3) ||
        !memcmp(header + 12, "GLDX", 4))
        return false;
    for (int cpu = 0; cpu < 2; cpu++) {
        unsigned offset = 0x20 + cpu * 16;
        uint32_t start = le32(header + offset), length = le32(header + offset + 12);
        if (start < 0x200 || (start & 3) || length < 4 || start > file_size ||
            length > file_size - start)
            return false;
    }
    return true;
}
