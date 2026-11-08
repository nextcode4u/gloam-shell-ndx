/* The installed tile contains no launcher assets and never writes NAND.
 * Unlaunch consumes the RAM request and boots the replaceable SD entry point. */
#include "system_routes.h"
#include "target.h"
#include <fat.h>
#include <nds.h>
#include <stdio.h>
#include <string.h>

static bool target_ready(void)
{
    if (!isDSiMode() || !fatInitDefault())
        return false;
    FILE *file = fopen("sd:/BOOT.NDS", "rb");
    if (!file)
        return false;
    unsigned char header[0x240];
    size_t bytes = fread(header, 1, sizeof header, file);
    bool measured = !fseek(file, 0, SEEK_END);
    long size = measured ? ftell(file) : -1;
    fclose(file);
    return size >= 0 && forwarder_target_valid(header, bytes, (uint64_t)size);
}
static void open_gloam(void)
{
    unsigned char request[0x400];
    system_frontend_autoload(request);
    /* Discard a stale Launcher title request from the application we left. */
    memset((void *)0x02000300, 0, 32);
    memcpy((void *)0x02000800, request, sizeof request);
    DC_FlushAll();
    systemReboot();
    while (1)
        swiWaitForVBlank();
}
int main(void)
{
    if (target_ready())
        open_gloam();
    consoleDemoInit();
    printf("Gloam Shell NDX\n\n");
    printf("Could not open sd:/BOOT.NDS\n\n");
    printf("Insert your Gloam SD card.\n"
            "Keep the launcher at BOOT.NDS.\n"
            "Do not rename this tile to\nBOOT.NDS.\n\n"
            "Requires DSi mode and Unlaunch.\n\nA: retry\nB: DSi Menu\n");
    while (1) {
        swiWaitForVBlank();
        scanKeys();
        unsigned pressed = keysDown();
        if ((pressed & KEY_A) && target_ready())
            open_gloam();
        if (pressed & KEY_B) {
            uint32_t request[8];
            system_menu_request(request);
            memset((void *)0x02000800, 0, 0x400);
            memcpy((void *)0x02000300, request, sizeof request);
            DC_FlushAll();
            systemReboot();
        }
    }
}
