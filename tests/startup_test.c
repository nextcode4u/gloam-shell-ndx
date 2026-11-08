#include "app.h"
#include "dashboard.h"
#include "paths.h"
#include "preferences.h"
#include "startup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static App app, restored;
static void put32(unsigned char *p, unsigned value)
{
    for (int i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    unsigned char binary[0x1200] = {0};
    memcpy(binary + 12, "####", 4);
    for (int cpu = 0; cpu < 2; cpu++) {
        unsigned char *part = binary + 0x20 + cpu * 16;
        unsigned address = cpu ? 0x037F8000 : 0x02000000;
        put32(part, 0x1000 + cpu * 0x100);
        put32(part + 4, address);
        put32(part + 8, address);
        put32(part + 12, 0x100);
    }
    char target[PATH_CAP], missing[PATH_CAP];
    assert(path_join(target, root, "Probe.nds"));
    FILE *file = fopen(target, "wb");
    assert(file && fwrite(binary, 1, sizeof binary, file) == sizeof binary && !fclose(file));
    app_init(&app, root, true, true);
    assert(!app.auto_launch_delay && !app.startup_countdown);
    assert(dashboard_record_request(&app, target));
    app.auto_launch_delay = 3;
    app.dashboard.config.enabled[DASH_RECENT] = false;
    startup_prepare(&app);
    assert(!app.startup_countdown);
    app.dashboard.config.enabled[DASH_RECENT] = true;
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    assert(restored.startup_countdown == 180 && restored.auto_launch_delay == 3);
    for (int i = 0; i < 179; i++)
        startup_tick(&restored);
    assert(!restored.launch_requested && restored.startup_countdown == 1);
    startup_tick(&restored);
    assert(restored.launch_requested && restored.startup_due && restored.startup_cancelled);
    startup_cancel(&restored); /* Cancellation also wins on the final pending frame. */
    assert(!restored.launch_requested && !restored.startup_due);
    for (int i = 0; i < 200; i++)
        startup_tick(&restored);
    assert(!restored.launch_requested);
    app_init_mode(&restored, root, true, true, true);
    assert(restored.recovery_mode && !restored.startup_countdown &&
           restored.auto_launch_delay == 3);
    assert(path_join(missing, root, "Missing.nds"));
    assert(dashboard_record_request(&app, missing));
    assert(app_save(&app));
    app_init(&restored, root, true, true);
    for (int i = 0; i < 180; i++)
        startup_tick(&restored);
    assert(!restored.launch_requested && restored.panel == PANEL_MESSAGE && *restored.message);
    assert(!restored.startup_countdown && restored.startup_cancelled);
    startup_prepare(&restored);
    assert(!restored.startup_countdown); /* Error prompts require attention first. */
    puts("PASS: optional countdown, persistence, final-frame input cancellation, early recovery "
         "bypass and failed auto-launch without a restart loop");
}
