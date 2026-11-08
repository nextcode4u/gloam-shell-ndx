/* Capability-limited direct homebrew preflight. Retail loading is a separate route. */
#include "launch.h"
#include "arguments.h"
#include "associations.h"
#include "bootstrap.h"
#include "icons.h"
#include "item_preferences.h"
#include "paths.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

bool launch_is_homebrew_header(const unsigned char header[16])
{
    return !memcmp(header + 12, "####", 4) || !memcmp(header + 12, "GYOB", 4) ||
           !memcmp(header + 12, "TGDS", 4) || !memcmp(header + 12, "KBSE", 4) ||
           (!memcmp(header + 12, "UNLI", 4) && !memcmp(header, "UNLINS", 6));
}
static uint32_t le32(const unsigned char *data)
{
    return data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}
static bool validate_binary(const char *path, bool identified_homebrew, char error[160])
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        snprintf(error, 160, "Application is missing or unreadable. Check its configured path.");
        return false;
    }
    unsigned char header[0x200];
    bool valid =
        fread(header, 1, sizeof header, file) == sizeof header && !fseek(file, 0, SEEK_END);
    long size = valid ? ftell(file) : -1;
    fclose(file);
    if (!valid || size < 0) {
        snprintf(error, 160, "Application has a truncated NDS header.");
        return false;
    }
    if (!identified_homebrew && !launch_is_homebrew_header(header) &&
        !launch_is_homebrew_file(path)) {
        snprintf(error, 160,
                 "Unsupported direct-launch header. Open retail titles directly to use "
                 "nds-bootstrap; check the build for homebrew applications.");
        return false;
    }
    for (int cpu = 0; cpu < 2; cpu++) {
        const unsigned char *part = header + 0x20 + cpu * 0x10;
        uint32_t offset = le32(part), entry = le32(part + 4), destination = le32(part + 8),
                 length = le32(part + 12);
        bool ram = destination >= 0x02000000 && destination < 0x02FF4000 &&
                   length <= 0x02FF4000 - destination;
        bool iwram = cpu == 1 && destination >= 0x037F8000 && destination < 0x03810000 &&
                     length <= 0x03810000 - destination;
        if (!length || offset < 0x200 || offset > (uint32_t)size ||
            length > (uint32_t)size - offset || !(ram || iwram) || (entry & ~1u) < destination ||
            (entry & ~1u) >= destination + length) {
            snprintf(error, 160,
                     "Application binary ranges are invalid or unsupported by the homebrew route.");
            return false;
        }
    }
    /* The legacy adapter places argv after ARM9/ARM9i. Reserve space for
     * both the command data and the receiving runtime's pointer table. */
    uint32_t argument_address = (le32(header + 0x28) + le32(header + 0x2C) + 3) & ~3u;
    if (le32(header + 0x2C) > 0x380000) {
        snprintf(error, 160, "Large ARM9 binaries require a different launch adapter.");
        return false;
    }
    if (header[0x12] & 2) {
        if (size < 0x1000) {
            snprintf(error, 160, "Application has a truncated DSi header.");
            return false;
        }
        for (int cpu = 0; cpu < 2; cpu++) {
            const unsigned char *part = header + 0x1C0 + cpu * 0x10;
            uint32_t offset = le32(part), destination = le32(part + 8), length = le32(part + 12);
            if (!length)
                continue;
            bool ram = destination >= 0x02000000 && destination < 0x02FF4000 &&
                       length <= 0x02FF4000 - destination;
            bool iwram = cpu == 1 && destination >= 0x037F8000 && destination < 0x03810000 &&
                         length <= 0x03810000 - destination;
            if (cpu == 0 && destination + length > argument_address)
                argument_address = (destination + length + 3) & ~3u;
            if (offset < 0x1000 || offset > (uint32_t)size || length > (uint32_t)size - offset ||
                !(ram || iwram)) {
                snprintf(error, 160, "Application has unsupported DSi binary ranges.");
                return false;
            }
        }
    }
    const uint32_t reserve = LAUNCH_DATA_CAP + (LAUNCH_ARG_CAP + 1) * 4 + 4;
    if (argument_address >= 0x02FF4000 || reserve > 0x02FF4000 - argument_address) {
        snprintf(error, 160, "Application leaves insufficient space for argv and the return stub.");
        return false;
    }
    for (int cpu7 = 0; cpu7 < ((header[0x12] & 2) ? 2 : 1); cpu7++) {
        const unsigned char *part = header + (cpu7 ? 0x1D0 : 0x30);
        uint32_t destination = le32(part + 8), length = le32(part + 12);
        if (length && destination < argument_address + reserve &&
            destination + length > argument_address) {
            snprintf(error, 160, "Application's ARM7 memory overlaps the argv reservation.");
            return false;
        }
    }
    return true;
}

bool launch_prepare(const App *app, const char *path, LaunchPlan *plan, char error[160])
{
    memset(plan, 0, sizeof *plan);
    path_copy(plan->content, path, PATH_CAP);
    if (!app->storage_ready || !app->dsi_mode) {
        snprintf(error, 160, "Direct homebrew currently requires DSi mode and its SD card.");
        return false;
    }
    ArgumentList shortcut = {0};
    char target[PATH_CAP];
    ItemPreferences item;
    bool custom = item_preferences_load(app->root, path, &item) && *item.application;
    if (!custom && !strcasecmp(path_extension(path), "argv")) {
        if (!arguments_read(path, &shortcut, error))
            return false;
        if (!path_resolve_sd(target, app->root, arguments_get(&shortcut, 0))) {
            snprintf(error, 160, "Shortcut application path is invalid.");
            return false;
        }
        path = target;
    }
    const Association *profile = NULL;
    bool identified_homebrew = false;
    if (custom) {
        struct stat content;
        if (stat(path, &content) || !S_ISREG(content.st_mode)) {
            snprintf(error, 160, "Selected content is missing or unreadable.");
            return false;
        }
        path_copy(plan->application, item.application, PATH_CAP);
    } else if (icon_is_ds_title(path))
        path_copy(plan->application, path, PATH_CAP);
    else {
        struct stat st;
        if (stat(path, &st) || !S_ISREG(st.st_mode)) {
            snprintf(error, 160, "Selected content is missing or unreadable.");
            return false;
        }
        profile = association_find(app, path);
        if (!profile) {
            snprintf(error, 160, "No application is configured for this file type.");
            return false;
        }
        if (profile->bootstrap || profile->hbargv) {
            if (!bootstrap_prepare_homebrew(app, profile->application,
                                            profile->use_argv || profile->hbargv ? path : "", plan,
                                            error))
                return false;
        } else
            path_copy(plan->application, profile->application, PATH_CAP);
    }
    if (!custom && !profile && icon_is_ds_title(plan->application)) {
        unsigned char header[16];
        FILE *file = fopen(plan->application, "rb");
        bool read = file && fread(header, 1, sizeof header, file) == sizeof header;
        if (file)
            fclose(file);
        bool homebrew = read && (launch_is_homebrew_header(header) ||
                                 launch_is_homebrew_file(plan->application));
        identified_homebrew = homebrew;
        if (read && !homebrew) {
            path_copy(target, plan->application, PATH_CAP);
            if (shortcut.count > 1) {
                snprintf(error, 160,
                         "Retail shortcuts cannot pass arbitrary argv through nds-bootstrap.");
                return false;
            }
            if (!bootstrap_prepare(app, target, plan, error))
                return false;
        }
    }
    if (plan->route == LAUNCH_BOOTSTRAP_HB && shortcut.count > 1) {
        snprintf(error, 160,
                 "The HB backend supports one profile content argument; use the direct route for "
                 "multi-argument shortcuts.");
        return false;
    }
    if (!validate_binary(plan->application, identified_homebrew, error))
        return false;
    if (!arguments_add(&plan->arguments, plan->application))
        return false;
    if (plan->route == LAUNCH_DIRECT && (custom || (profile && profile->use_argv)) &&
        !arguments_add(&plan->arguments, path))
        goto too_many;
    for (int index = 1; index < shortcut.count; index++) {
        const char *argument = arguments_get(&shortcut, index);
        if (!*argument) {
            snprintf(error, 160, "Empty argv arguments are not supported by this launch route.");
            return false;
        }
        if (!arguments_add(&plan->arguments, argument))
            goto too_many;
    }
    return true;
too_many:
    snprintf(error, 160, "Launch exceeds 16 arguments or 4 KiB of argument data.");
    return false;
}

bool launch_prepare_slot1(const App *app, LaunchPlan *plan, char error[160])
{
    memset(plan, 0, sizeof *plan);
    path_copy(plan->content, "system:slot1", sizeof plan->content);
    if (!app->storage_ready || !app->dsi_mode) {
        snprintf(error, 160, "Slot-1 launch requires DSi mode and an accessible SD card.");
        return false;
    }
    static const char *const locations[] = {
        "_nds/dsi-frontend/loaders/slot1launch.nds", "_nds/slot1launch.nds",
        "_nds/akmenunext/slot1launch.nds",           "_nds/TWiLightMenu/slot1launch.srldr",
        "_nds/TWiLightMenu/slot1launch.nds",
    };
    for (unsigned i = 0; i < sizeof locations / sizeof *locations; ++i) {
        char path[PATH_CAP];
        struct stat status;
        if (!path_join(path, app->root, locations[i]) || stat(path, &status) ||
            !S_ISREG(status.st_mode))
            continue;
        /* A named helper is still subject to normal homebrew/binary checks.
         * File associations and per-item overrides must not redirect this action. */
        if (!launch_is_homebrew_file(path)) {
            snprintf(
                error, 160,
                "Slot-1 helper is not recognized as homebrew. Replace it with slot1launch.nds.");
            return false;
        }
        if (!validate_binary(path, true, error))
            return false;
        path_copy(plan->application, path, sizeof plan->application);
        if (!arguments_add(&plan->arguments, plan->application)) {
            snprintf(error, 160, "The Slot-1 helper path exceeds the argument limit.");
            return false;
        }
        plan->route = LAUNCH_DIRECT;
        return true;
    }
    snprintf(error, 160,
             "Install slot1launch.nds in /_nds/dsi-frontend/loaders/. Existing AKMenu or "
             "TWiLightMenu helpers are also supported.");
    return false;
}

bool launch_prepare_installer(const App *app, LaunchPlan *plan, char error[160])
{
    static const char *locations[] = {"_nds/dsi-frontend/system/unlaunch-installer.nds",
                                      "apps/Homebrew Apps/unlaunch.nds", "unlaunch.nds"};
    for (unsigned i = 0; i < sizeof locations / sizeof locations[0]; i++) {
        char path[PATH_CAP];
        struct stat status;
        if (path_join(path, app->root, locations[i]) && !stat(path, &status) &&
            S_ISREG(status.st_mode)) {
            if (!launch_prepare(app, path, plan, error))
                return false;
            if (plan->route != LAUNCH_DIRECT) {
                snprintf(error, 160,
                         "The supplied installer must support direct DSi homebrew launch.");
                return false;
            }
            return true;
        }
    }
    snprintf(error, 160,
             "Place your installer at /apps/Homebrew Apps/unlaunch.nds. Boot and H&S choices are "
             "managed in the installer.");
    return false;
}
