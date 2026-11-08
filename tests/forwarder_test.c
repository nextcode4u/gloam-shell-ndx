#include "system_routes.h"
#include "target.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void le32(unsigned char *p, uint32_t n)
{
    for (int i = 0; i < 4; i++)
        p[i] = n >> (i * 8);
}
int main(void)
{
    unsigned char request[0x400];
    system_frontend_autoload(request);
    assert(!memcmp(request, "AutoLoadInfo", 12));
    const char *target = "sdmc:/BOOT.NDS";
    for (size_t i = 0; i <= strlen(target); i++) {
        assert(request[0x38 + i * 2] == target[i]);
        assert(!request[0x39 + i * 2]);
    }
    unsigned crc = 0xFFFF;
    for (int i = 0x10; i < 0x400; i++) {
        crc ^= request[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    assert(crc && request[14] == (crc & 255) && request[15] == (crc >> 8));
    unsigned char header[0x240] = {0};
    header[0x12] = 2;
    le32(header + 0x20, 0x4000);
    le32(header + 0x2C, 0x1000);
    le32(header + 0x30, 0x5000);
    le32(header + 0x3C, 0x1000);
    assert(forwarder_target_valid(header, sizeof header, 0x6000));
    assert(!forwarder_target_valid(header, 512, 0x6000));
    assert(!forwarder_target_valid(header, sizeof header, 0x5FFF));
    memcpy(header + 12, "GLDX", 4);
    assert(!forwarder_target_valid(header, sizeof header, 0x6000));
    memset(header + 12, 0, 4);
    le32(header + 0x2C, 0xFFFFFFFF);
    assert(!forwarder_target_valid(header, sizeof header, 0x6000));
    puts("PASS: forwarder SD request/CRC, executable bounds and self-forwarding rejection");
}
