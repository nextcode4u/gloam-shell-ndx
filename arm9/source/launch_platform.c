#ifndef HOST_PREVIEW
/* DSi-SD adapter for the pinned Chishm/WinterMute-derived loader.
 * See third_party/nds-bootloader/UPSTREAM.md and its retained licenses. */
#include "launch.h"
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

extern const unsigned char frontend_loader_start[], frontend_loader_end[];
extern const unsigned char frontend_bootstub_start[], frontend_bootstub_end[];

static void copy_words(volatile uint32_t *destination, const uint32_t *source, size_t bytes)
{
    for (size_t index = 0; index < (bytes + 3) / 4; index++)
        destination[index] = source[index];
}
bool launch_platform(const LaunchPlan *plan, char error[160])
{
    struct stat st;
    if (!isDSiMode() || strncmp(plan->application, "sd:/", 4)) {
        snprintf(error, 160, "This adapter supports the DSi SD card only.");
        return false;
    }
    if (stat(plan->application, &st) || st.st_ino < 2) {
        snprintf(error, 160, "Could not resolve the application's FAT cluster.");
        return false;
    }
    size_t loader_size = (size_t)(frontend_loader_end - frontend_loader_start);
    size_t stub_size = (size_t)(frontend_bootstub_end - frontend_bootstub_start);
    const uint32_t *header = (const uint32_t *)frontend_loader_start;
    size_t offset = ((size_t)header[4] + 3) & ~(size_t)3;
    if (loader_size + stub_size > 0x6000 || stub_size % 4 || offset > 128 * 1024 ||
        (size_t)plan->arguments.used + 3 > 128 * 1024 - offset) {
        snprintf(error, 160, "Loader or argument data exceeds reserved memory.");
        return false;
    }
    /* Copy through ARM9's mirror but publish canonical pointers for both CPUs. */
    struct __bootstub *stub = __system_bootstub;
    memcpy(stub, frontend_bootstub_start, stub_size);
    uint32_t *return_loader = (uint32_t *)((unsigned char *)stub + stub_size);
    memcpy(return_loader, frontend_loader_start, loader_size);
    struct stat frontend;
    if (!stat("sd:/BOOT.NDS", &frontend) && frontend.st_ino >= 2)
        return_loader[1] = (uint32_t)frontend.st_ino;
    return_loader[3] = 0;
    return_loader[7] = 1;
    return_loader[8] = 1;
    stub->arm9reboot = (VoidFn)(0x02FF4000u + (uintptr_t)stub->arm9reboot);
    stub->arm7reboot = (VoidFn)(0x02FF4000u + (uintptr_t)stub->arm7reboot);
    stub->bootsize = loader_size;
    DC_FlushAll();
    irqDisable(IRQ_ALL);
    VRAM_C_CR = VRAM_ENABLE | VRAM_C_LCD;
    volatile uint32_t *loader = (volatile uint32_t *)0x06840000;
    copy_words(loader, (const uint32_t *)frontend_loader_start, loader_size);
    loader[1] = (uint32_t)st.st_ino;
    loader[2] = 1;
    loader[3] = 0;
    loader[7] = 1;
    loader[8] = 1;
    copy_words((volatile uint32_t *)((uintptr_t)loader + offset),
               (const uint32_t *)plan->arguments.data, (size_t)plan->arguments.used);
    loader[4] = offset;
    loader[5] = (uint32_t)plan->arguments.used;
    DC_FlushAll();
    VRAM_C_CR = VRAM_ENABLE | VRAM_C_ARM7_0x06000000;
    REG_EXMEMCNT |= ARM7_OWNS_ROM | ARM7_OWNS_CARD;
    *(volatile uint32_t *)0x02FFFFFC = 0;
    *(volatile uint32_t *)0x02FFFE04 = 0xE59FF018;
    *(volatile uint32_t *)0x02FFFE24 = 0x02FFFE04;
    resetARM7(0x06000000);
    swiSoftReset();
    return true;
}
#endif
