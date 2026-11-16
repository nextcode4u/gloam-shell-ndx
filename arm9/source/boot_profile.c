/* Bounded wall-time observations; collected once and exported only on request. */
#include "boot_profile.h"
#include <string.h>
static BootProfile profile;
static uint32_t (*clock_now)(void);
static uint32_t started, phase_started[BOOT_PHASE_COUNT];
static unsigned active;
void boot_profile_begin(uint32_t (*clock_ms)(void))
{
    memset(&profile, 0, sizeof profile);
    active = 0;
    clock_now = clock_ms;
    started = clock_now ? clock_now() : 0;
}
void boot_profile_enter(BootPhase phase)
{
    if (!clock_now || (unsigned)phase >= BOOT_PHASE_COUNT || (active & (1u << phase)))
        return;
    phase_started[phase] = clock_now();
    active |= 1u << phase;
}
void boot_profile_leave(BootPhase phase)
{
    if (!clock_now || (unsigned)phase >= BOOT_PHASE_COUNT || !(active & (1u << phase)))
        return;
    profile.milliseconds[phase] += clock_now() - phase_started[phase];
    profile.measured |= 1u << phase;
    active &= ~(1u << phase);
}
void boot_profile_finish(void)
{
    if (!clock_now)
        return;
    profile.total_ms = clock_now() - started;
    profile.complete = true;
    active = 0;
    clock_now = NULL;
}
const BootProfile *boot_profile_get(void)
{
    return &profile;
}
const char *boot_profile_name(BootPhase phase)
{
    static const char *names[] = {"hardware",    "sd_mount",   "configuration", "associations",
                                  "dashboard",   "theme",      "script_fonts",  "first_folder",
                                  "splash_wait", "first_audio"};
    _Static_assert(sizeof names / sizeof *names == BOOT_PHASE_COUNT, "boot phase labels");
    return (unsigned)phase < BOOT_PHASE_COUNT ? names[phase] : "unknown";
}
