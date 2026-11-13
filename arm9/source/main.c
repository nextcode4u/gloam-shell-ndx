#ifndef HOST_PREVIEW
#include "app.h"
#include "artwork.h"
#include "association_editor.h"
#include "audio.h"
#include "bootstrap.h"
#include "browser.h"
#include "cheats.h"
#include "dashboard.h"
#include "diagnostics.h"
#include "game_options.h"
#include "grid.h"
#include "icons.h"
#include "image_job.h"
#include "installed_titles.h"
#include "launch.h"
#include "layout.h"
#include "navigation.h"
#include "paths.h"
#include "picker.h"
#include "preferences.h"
#include "renderer.h"
#include "settings.h"
#include "slot1.h"
#include "startup.h"
#include "storage_state.h"
#include "system_routes.h"
#include "themes.h"
#include "update_transport.h"
#include <dswifi9.h>

#include <blocksds_version.h>
#include <errno.h>
#include <fat.h>

#if !BLOCKSDS_VERSION_AT_LEAST(1, 25, 0)
#error "Build this frontend with BlocksDS 1.25.0 or newer."
#endif
#include "hardware_protocol.h"
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <nds/arm9/sdmmc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static App app;
static uint16_t top[256 * 192] __attribute__((aligned(32)));
static uint16_t bottom[256 * 192] __attribute__((aligned(32)));
static int music_channel = -1, effect_channel = -1;
static int previous_music_volume = -1, previous_effect_volume = -1;

/* No network allocation or radio initialization occurs on the boot path. */
static bool update_cancel;
static unsigned update_started;
static time_t update_started_time;
static bool update_network_initialized, update_network_attempted;
static cothread_t update_thread = -1;
static bool update_wait(void)
{
    do {
        cothread_yield();
        if (update_cancel || !app.updates.enabled || app.storage_lost ||
            app.frame_count - update_started >= 45 * 60 || time(NULL) - update_started_time >= 45)
            return false;
    } while (app.selection_idle_frames < 30);
    return true;
}

static int update_worker(void *unused)
{
    (void)unused;
    char tag[UPDATE_TAG_CAP] = "", ca[PATH_CAP];
    if (!update_network_attempted) {
        update_network_attempted = true;
        update_network_initialized = Wifi_InitDefault(INIT_ONLY | WIFI_ATTEMPT_DSI_MODE);
    }
    bool initialized = update_network_initialized;
    if (initialized && update_wait()) {
        Wifi_AutoConnect();
        while (update_wait()) {
            int status = Wifi_AssocStatus();
            if (status == ASSOCSTATUS_CANNOTCONNECT)
                break;
            if (status != ASSOCSTATUS_ASSOCIATED)
                continue;
            app.updates.connected = true;
            struct in_addr dns;
            Wifi_GetIPInfo(NULL, NULL, &dns, NULL);
            if (path_join(ca, app.root, "_nds/dsi-frontend/update-ca.pem"))
                if (!update_fetch(ca, dns.s_addr, update_wait, tag))
                    tag[0] = 0;
            break;
        }
    }
    if (initialized) {
        Wifi_DisconnectAP();
        Wifi_DisableWifi();
        swiWaitForVBlank();
        swiWaitForVBlank();
        /* SDK 1.25.0 wifi_lwip_deinit deliberately crashes. Keep the initialized
         * stack for reuse; only the radio and this request are shut down. */
    }
    if (!update_cancel && app.storage_ready && !app.storage_lost)
        update_complete(&app.updates, app.root, *tag ? tag : NULL, time(NULL));
    else
        app.updates.busy = app.updates.connected = false;
    /* Detached workers are reclaimed by the SDK scheduler. Deleting another
     * thread here can invalidate its saved next-thread pointer in SDK 1.25. */
    update_thread = -1;
    return 0;
}
static void stop_update_checker(void)
{
    update_cancel = true;
    while (update_thread >= 0)
        cothread_yield();
}
static void update_checker_tick(void)
{
    if (update_thread >= 0)
        return;
    if (app.recovery_mode || !app.dsi_mode || !app.storage_ready || app.storage_lost ||
        !app.updates.enabled || app.frame_count < 1800)
        return;
    if (!app.updates.cache_loaded)
        update_cache_load(&app.updates, app.root);
    if (!update_should_start(&app.updates, app.frame_count, app.selection_idle_frames,
                             !app.launch_requested && !app.startup_due, time(NULL)))
        return;
    update_cancel = false;
    update_started = app.frame_count;
    update_started_time = time(NULL);
    update_thread = cothread_create(update_worker, NULL, 24 * 1024, COTHREAD_DETACHED);
    if (update_thread < 0)
        update_complete(&app.updates, app.root, NULL, time(NULL));
}

static unsigned artwork_clock(void)
{
    return REG_VCOUNT;
}

static void stop_launcher_audio(void)
{
    bool active = music_channel >= 0 || effect_channel >= 0;
    if (music_channel >= 0)
        soundKill(music_channel);
    if (effect_channel >= 0)
        soundKill(effect_channel);
    music_channel = effect_channel = -1;
    /* Sound commands are handled asynchronously by ARM7. Retire sample storage
     * only after its FIFO handler has had time to process the stop commands. */
    if (active) {
        swiWaitForVBlank();
        swiWaitForVBlank();
    }
}
static void update_launcher_audio(void)
{
    if (app.audio.reload_requested) {
        stop_launcher_audio();
        audio_reload(&app);
        if (app.audio.music.samples) {
            DC_FlushRange(app.audio.music.samples, app.audio.music.bytes);
            music_channel = soundPlaySampleChannel(0, app.audio.music.samples, SoundFormat_16Bit,
                                                   app.audio.music.bytes, app.audio.music.rate,
                                                   app.audio.music_volume * 127 / 100, 64, true, 0);
        }
        for (int i = 0; i < 3; i++)
            if (app.audio.effects[i].samples)
                DC_FlushRange(app.audio.effects[i].samples, app.audio.effects[i].bytes);
        previous_music_volume = previous_effect_volume = -1;
    }
    if (music_channel >= 0 && previous_music_volume != app.audio.music_volume)
        soundSetVolume(music_channel, app.audio.music_volume * 127 / 100);
    if (effect_channel >= 0 && previous_effect_volume != app.audio.effects_volume)
        soundSetVolume(effect_channel, app.audio.effects_volume * 127 / 100);
    previous_music_volume = app.audio.music_volume;
    previous_effect_volume = app.audio.effects_volume;
    if (app.audio.event > AUDIO_NONE && app.audio.event < AUDIO_EVENT_COUNT) {
        AudioClip *clip = &app.audio.effects[app.audio.event - 1];
        if (clip->samples && app.audio.effects_enabled && !app.recovery_mode)
            effect_channel =
                soundPlaySampleChannel(1, clip->samples, SoundFormat_16Bit, clip->bytes, clip->rate,
                                       app.audio.effects_volume * 127 / 100, 64, false, 0);
    }
    app.audio.event = AUDIO_NONE;
}

static void update_console_status(void)
{
    if (app.frame_count % 300 == 0) {
        u32 value = getBatteryLevel();
        app.battery_level = value & BATTERY_LEVEL_MASK;
        app.battery_charging = (value & BATTERY_CHARGER_CONNECTED) != 0;
        fifoSendValue32(FRONTEND_HW_FIFO, FRONTEND_HW_READ_BRIGHTNESS);
    }
    while (fifoCheckValue32(FRONTEND_HW_FIFO)) {
        u32 reply = fifoGetValue32(FRONTEND_HW_FIFO);
        if ((reply & 0xFFFF0000u) == FRONTEND_HW_BRIGHTNESS_REPLY) {
            unsigned level = reply & 0xFFFF;
            app.brightness = level >= 1 && level <= 5 ? (int)level : -1;
        }
    }
    if (app.requested_brightness >= 1) {
        app.brightness = systemSetBacklightLevel(app.requested_brightness);
        app.requested_brightness = -1;
    }
}

/* Card metadata uses its own bounded reader. It neither mounts NitroFS nor
 * changes cartridge save memory. The launch helper still owns game boot. */
static Slot1Probe slot1_probe;

static bool slot1_present(void)
{
    return (REG_SCFG_EXT & SCFG_EXT_SCFG_MBK_REG) && !(REG_SCFG_MC & SCFG_MC_EJECTED);
}
static void slot1_power(bool enabled)
{
    if (enabled)
        enableSlot1();
    else
        disableSlot1();
}
__attribute__((target("arm"))) static bool slot1_key_seed(uint32_t words[0x412])
{
    uint32_t setting, control;
    __asm__ volatile("mrc p15, 0, %0, c9, c1, 1" : "=r"(setting));
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(control));
    /* The DSi boot ROM relocates the NTR seed here before locking its upper
     * ROM. Read only when the SDK's ITCM mapping covers this address. Some
     * boot chains clear it; those retain the ordinary cartridge fallback. */
    if (!(control & (1u << 18)) || ((setting >> 1) & 31) < 16)
        return false;
    /* Access the physical ITCM offset through the SDK's mapped 32 KiB
     * alias. The boot-ROM alias at 0x01FFC894 is outside its MPU window. */
    const volatile uint32_t *seed = (const volatile uint32_t *)0x01004894;
    if (seed[0] != 0x5F20D599 || seed[1] != 0xB9F54457)
        return false;
    for (unsigned i = 0; i < 0x412; i++)
        words[i] = seed[i];
    return slot1_crc16(words, 0x1048) == 0xC6F4;
}
static void slot1_seed(uint32_t low, uint16_t high)
{
    sysSetCardOwner(BUS_OWNER_ARM9);
    REG_ROMCTRL = 0;
    REG_CARD_1B0 = low;
    REG_CARD_1B4 = 0x879B9B05;
    REG_CARD_1B8 = high;
    REG_CARD_1BA = 0x5C;
    REG_ROMCTRL = CARD_nRESET | CARD_SEC_SEED | CARD_SEC_EN | CARD_SEC_DAT;
}
static bool slot1_transfer(const uint8_t command[8], uint32_t flags, void *output, size_t bytes)
{
    unsigned block = (flags >> 24) & 7;
    unsigned expected = block == 7 ? 4 : block ? 0x100u << block : 0;
    if ((bytes & 3) || bytes > expected || (bytes && !output))
        return false;
    sysSetCardOwner(BUS_OWNER_ARM9);
    REG_ROMCTRL = 0;
    REG_AUXSPICNTH = CARD_SPICNTH_ENABLE;
    for (unsigned i = 0; i < 8; i++)
        REG_CARD_COMMAND[i] = command[i];
    unsigned previous = REG_VCOUNT, elapsed = 0, received = 0, polls = 0;
    REG_ROMCTRL = flags;
    bool valid = true;
    do {
        if (REG_ROMCTRL & CARD_DATA_READY) {
            uint32_t word = REG_CARD_DATA_RD;
            if (received < bytes)
                memcpy((uint8_t *)output + received, &word, sizeof word);
            received += sizeof word;
        }
        unsigned line = REG_VCOUNT;
        elapsed += line >= previous ? line - previous : line + 263 - previous;
        previous = line;
        if (!slot1_present() || elapsed >= 200 || ++polls >= 1000000) {
            valid = false;
            REG_ROMCTRL = 0;
            break;
        }
    } while (REG_ROMCTRL & CARD_BUSY);
    return valid && received == expected;
}
static const Slot1Operations slot1_operations = {slot1_present, slot1_power, slot1_transfer,
                                                 slot1_seed, slot1_key_seed};

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    defaultExceptionHandler();
    /* Cooperative UI/network workers both wait for this IRQ. BIOS-only waits
     * did not require an explicit enabled VBlank source. */
    irqEnable(IRQ_VBLANK);
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    int mainbg = bgInit(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int subbg = bgInitSub(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    lcdMainOnTop();
    /* This frontend owns the DSi SD root. Some cold-boot loaders supply a
     * flashcard DLDI stub or a misleading incoming device path; probing it
     * is unnecessary here and can jump through invalid driver callbacks.
     * In DS mode, fail storage initialization cleanly instead of probing it. */
    DLDI_INTERFACE *driver = (DLDI_INTERFACE *)(uintptr_t)io_dldi_data;
    if (driver)
        driver->ioInterface.features = 0;
    __system_argv->argvMagic = 0;
    bool storage = fatInitDefault();
    char root[PATH_CAP] = "sd:/";
    if (storage) {
        char cwd[PATH_CAP];
        if (getcwd(cwd, sizeof cwd)) {
            char *colon = strchr(cwd, ':');
            if (colon) {
                size_t n = (size_t)(colon - cwd) + 1;
                if (n + 2 < sizeof root) {
                    memcpy(root, cwd, n);
                    root[n] = '/';
                    root[n + 1] = 0;
                }
            }
        }
    }
    scanKeys();
    app_init_mode(&app, root, storage, isDSiMode(), (keysHeld() & KEY_SELECT) != 0);
    image_job_set_clock(artwork_clock, 263, 32); /* About 2 ms of decode work. */
    artwork_cache_enable_async(true);
    int applied_scroll_speed = -1;
    bool library_jump_used = false, details_pending = false;
    while (1) {
        /* Keep settings adjustment predictable even when browser scrolling is Fast. */
        int speed =
            app.panel == PANEL_NONE && !app.settings.open ? app.scroll_speed : SCROLL_NORMAL;
        if (speed != applied_scroll_speed) {
            ScrollTiming timing = navigation_scroll_timing(speed);
            keysSetRepeat(timing.delay_frames, timing.interval_frames);
            applied_scroll_speed = speed;
        }
        scanKeys();
        unsigned down = keysDown(), repeat = keysDownRepeat();
        if (down & KEY_Y) {
            library_jump_used = false;
            details_pending = false;
        }
        app.library_jump_held = (keysHeld() & KEY_Y) != 0;
        if ((app.startup_countdown || app.startup_due) && (down || repeat)) {
            startup_cancel(&app);
            down = repeat = 0; /* The cancelling press does not also launch an item. */
        }
        if (app.launch_requested) {
            /* Keep the prepared selection stable while the opening press is
             * released. B cancels without also navigating out of the folder. */
            if (down & KEY_B)
                app.launch_requested = app.startup_due = false;
            down = repeat = 0;
        }
        if (app.dsi_mode && !strcmp(app.root, "sd:/") && app.frame_count % 30 == 0)
            storage_observe(&app, (sdmmc_GetDiskStatus() & SDMMC_STATUS_NODISK) == 0);
        if (app.storage_lost) {
            if (down & KEY_START)
                app.system_menu_requested = true;
            down = repeat = 0;
        }
        if (repeat & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_L | KEY_R))
            audio_event(&app, AUDIO_CURSOR);
        if (down & KEY_A)
            audio_event(&app, AUDIO_CONFIRM);
        if (down & KEY_B)
            audio_event(&app, AUDIO_BACK);
        if (down & KEY_START) {
            if (app.cheats)
                cheats_close(&app);
            else if (app.game_editor.open)
                game_editor_close(&app);
            else if (app.themes.open)
                themes_cancel(&app);
            else if (app.picker.open)
                picker_close(&app);
            else if (app.dashboard.open)
                app.dashboard.open = false;
            else if (app.association_editor.open)
                app.association_editor.open = false;
            else if (app.settings.open)
                settings_close(&app);
            else
                settings_open(&app);
        }
        if (down & KEY_B) {
            app.system_title_requested = -1;
            app.system_menu_requested = app.slot1_requested = false;
            app_back(&app);
        } else if (app.rail_focus) {
            if (repeat & (KEY_UP | KEY_L))
                app_rail_move(&app, -1);
            if (repeat & (KEY_DOWN | KEY_R))
                app_rail_move(&app, 1);
            if (down & KEY_A)
                app_rail_commit(&app);
            if (down & KEY_X)
                app_rail_toggle(&app);
        } else if (app.cheats) {
            if (repeat & KEY_UP)
                cheats_move(&app, -1);
            if (repeat & KEY_DOWN)
                cheats_move(&app, 1);
            if (down & KEY_A)
                cheats_activate(&app);
            if (down & KEY_Y)
                for (int i = 0; i < app.cheats->count; i++)
                    app.cheats->entries[i].selected = false;
        } else if (app.game_editor.open) {
            if (repeat & KEY_UP)
                game_editor_move(&app, -1);
            if (repeat & KEY_DOWN)
                game_editor_move(&app, 1);
            if (repeat & KEY_LEFT)
                game_editor_adjust(&app, -1);
            if (repeat & KEY_RIGHT || down & KEY_A)
                game_editor_adjust(&app, 1);
            if (down & KEY_Y)
                game_options_defaults(&app.game_editor.options, !app.game_editor.defaults);
        } else if (app.themes.open) {
            if (repeat & KEY_UP)
                themes_move(&app, -1);
            if (repeat & KEY_DOWN)
                themes_move(&app, 1);
            if (down & KEY_A)
                themes_apply(&app);
        } else if (app.picker.open) {
            if (down & KEY_L)
                picker_batch(&app, -1);
            if (down & KEY_R)
                picker_batch(&app, 1);
            if (repeat & KEY_UP)
                picker_move(&app, -1);
            if (repeat & KEY_DOWN)
                picker_move(&app, 1);
            if (repeat & KEY_LEFT)
                picker_move(&app, -PICKER_VISIBLE_ROWS);
            if (repeat & KEY_RIGHT)
                picker_move(&app, PICKER_VISIBLE_ROWS);
            if (down & KEY_A)
                picker_activate(&app);
            if (down & KEY_Y)
                picker_choose_folder(&app);
        } else if (app.dashboard.open) {
            if (repeat & KEY_UP)
                dashboard_move(&app, -1);
            if (repeat & KEY_DOWN)
                dashboard_move(&app, 1);
            if (repeat & KEY_LEFT)
                dashboard_edit(&app, -1);
            if (repeat & KEY_RIGHT)
                dashboard_edit(&app, 1);
            if (down & KEY_L)
                dashboard_limit(&app, -1);
            if (down & KEY_R)
                dashboard_limit(&app, 1);
            if (down & KEY_A)
                dashboard_toggle(&app);
            if (down & KEY_Y)
                dashboard_clear_recent(&app);
        } else if (app.association_editor.open) {
            if (repeat & KEY_UP)
                association_editor_move(&app, -1);
            if (repeat & KEY_DOWN)
                association_editor_move(&app, 1);
            if (down & KEY_A)
                association_editor_activate(&app);
        } else if (app.settings.open) {
            if (repeat & KEY_UP)
                settings_move(&app, -1);
            if (repeat & KEY_DOWN)
                settings_move(&app, 1);
            if (repeat & KEY_LEFT)
                settings_adjust(&app, -1);
            if (repeat & KEY_RIGHT)
                settings_adjust(&app, 1);
            if (down & KEY_L)
                settings_section(&app, -1);
            if (down & KEY_R)
                settings_section(&app, 1);
            if (down & KEY_A)
                settings_activate(&app);
        } else if (app.panel != PANEL_NONE) {
            if (repeat & KEY_UP)
                app_panel_move(&app, -1);
            if (repeat & KEY_DOWN)
                app_panel_move(&app, 1);
            if (repeat & KEY_LEFT)
                app_panel_adjust(&app, -1);
            if (repeat & KEY_RIGHT)
                app_panel_adjust(&app, 1);
            if (down & KEY_A)
                app_panel_activate(&app);
        } else {
            if (down & KEY_Y)
                details_pending = true;
            if (repeat & KEY_L)
                app_category(&app, app.category - 1);
            if (repeat & KEY_R)
                app_category(&app, app.category + 1);
            if (repeat & KEY_LEFT)
                browser_direction(&app, -1, 0);
            if (repeat & KEY_RIGHT)
                browser_direction(&app, 1, 0);
            if (repeat & (KEY_UP | KEY_DOWN)) {
                int direction = repeat & KEY_UP ? -1 : 1;
                if (app.library_jump_held) {
                    browser_jump_initial(&app, direction);
                    library_jump_used = true;
                } else
                    browser_direction(&app, 0, direction);
            }
            if (down & KEY_A)
                app_open(&app);
            if (down & KEY_SELECT)
                app_panel(&app, PANEL_ACTIONS);
            if (down & KEY_X)
                app_rail_toggle(&app);
            if (keysUp() & KEY_Y) {
                if (details_pending && !library_jump_used && !app.launch_requested)
                    app_toggle_details(&app);
                details_pending = false;
            }
        }
        if (!app.storage_lost && (down & KEY_TOUCH)) {
            touchPosition t;
            touchRead(&t);
            audio_event(&app, AUDIO_CURSOR);
            if (app.cheats)
                cheats_touch(&app, t.px, t.py);
            else if (app.game_editor.open)
                game_editor_touch(&app, t.px, t.py);
            else if (app.themes.open)
                themes_touch(&app, t.px, t.py);
            else if (app.picker.open)
                picker_touch(&app, t.px, t.py);
            else if (app.dashboard.open)
                dashboard_touch(&app, t.px, t.py);
            else if (app.association_editor.open)
                association_editor_touch(&app, t.px, t.py);
            else if (app.settings.open)
                settings_touch(&app, t.px, t.py);
            else if (app.panel == PANEL_QUICK || app.panel == PANEL_ACTIONS) {
                int rows_top = layout_panel_top(&app) + 29;
                int row = ((int)t.py - rows_top) / 15;
                int index = app.panel_scroll + row;
                int count = app.panel == PANEL_QUICK ? QUICK_CONTROL_COUNT : ACTION_COUNT;
                if (t.px >= 4 && t.px < 252 && t.py >= rows_top && row < UI_PANEL_VISIBLE_ROWS &&
                    index < count) {
                    app.panel_selection = index;
                    app_panel_activate(&app);
                }
            } else if (app.panel != PANEL_NONE)
                app_back(&app);
            else if (t.py < 20 && t.px > 180)
                settings_open(&app);
            else if (t.py >= layout_list_top(app.category) && t.py < layout_list_bottom(&app)) {
                Location *location = &app.locations[app.category];
                int i = layout_is_grid(&app)
                            ? grid_touch_index(&app, t.px, t.py)
                            : location->scroll +
                                  (t.py - layout_list_top(app.category)) / layout_row_height(&app);
                if (i >= 0 && i < app.entry_count) {
                    if (i == location->selected)
                        app_open(&app);
                    else
                        app_move(&app, i - location->selected);
                }
            }
        }
        if (app.configuration_reload_requested) {
            stop_update_checker();
            stop_launcher_audio();
            for (int i = 0; i < 3; i++)
                audio_free_clip(&app.audio.effects[i]);
            audio_free_clip(&app.audio.music);
            browser_scan_cancel(&app);
            installed_catalog_cancel(&app.installed_catalog);
            char root[PATH_CAP], result[160];
            path_copy(result, app.message, sizeof result);
            path_copy(root, app.root, sizeof root);
            bool dsi = app.dsi_mode;
            app_init(&app, root, true, dsi);
            startup_cancel(&app);
            path_copy(app.message, result, sizeof app.message);
            app_panel(&app, PANEL_MESSAGE);
        }
        bool boot_keys_released = !(keysHeld() & (KEY_A | KEY_B | KEY_X | KEY_Y));
        if (app.slot1_requested) {
            app.slot1_requested = false;
            if (!app.dsi_mode || (REG_SCFG_MC & SCFG_MC_EJECTED)) {
                snprintf(app.message, sizeof app.message,
                         "Insert a Slot-1 card before launching it.");
                app_panel(&app, PANEL_MESSAGE);
            } else if (launch_prepare_slot1(&app, &app.launch, app.message)) {
                /* Use the same release, save, diagnostic and direct-loader path
                 * as launching the helper from Apps. Never issue a cart: reboot. */
                app.launch_requested = true;
            } else {
                diagnostics_record_launch(&app, LAUNCH_REPORT_REJECTED, app.message);
                app_panel(&app, PANEL_MESSAGE);
            }
        }
        if (app.system_discovery_requested) {
            app.system_discovery_requested = false;
            errno = 0;
            bool nand_ready = app.dsi_mode && nandInit(true);
            int mount_error = errno;
            if (nand_ready)
                system_catalog_scan(&app.system_catalog, "nand:/");
            else {
                app.system_catalog.attempted = true;
                app.system_catalog.mount_error = mount_error;
                snprintf(app.system_catalog.status, sizeof app.system_catalog.status,
                         "Read-only NAND mount unavailable (error %d). Native system apps cannot "
                         "be launched here.",
                         mount_error);
            }
            icons_reset(&app);
        }
        if (app.installed_discovery_requested) {
            app.installed_discovery_requested = false;
            errno = 0;
            if (app.dsi_mode && nandInit(true))
                installed_catalog_begin(&app.installed_catalog, "nand:/", app.title_language);
            else {
                installed_catalog_cancel(&app.installed_catalog);
                app.installed_catalog.attempted = true;
                app.installed_catalog.ready = false;
                snprintf(app.installed_catalog.status, sizeof app.installed_catalog.status,
                         "Read-only NAND unavailable (error %d). Press B to return.", errno);
            }
            if (app.category == CATEGORY_SYSTEM && app.installed_open)
                app_scan(&app);
        }
        if (app.system_title_requested >= 0 && boot_keys_released) {
            int title = app.system_title_requested;
            app.system_title_requested = -1;
            uint32_t request[8];
            const NativeTitle *native = NULL;
            if (title < 5)
                native = &app.system_catalog.titles[title];
            else if ((unsigned)(title - 5) < app.installed_catalog.count &&
                     installed_title_revalidate(&app.installed_catalog, (unsigned)(title - 5)) &&
                     app.installed_catalog.titles[title - 5].title.low == app.installed_selected_id)
                native = &app.installed_catalog.titles[title - 5].title;
            if (!native || !native->available) {
                path_copy(app.message,
                          title >= 5
                              ? "Installed title is unavailable or changed. Refresh the library."
                          : *app.system_catalog.status ? app.system_catalog.status
                                                       : "System title was not discovered.",
                          sizeof app.message);
                app_panel(&app, PANEL_MESSAGE);
            } else if (app_save(&app) && system_title_request(request, native)) {
                unsigned char autoload[0x400];
                system_title_autoload(autoload, native);
                stop_update_checker();
                stop_launcher_audio();
                memcpy((void *)0x02000300, request, sizeof request);
                memcpy((void *)0x02000800, autoload, sizeof autoload);
                DC_FlushAll();
                systemReboot();
                while (1)
                    swiWaitForVBlank();
            } else {
                snprintf(app.message, sizeof app.message,
                         "System-title request unavailable or preferences could not be saved. No "
                         "reboot occurred.");
                app_panel(&app, PANEL_MESSAGE);
            }
        }
        if (app.system_menu_requested && boot_keys_released) {
            app.system_menu_requested = false;
            if (!app.storage_lost && !app_save(&app)) {
                snprintf(
                    app.message, sizeof app.message,
                    "Could not save navigation before returning to DSi Menu. Check SD access.");
                app_panel(&app, PANEL_MESSAGE);
            } else {
                uint32_t request[8];
                system_menu_request(request);
                /* Unlaunch ignores an empty title ID and chooses its default.
                 * Supply the discovered Launcher path to reach the actual Menu.
                 * Stock firmware still receives its normal zero-ID request. */
                if (!app.system_catalog.attempted && app.dsi_mode && nandInit(true))
                    system_catalog_scan(&app.system_catalog, "nand:/");
                unsigned char autoload[0x400];
                system_title_autoload(autoload, &app.system_catalog.launcher);
                stop_update_checker();
                stop_launcher_audio();
                memcpy((void *)0x02000300, request, sizeof request);
                memcpy((void *)0x02000800, autoload, sizeof autoload);
                DC_FlushAll();
                systemReboot();
                while (1)
                    swiWaitForVBlank();
            }
        }
        unsigned launch_buttons = KEY_A | KEY_B | KEY_X | KEY_Y | KEY_START | KEY_SELECT | KEY_L |
                                  KEY_R | KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_TOUCH;
        if (app.launch_requested && !(keysHeld() & launch_buttons)) {
            app.launch_requested = app.startup_due = false;
            browser_scan_cancel(&app);
            installed_catalog_cancel(&app.installed_catalog);
            if (!app_save(&app)) {
                snprintf(app.message, sizeof app.message,
                         "Could not save navigation before launch. Check SD access.");
                app_panel(&app, PANEL_MESSAGE);
            } else {
                /* A request is not proof the launched application started. */
                dashboard_record_request(&app, app.launch.content);
                stop_update_checker();
                stop_launcher_audio();
                bool prepared = bootstrap_commit(app.root, &app.launch, app.message);
                if (!prepared) {
                    diagnostics_record_launch(&app, LAUNCH_REPORT_PREPARATION_FAILED, app.message);
                } else {
                    diagnostics_record_launch(&app, LAUNCH_REPORT_HANDOFF_REQUESTED, NULL);
                }
                bool handed_off = prepared && launch_platform(&app.launch, app.message);
                if (prepared && !handed_off)
                    diagnostics_record_launch(&app, LAUNCH_REPORT_LOADER_FAILED, app.message);
                if (!handed_off) {
                    audio_request_reload(&app);
                    app_panel(&app, PANEL_MESSAGE);
                }
            }
        }
        update_console_status();
        update_checker_tick();
        slot1_tick(&app, &slot1_probe, &slot1_operations);
        update_launcher_audio();
        app_tick(&app);
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        char clock[12] = "--:--";
        if (tm)
            settings_format_clock(clock, tm->tm_hour, tm->tm_min, app.clock_12_hour);
        render_app(&app, top, bottom, clock);
        DC_FlushRange(top, sizeof top);
        DC_FlushRange(bottom, sizeof bottom);
        swiWaitForVBlank();
        dmaCopy(top, bgGetGfxPtr(mainbg), sizeof top);
        dmaCopy(bottom, bgGetGfxPtr(subbg), sizeof bottom);
        cothread_yield();
    }
    return 0;
}
#endif
