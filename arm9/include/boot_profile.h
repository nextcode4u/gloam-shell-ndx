#ifndef GLOAM_BOOT_PROFILE_H
#define GLOAM_BOOT_PROFILE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    BOOT_HARDWARE,
    BOOT_SD,
    BOOT_CONFIG,
    BOOT_ASSOCIATIONS,
    BOOT_DASHBOARD,
    BOOT_THEME,
    BOOT_FONTS,
    BOOT_FOLDER,
    BOOT_SPLASH,
    BOOT_AUDIO,
    BOOT_PHASE_COUNT
} BootPhase;
typedef struct {
    uint32_t milliseconds[BOOT_PHASE_COUNT], total_ms;
    unsigned measured;
    bool complete;
} BootProfile;
/* Platform owns the clock/timers; these helpers do not allocate or write files. */
void boot_profile_begin(uint32_t (*clock_ms)(void));
void boot_profile_enter(BootPhase phase);
void boot_profile_leave(BootPhase phase);
void boot_profile_finish(void);
const BootProfile *boot_profile_get(void);
const char *boot_profile_name(BootPhase phase);
#endif
