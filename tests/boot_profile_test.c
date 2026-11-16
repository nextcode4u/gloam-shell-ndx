#include "boot_profile.h"
#include <assert.h>
#include <stdio.h>
static uint32_t now;
static uint32_t clock_ms(void)
{
    return now;
}
int main(void)
{
    boot_profile_begin(NULL);
    boot_profile_enter(BOOT_SD);
    boot_profile_leave(BOOT_SD);
    boot_profile_finish();
    assert(!boot_profile_get()->complete);
    now = 100;
    boot_profile_begin(clock_ms);
    boot_profile_enter(BOOT_SD);
    now += 25;
    boot_profile_enter(BOOT_SD); /* Does not restart an active region. */
    now += 5;
    boot_profile_leave(BOOT_SD);
    boot_profile_leave(BOOT_SD); /* An inactive region cannot accumulate time. */
    now += 70;
    boot_profile_finish();
    assert(boot_profile_get()->complete && boot_profile_get()->total_ms == 100);
    assert(boot_profile_get()->milliseconds[BOOT_SD] == 30);
    now += 100;
    boot_profile_enter(BOOT_THEME);
    boot_profile_leave(BOOT_THEME);
    assert(!boot_profile_get()->milliseconds[BOOT_THEME]);
    now = UINT32_MAX - 5;
    boot_profile_begin(clock_ms);
    boot_profile_enter(BOOT_CONFIG);
    now = 4;
    boot_profile_leave(BOOT_CONFIG);
    boot_profile_finish();
    assert(boot_profile_get()->total_ms == 10);
    assert(boot_profile_get()->milliseconds[BOOT_CONFIG] == 10);
    puts("PASS: startup phase timing, repeated/disabled regions, completion and clock wrap");
}
