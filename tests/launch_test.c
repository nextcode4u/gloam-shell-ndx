#include "app.h"
#include "arguments.h"
#include "associations.h"
#include "diagnostics.h"
#include "launch.h"
#include "paths.h"
#include "system_routes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static App app;
static LaunchPlan plan;
static char error[160], path[PATH_CAP];
static void word(unsigned char *data, unsigned value)
{
    for (int i = 0; i < 4; i++)
        data[i] = value >> (8 * i);
}
static void binary(const char *root, const char *code, unsigned destination, bool twl)
{
    unsigned char data[0x1200] = {0};
    memcpy(data + 12, code, 4);
    data[0x12] = twl ? 2 : 0;
    for (int cpu = 0; cpu < 2; cpu++) {
        unsigned char *part = data + 0x20 + cpu * 16;
        unsigned address = cpu ? 0x037F8000 : destination;
        word(part, 0x1000 + cpu * 0x100);
        word(part + 4, address);
        word(part + 8, address);
        word(part + 12, 0x100);
    }
    if (twl) {
        word(data + 0x1C0, 0x1100);
        word(data + 0x1C8, 0x03000000); /* Outside the reserved loader's supported RAM. */
        word(data + 0x1CC, 0x100);
    }
    assert(path_join(path, root, "probe.nds"));
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(data, 1, sizeof data, file) == sizeof data);
    assert(!fclose(file));
}
static void shortcut(const char *root, const char *text)
{
    assert(path_join(path, root, "test.argv"));
    FILE *file = fopen(path, "w");
    assert(file && fputs(text, file) >= 0 && !fclose(file));
}
static void profile(const char *root, int argv_flag, int bootstrap, int hbargv)
{
    assert(path_join(path, root, "_nds/dsi-frontend/plugins/probe.ini"));
    FILE *file = fopen(path, "w");
    assert(file);
    fprintf(file,
            "[plugin]\nversion=1\nextensions=fixture\napplication=/probe.nds\n"
            "argv=%d\nbootstrap=%d\nhbargv=%d\n",
            argv_flag, bootstrap, hbargv);
    assert(!fclose(file));
    associations_load(&app);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    app_init(&app, root, true, true);
    binary(root, "####", 0x02000000, false);
    assert(launch_prepare(&app, path, &plan, error));
    assert(plan.arguments.count == 1);
    shortcut(root, "# A comment\n/probe.nds --flag \"two words\" ';literal'\n");
    assert(launch_prepare(&app, path, &plan, error));
    assert(plan.arguments.count == 4);
    assert(!strcmp(arguments_get(&plan.arguments, 2), "two words"));
    assert(!strcmp(arguments_get(&plan.arguments, 3), ";literal"));
    shortcut(root, "/probe.nds ''");
    assert(!launch_prepare(&app, path, &plan, error));
    shortcut(root, "/probe.nds \"unterminated");
    assert(!launch_prepare(&app, path, &plan, error));
    shortcut(root, "/probe.nds a a a a a a a a a a a a a a a a");
    assert(!launch_prepare(&app, path, &plan, error));
    binary(root, "ABCD", 0x02000000, false);
    assert(!launch_prepare(&app, path, &plan, error));
    binary(root, "####", 0x02FF4000, false);
    assert(!launch_prepare(&app, path, &plan, error));
    binary(root, "####", 0x02000000, true);
    assert(!launch_prepare(&app, path, &plan, error));
    binary(root, "####", 0x02000000, false);
    const char *directories[] = {"_nds", "_nds/dsi-frontend", "_nds/dsi-frontend/plugins"};
    for (unsigned i = 0; i < sizeof directories / sizeof directories[0]; i++) {
        assert(path_join(path, root, directories[i]));
        assert(!mkdir(path, 0777));
    }
    assert(path_join(path, root, "Content with spaces.fixture"));
    FILE *content = fopen(path, "w");
    assert(content && !fclose(content));
    profile(root, 1, 0, 0);
    assert(path_join(path, root, "Content with spaces.fixture"));
    assert(launch_prepare(&app, path, &plan, error));
    assert(plan.arguments.count == 2);
    assert(!strcmp(arguments_get(&plan.arguments, 1), path));
    profile(root, 0, 0, 0);
    assert(path_join(path, root, "Content with spaces.fixture"));
    assert(launch_prepare(&app, path, &plan, error));
    assert(plan.arguments.count == 1);
    profile(root, 1, 1, 0);
    assert(path_join(path, root, "Content with spaces.fixture"));
    assert(!launch_prepare(&app, path, &plan, error));
    profile(root, 1, 0, 1);
    assert(path_join(path, root, "Content with spaces.fixture"));
    assert(!launch_prepare(&app, path, &plan, error));
    unsigned char installer_header[16] = {0};
    memcpy(installer_header, "UNLINS", 6);
    memcpy(installer_header + 12, "UNLI", 4);
    assert(launch_is_homebrew_header(installer_header));
    installer_header[0] = 'X';
    assert(!launch_is_homebrew_header(installer_header));
    assert(!launch_prepare_installer(&app, &plan, error));
    uint32_t request[8];
    memset(request, 0xFF, sizeof request);
    system_menu_request(request);
    assert(request[0] == 0x434E4C54 && request[1] == 0xEE3B1801);
    assert(!request[2] && !request[3] && !request[4] && !request[5] && request[6] == 0x13 &&
           !request[7]);
    app_request_system_menu(&app);
    assert(app.system_menu_requested && !app.launch_requested);
    app.system_menu_requested = false;
    app.dsi_mode = false;
    app_request_system_menu(&app);
    assert(!app.system_menu_requested && app.panel == PANEL_MESSAGE);
    assert(!launch_prepare(&app, path, &plan, error));
    app.launch = plan;
    path_copy(app.launch.content, "sd:/roms/Example.nds\nnot_a_report_field=1", PATH_CAP);
    assert(diagnostics_record_launch(&app, LAUNCH_REPORT_REJECTED, "Missing application"));
    char report[PATH_CAP], temporary[PATH_CAP], report_text[8192] = {0};
    assert(path_preferences_file(report, root, "launch-report.txt"));
    FILE *report_file = fopen(report, "r");
    assert(report_file && fread(report_text, 1, sizeof report_text - 1, report_file));
    fclose(report_file);
    assert(strstr(report_text, "stage=rejected before handoff\n"));
    assert(strstr(report_text, "error=Missing application\n"));
    assert(strstr(report_text, "does not confirm application startup"));
    assert(!strstr(report_text, "\nnot_a_report_field="));
    assert(path_preferences_file(temporary, root, "launch-report.tmp"));
    assert(!mkdir(temporary, 0777));
    assert(!diagnostics_record_launch(&app, LAUNCH_REPORT_HANDOFF_REQUESTED, NULL));
    assert(!rmdir(temporary));
    char unchanged[8192] = {0};
    report_file = fopen(report, "r");
    assert(report_file && fread(unchanged, 1, sizeof unchanged - 1, report_file));
    fclose(report_file);
    assert(!strcmp(unchanged, report_text));
    app.storage_lost = true;
    assert(!diagnostics_record_launch(&app, LAUNCH_REPORT_REJECTED, "SD unavailable"));
    app.storage_lost = false;
    /* Native ScummVM discovery is attached by the full launch planner as well. */
    app.dsi_mode = true;
    const char *scumm_dirs[] = {"roms", "roms/ScummVM", "roms/ScummVM/titles",
                                "_nds/dsi-frontend/plugins/startup"};
    for (unsigned i = 0; i < sizeof scumm_dirs / sizeof scumm_dirs[0]; ++i) {
        assert(path_join(path, root, scumm_dirs[i]));
        assert(!mkdir(path, 0777));
    }
    binary(root, "####", 0x02000000, false);
    char scumm_app[PATH_CAP];
    assert(path_join(scumm_app, root, "roms/ScummVM/scummvm.nds"));
    assert(!rename(path, scumm_app));
    assert(path_join(path, root, "_nds/dsi-frontend/plugins/startup/scummvm.ini"));
    FILE *scumm_config = fopen(path, "w");
    assert(scumm_config && fputs("[scummvm]\nauto_add=1\n", scumm_config) >= 0);
    assert(!fclose(scumm_config));
    assert(launch_prepare(&app, scumm_app, &plan, error));
    assert(plan.arguments.count == 5 && !strcmp(arguments_get(&plan.arguments, 1), "--add"));
    shortcut(root, "/roms/ScummVM/scummvm.nds explicit-game-id");
    assert(launch_prepare(&app, path, &plan, error));
    assert(plan.arguments.count == 2);
    assert(!strcmp(arguments_get(&plan.arguments, 1), "explicit-game-id"));
    puts("Launch parser and preflight checks passed.");
}
