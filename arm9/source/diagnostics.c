/* Export only frontend runtime state. No BIOS, NAND, keys or file contents. */
#include "diagnostics.h"
#include "artwork.h"
#include "image_job.h"
#include "paths.h"
#include <stdio.h>
#ifndef HOST_PREVIEW
#include <blocksds_version.h>
#include <malloc.h>
#endif

static void report_line(FILE *file, const char *key, const char *value)
{
    fprintf(file, "%s=", key);
    for (const unsigned char *p = (const unsigned char *)value; *p; p++)
        fputc(*p < 32 || *p == 127 ? ' ' : *p, file);
    fputc('\n', file);
}

bool diagnostics_record_launch(const App *app, LaunchReportStage stage, const char *error)
{
    static const char *const stages[] = {"rejected before handoff", "backend preparation failed",
                                         "handoff requested", "loader rejected handoff"};
    static const char *const routes[] = {"direct homebrew", "nds-bootstrap retail",
                                         "nds-bootstrap homebrew", "DSiWare"};
    char path[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP];
    if ((unsigned)stage >= sizeof stages / sizeof *stages || !app->storage_ready ||
        app->storage_lost || !path_prepare_preferences_directory(app->root) ||
        !path_preferences_file(path, app->root, "launch-report.txt") ||
        !path_preferences_file(temporary, app->root, "launch-report.tmp") ||
        !path_preferences_file(backup, app->root, "launch-report.bak"))
        return false;
    FILE *file = fopen(temporary, "w");
    if (!file)
        return false;
    fputs("Gloam Shell NDX launch report v1\n"
          "A handoff request does not confirm application startup or compatibility.\n",
          file);
    report_line(file, "stage", stages[stage]);
    report_line(file, "content", app->launch.content);
    report_line(file, "application", app->launch.application);
    report_line(file, "target", app->launch.target);
    report_line(file, "route",
                *app->launch.application &&
                        (unsigned)app->launch.route < sizeof routes / sizeof *routes
                    ? routes[app->launch.route]
                    : "unresolved");
    report_line(file, "error", error ? error : "");
    fprintf(file, "argument_count=%d\ndsi_mode=%d\n", app->launch.arguments.count, app->dsi_mode);
    if (app->launch.route != LAUNCH_DIRECT && *app->launch.target) {
        report_line(file, "save", app->launch.save);
        fprintf(file, "mode=%d\ncpu=%d\nvram=%d\nlanguage=%d\nsave_slot=%d\n",
                app->launch.options.values[GAME_MODE], app->launch.options.values[GAME_CPU],
                app->launch.options.values[GAME_VRAM], app->launch.options.values[GAME_LANGUAGE],
                app->launch.options.values[GAME_SLOT]);
    }
    bool written = !ferror(file);
    if (fclose(file))
        written = false;
    if (!written) {
        remove(temporary);
        return false;
    }
    if (path_commit_file(temporary, path, backup) != FILE_COMMIT_OK)
        return false;
    return true;
}

bool diagnostics_export(const App *app)
{
    char path[PATH_CAP], temporary[PATH_CAP], backup[PATH_CAP];
    if (!app->storage_ready || app->storage_lost ||
        !path_prepare_preferences_directory(app->root) ||
        !path_preferences_file(path, app->root, "diagnostics.txt") ||
        !path_preferences_file(temporary, app->root, "diagnostics.tmp") ||
        !path_preferences_file(backup, app->root, "diagnostics.bak"))
        return false;
    FILE *file = fopen(temporary, "w");
    if (!file)
        return false;
    unsigned icons = 0;
    for (int i = 0; i < ICON_CACHE_CAP; i++)
        icons += app->icons[i].ready;
    fprintf(file,
            "Gloam Shell NDX diagnostics v1\n"
            "This file describes observed frontend state, not backend compatibility.\n"
            "frames=%u\ndsi_mode=%d\nrecovery=%d\nsd_ready=%d\nsd_lost=%d\n"
            "battery_raw=%d\nbrightness=%d\npreferences_unsaved=%d\n"
            "tabs=%d\nactive_tab=%s\nentries=%d\nfolder_batch=%u\ntruncated=%d\n"
            "scan_active=%d\nscan_records=%u\nicons_ready=%u\n"
            "artwork_ready=%d\nartwork_source=%s\ntheme=%s\nfont_glyphs=%u\n"
            "browser_style=%d\nicon_size=%d\nreduced_motion=%d\n"
            "music_mode=%d\nmusic_volume=%d\neffects_volume=%d\n"
            "nand_discovery_attempted=%d\nnand_catalog_ready=%d\nnand_mount_errno=%d\n"
            "last_prepared_route=%d\napplication_argument_count=%d\n"
            "sizeof_app=%lu\n",
            app->frame_count, app->dsi_mode, app->recovery_mode, app->storage_ready,
            app->storage_lost, app->battery_level, app->brightness, app->preferences_dirty,
            app->tab_count, app->tabs[app->category].id, app->entry_count,
            app->locations[app->category].batch, app->truncated, app->scan_directory != NULL,
            app->scan_visited, icons, app->artwork.ready, app->artwork.source, app->theme_id,
            app->themes.active.font.glyph_count, app->browser_style, app->icon_size,
            app->reduced_motion, app->audio.mode, app->audio.music_volume,
            app->audio.effects_volume, app->system_catalog.attempted, app->system_catalog.ready,
            app->system_catalog.mount_error, app->launch.route, app->launch.arguments.count,
            (unsigned long)sizeof *app);
    fprintf(file, "artwork_decodes=%u\nartwork_disk_hits=%u\n", artwork_cache_decodes(),
            artwork_cache_disk_hits());
    fprintf(file, "artwork_decoder_yields=%u\n", image_job_yields());
    for (int i = 0; i < 5; i++)
        fprintf(file, "system_title_%d_available=%d\n", i, app->system_catalog.titles[i].available);
#ifdef HOST_PREVIEW
    fputs("build=host-validation\n", file);
#else
    fprintf(file, "blocksds=%s\n", BLOCKSDS_VERSION_STRING);
    struct mallinfo heap = mallinfo();
    fprintf(file, "heap_in_use=%lu\nheap_reserved=%lu\nheap_free=%lu\n",
            (unsigned long)heap.uordblks, (unsigned long)heap.arena, (unsigned long)heap.fordblks);
#endif
    bool written = !ferror(file);
    if (fclose(file))
        written = false;
    if (!written) {
        remove(temporary);
        return false;
    }
    if (path_commit_file(temporary, path, backup) != FILE_COMMIT_OK)
        return false;
    return true;
}
