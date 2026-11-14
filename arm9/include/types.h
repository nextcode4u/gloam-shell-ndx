#ifndef FRONTEND_TYPES_H
#define FRONTEND_TYPES_H
#include "font_pack.h"
#include "ui_strings.h"
#include "update_checker.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PATH_CAP 768
#define ENTRY_CAP 512
#define CATEGORY_COUNT 6
#define TAB_CAP 48
#define TAB_VISIBLE_COUNT 6
#define TAB_ID_CAP 32
#define TAB_LABEL_CAP 64
#define FAVORITE_CAP 64
#define ICON_CACHE_CAP 16
#define LIBRARY_INDEX_GROUPS 29
#define ASSOCIATION_CAP 32
#define USER_ASSOCIATION_CAP 32
#define EXTENSION_CAP 16
#define EDITOR_EXTENSION_CAP 96
#define ACTION_COUNT 16
#define LAUNCH_ARG_CAP 16
#define LAUNCH_DATA_CAP 4096

#define COLLECTION_CAP 8
#define COLLECTION_ITEM_CAP 32
typedef struct {
    char id[32], label[64];
    unsigned char items[COLLECTION_ITEM_CAP], count;
} NamedCollection;
typedef struct {
    NamedCollection groups[COLLECTION_CAP];
    char paths[COLLECTION_ITEM_CAP][PATH_CAP];
    int count, item_count;
    bool invalid;
} CollectionState;

typedef enum {
    CATEGORY_HOME,
    CATEGORY_GAMES,
    CATEGORY_APPS,
    CATEGORY_MEDIA,
    CATEGORY_SYSTEM,
    CATEGORY_FILES
} Category;

/* These fixed bounds are the scratch's memory budget, not filesystem limits. */
typedef enum {
    ITEM_FILE,
    ITEM_DIRECTORY,
    ITEM_CATEGORY,
    ITEM_SYSTEM,
    ITEM_MISSING,
    ITEM_INSTALLED_LIBRARY,
    ITEM_INSTALLED_TITLE
} ItemKind;
typedef struct {
    char name[256];
    char path[PATH_CAP];
    ItemKind kind;
    uint32_t size;
    int64_t modified;
    int target;
    char internal_title[128];
    bool title_loaded;
} Entry;
typedef struct {
    char path[PATH_CAP];
    int selected, scroll;
    unsigned batch;
} Location;
typedef struct {
    char id[TAB_ID_CAP];
    char label[TAB_LABEL_CAP];
    char home[PATH_CAP];
} Tab;
typedef struct {
    uint16_t pixels[96 * 104];
    int width, height;
    bool ready;
    char source[36];
} Artwork;
typedef struct {
    char path[PATH_CAP];
    uint16_t pixels[32 * 32];
    bool occupied, ready, system_fallback;
    unsigned last_used;
    unsigned char animation_tiles[8][512];
    uint16_t animation_palettes[8][16], animation_sequence[64];
    unsigned char animation_count, animation_index;
    unsigned short animation_remaining;
} BrowserIcon;
typedef struct {
    BrowserIcon icon;
    char title[128];
    bool present;
} Slot1Metadata;
typedef struct {
    uint32_t high, low;
    bool available;
    uint32_t boot_content;
} NativeTitle;
typedef struct {
    NativeTitle titles[5];
    NativeTitle launcher;
    bool patched_launcher_present, patched_launcher_valid;
    char content_paths[5][PATH_CAP];
    bool attempted, ready;
    int mount_error;
    char status[160];
} SystemCatalog;
#define INSTALLED_TITLE_CAP 128
typedef struct {
    NativeTitle title;
    char name[128];
    uint32_t bytes;
} InstalledTitle;
typedef struct {
    InstalledTitle titles[INSTALLED_TITLE_CAP];
    char root[PATH_CAP], status[160];
    void *directory;
    unsigned count, scanned, skipped;
    int language;
    bool attempted, scanning, ready, truncated;
} InstalledCatalog;
typedef struct {
    char extensions[128];
    char name[64], content_root[PATH_CAP];
    char application[PATH_CAP];
    char cover[PATH_CAP];
    char icon[PATH_CAP];
    bool use_argv, bootstrap, hbargv, disabled;
} Association;
typedef struct {
    char data[LAUNCH_DATA_CAP];
    uint16_t offsets[LAUNCH_ARG_CAP];
    int count, used;
} ArgumentList;
enum {
    GAME_MODE,
    GAME_CPU,
    GAME_VRAM,
    GAME_DMA,
    GAME_ASYNC,
    GAME_LANGUAGE,
    GAME_SLOT,
    GAME_SAVE_SIZE,
    GAME_HOTKEY,
    GAME_OPTION_COUNT
};
typedef struct {
    int values[GAME_OPTION_COUNT];
} GameOptions;
typedef struct {
    bool open, defaults;
    int selected, scroll;
    char path[PATH_CAP], status[160];
    GameOptions options, inherited;
} GameEditor;
typedef enum { LAUNCH_DIRECT, LAUNCH_BOOTSTRAP, LAUNCH_BOOTSTRAP_HB, LAUNCH_DSIWARE } LaunchRoute;
typedef struct {
    char application[PATH_CAP];
    char content[PATH_CAP];
    ArgumentList arguments;
    LaunchRoute route;
    char target[PATH_CAP], save[PATH_CAP], private_save[PATH_CAP];
    char homebrew_argument[PATH_CAP];
    GameOptions options;
    uint32_t save_bytes, private_bytes;
} LaunchPlan;
typedef enum {
    PICKER_COVER,
    PICKER_APPLICATION,
    PICKER_EXTENSION_APPLICATION,
    PICKER_AUDIO,
    PICKER_TOP_BACKGROUND,
    PICKER_BOTTOM_BACKGROUND,
    PICKER_REPAIR,
    PICKER_CONFIGURATION,
    PICKER_ICON,
    PICKER_HANDLER,
    PICKER_SPLASH
} PickerKind;
typedef struct {
    bool open, truncated, failed, confirmed;
    PickerKind kind;
    Entry *entries; /* Allocated only while this temporary view is open. */
    int count, selected, scroll, idle_frames;
    void *directory;
    unsigned batch, visited, eligible;
    int scan_count;
    char restore_path[PATH_CAP];
    char path[PATH_CAP], target[PATH_CAP], status[96];
    char extension[EXTENSION_CAP];
    Artwork preview;
} PickerState;
typedef enum { ICON_SMALL, ICON_LARGE, ICON_SIZE_COUNT } IconSize;
/* Keep 0/1 stable: saved Modern preferences now select Extended. */
typedef enum {
    BROWSER_CLASSIC,
    BROWSER_EXTENDED,
    BROWSER_LIST,
    BROWSER_GRID,
    BROWSER_SHELF,
    BROWSER_INDEX,
    BROWSER_STYLE_COUNT
} BrowserStyle;
typedef enum { SORT_NAME_ASC, SORT_NAME_DESC, SORT_TYPE, SORT_MODIFIED, SORT_COUNT } SortMode;
typedef struct {
    bool open, reset_confirm;
    bool retail_backend_present, homebrew_backend_present, slot1_helper_ready;
    unsigned text_started_frame;
    int section, selected, scroll, preset_slot;
    char status[96];
} SettingsState;
typedef enum { PANEL_NONE, PANEL_QUICK, PANEL_ACTIONS, PANEL_INFO, PANEL_MESSAGE } Panel;
typedef enum { SCROLL_SLOW, SCROLL_NORMAL, SCROLL_FAST, SCROLL_SPEED_COUNT } ScrollSpeed;
typedef enum {
    QUICK_ACCENT,
    QUICK_REDUCED_MOTION,
    QUICK_SCROLL_SPEED,
    QUICK_ICON_SIZE,
    QUICK_BROWSER_STYLE,
    QUICK_HOME,
    QUICK_SYSTEM,
    QUICK_SYSTEM_MENU,
    QUICK_SAVE,
    QUICK_SETTINGS,
    QUICK_CONTROL_COUNT
} QuickControl;
#define THEME_CAP 24
#define THEME_ID_CAP 64
typedef struct {
    uint16_t bg, surface, line, text, muted, accent, ink;
} ThemePalette;
typedef enum { MUSIC_OFF, MUSIC_THEME, MUSIC_CUSTOM, MUSIC_MODE_COUNT } MusicMode;
typedef enum { AUDIO_NONE, AUDIO_CURSOR, AUDIO_CONFIRM, AUDIO_BACK, AUDIO_EVENT_COUNT } AudioEvent;
typedef struct {
    int16_t *samples;
    unsigned bytes, rate;
} AudioClip;
typedef struct {
    MusicMode mode;
    int music_volume, effects_volume;
    bool effects_enabled, reload_requested;
    char custom_path[PATH_CAP], status[96];
    AudioClip music, effects[3];
    AudioEvent event;
} AudioState;
typedef struct {
    char id[THEME_ID_CAP], name[THEME_ID_CAP];
    char header[64];
    ThemePalette palette;
    FontPack font, heading_font;
    bool external_font, custom_heading;
    int default_view, default_icon_size;
    unsigned row_padding, large_grid_columns;
    /* Screen composition is independent of the user-selected browser view. */
    unsigned top_layout, content_opacity;
    unsigned format_version, screen_opacity[2]; /* 101 inherits content_opacity. */
    unsigned tab_style, tab_highlight, tab_gap, tab_visible;
    unsigned scene_cover_x, scene_cover_y, scene_cover_width, scene_cover_height;
    bool minimal_chrome, component_coverage;
    uint16_t selection_text, tab_text;
    char backgrounds[2][PATH_CAP];
    char icon_directory[PATH_CAP], component_directory[PATH_CAP];
    unsigned component_border;
    unsigned background_strength;
    char music[PATH_CAP], sounds[3][PATH_CAP];
} ThemePackage;
typedef struct {
    ThemePackage active, candidate;
    char ids[THEME_CAP][THEME_ID_CAP];
    int count, selected, scroll;
    bool open, ready;
    char status[96];
} ThemeState;
#define RECENT_CAP 16
typedef enum { DASH_LINKS, DASH_TABS, DASH_PINS, DASH_RECENT, DASH_SECTION_COUNT } DashboardSection;
typedef struct {
    DashboardSection order[DASH_SECTION_COUNT];
    bool enabled[DASH_SECTION_COUNT];
    unsigned char limit[DASH_SECTION_COUNT]; /* Zero shows the full bounded section. */
} DashboardConfig;
typedef struct {
    char path[PATH_CAP];
    uint32_t timestamp;
} RecentRequest;
typedef struct {
    DashboardConfig config;
    RecentRequest recent[RECENT_CAP];
    int recent_count, selected;
    bool open;
    char status[96];
} Dashboard;
typedef struct {
    bool open;
    int selected, scroll, option;
    char extensions[EDITOR_EXTENSION_CAP][EXTENSION_CAP];
    int count;
    char status[96];
} AssociationEditor;
/* Shared application state. Hardware access belongs in main.c. */
typedef struct {
    char root[PATH_CAP];
    bool storage_ready, dsi_mode;
    bool storage_lost, storage_card_present;
    Location locations[TAB_CAP];
    Tab tabs[TAB_CAP];
    int tab_count, tab_scroll;
    bool rail_focus, configuration_reload_requested, startup_cancelled, startup_due;
    bool advanced_settings, simple_settings_lock;
    UpdateState updates;
    int splash_mode;
    bool splash_once, splash_sound, splash_preview_requested;
    char splash_path[PATH_CAP];
    int auto_launch_delay;
    unsigned startup_countdown;
    int rail_selected, rail_saved_scroll;
    unsigned tab_text_started_frame;
    unsigned char tab_details[TAB_CAP];
    unsigned tab_config_skipped;
    int category;
    Entry entries[ENTRY_CAP];
    void *scan_directory; /* DIR owned by browser.c; never accessed by rendering. */
    int scan_count;
    unsigned scan_visited, scan_eligible;
    char scan_restore_path[PATH_CAP];
    int entry_count;
    bool truncated;
    bool directory_error;
    UiLanguage language;
    CollectionState collections;
    int collection_open, collection_home_selection;
    char favorites[FAVORITE_CAP][PATH_CAP];
    int favorite_count;
    Panel panel, panel_parent;
    int panel_parent_selection, panel_parent_scroll;
    bool action_enabled[ACTION_COUNT];
    int panel_selection, panel_scroll, accent_index, detail_tab;
    bool reduced_motion, preferences_dirty;
    char preferences_error[160];
    ScrollSpeed scroll_speed;
    SortMode sort_mode;
    bool show_hidden, show_system, show_extensions, show_covers, filter_recognized, clock_12_hour;
    bool hide_save_folders, hide_bios_folders;
    char startup_tab[TAB_ID_CAP];
    bool internal_titles;
    int title_language;
    SettingsState settings;
    PickerState picker;
    ThemeState themes;
    char theme_id[THEME_ID_CAP];
    char custom_backgrounds[2][PATH_CAP];
    int wallpaper_strength;
    bool recovery_mode;
    struct {
        int accent_index;
        IconSize icon_size;
        BrowserStyle browser_style;
        bool reduced_motion, accent_override, show_covers, clock_12_hour;
    } recovery_preferences;
    bool theme_accent_override;
    LaunchPlan launch;
    bool launch_requested;
    bool system_menu_requested;
    bool system_discovery_requested, slot1_requested;
    int system_title_requested;
    bool system_notice_active;
    unsigned system_notice_started;
    int system_notice_target;
    SystemCatalog system_catalog;
    Slot1Metadata slot1;
    InstalledCatalog installed_catalog;
    bool installed_open, installed_discovery_requested;
    uint32_t installed_selected_id;
    IconSize icon_size;
    BrowserStyle browser_style;
    BrowserIcon icons[ICON_CACHE_CAP];
    int entry_icons[ENTRY_CAP];
    Association associations[ASSOCIATION_CAP];
    int association_count;
    Association user_associations[USER_ASSOCIATION_CAP];
    int user_association_count;
    AssociationEditor association_editor;
    Dashboard dashboard;
    GameEditor game_editor;
    struct CheatState *cheats;
    AudioState audio;
    int battery_level, brightness, requested_brightness;
    bool battery_charging;
    unsigned icon_reads, icons_idle_frames;
    int panel_age, selection_idle_frames, preferences_idle_frames;
    int selection_y_fixed; /* Selection offset in 24.8 fixed-point pixels. */
    unsigned frame_count;
    char message[160], title[128], code[8];
    char info_title[128], info_code[5];
    bool info_loaded, info_header_valid;
    unsigned char info_unit, info_revision;
    Location folder_history[8];
    bool library_jump_held, library_index_dirty;
    uint16_t library_index_counts[LIBRARY_INDEX_GROUPS];
    int16_t library_index_first[LIBRARY_INDEX_GROUPS];
    Artwork artwork;
    Artwork shelf_artwork[3];
    bool shelf_resolved[3];
    char shelf_paths[3][PATH_CAP];
    int shelf_offset_fixed; /* Horizontal shelf motion in 24.8 pixels. */
    bool artwork_resolved;
    unsigned artwork_reads;
} App;
#endif
