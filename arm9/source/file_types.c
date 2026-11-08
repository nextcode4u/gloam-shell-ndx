/* File presentation has no dependency on installed players or filesystem reads. */
#include "file_types.h"
#include "paths.h"
#include <string.h>
#include <strings.h>

typedef struct {
    const char *extension;
    PlaceholderIcon icon;
    const char *label;
} ExtensionType;
/* Sorted by extension for logarithmic lookup in the renderer's hot path. */
static const ExtensionType extensions[] = {
    {"7z", PLACEHOLDER_ARCHIVE, "ARCHIVE"},  {"a26", PLACEHOLDER_GAME, "ATARI"},
    {"a52", PLACEHOLDER_GAME, "ATARI"},      {"a78", PLACEHOLDER_GAME, "ATARI"},
    {"aac", PLACEHOLDER_AUDIO, "AUDIO"},     {"aif", PLACEHOLDER_AUDIO, "AUDIO"},
    {"aiff", PLACEHOLDER_AUDIO, "AUDIO"},    {"app", PLACEHOLDER_GAME, "DS/DSi"},
    {"argv", PLACEHOLDER_SHORTCUT, "ARGV"},  {"atr", PLACEHOLDER_GAME, "ATARI"},
    {"avi", PLACEHOLDER_VIDEO, "VIDEO"},     {"bmp", PLACEHOLDER_IMAGE, "IMAGE"},
    {"bz2", PLACEHOLDER_ARCHIVE, "ARCHIVE"}, {"c10", PLACEHOLDER_GAME, "ROM"},
    {"cas", PLACEHOLDER_GAME, "ROM"},        {"cbr", PLACEHOLDER_BOOK, "BOOK"},
    {"cbz", PLACEHOLDER_BOOK, "BOOK"},       {"ccc", PLACEHOLDER_GAME, "ROM"},
    {"cfg", PLACEHOLDER_CONFIG, "CONFIG"},   {"cht", PLACEHOLDER_CONFIG, "CHEATS"},
    {"col", PLACEHOLDER_GAME, "COLECO"},     {"conf", PLACEHOLDER_CONFIG, "CONFIG"},
    {"cpr", PLACEHOLDER_GAME, "ROM"},        {"crt", PLACEHOLDER_GAME, "ROM"},
    {"csv", PLACEHOLDER_TEXT, "TEXT"},       {"d64", PLACEHOLDER_GAME, "ROM"},
    {"dan", PLACEHOLDER_GAME, "ROM"},        {"desktop", PLACEHOLDER_SHORTCUT, "SHORTCUT"},
    {"dpg", PLACEHOLDER_VIDEO, "VIDEO"},     {"dsconfig", PLACEHOLDER_CONFIG, "CONFIG"},
    {"dsi", PLACEHOLDER_GAME, "DS/DSi"},     {"dsi_mode", PLACEHOLDER_GAME, "DS/DSi"},
    {"dsk", PLACEHOLDER_GAME, "ROM"},        {"dsv", PLACEHOLDER_SAVE, "SAVE"},
    {"elf", PLACEHOLDER_APPLICATION, "APP"}, {"epub", PLACEHOLDER_BOOK, "BOOK"},
    {"fb2", PLACEHOLDER_BOOK, "BOOK"},       {"fds", PLACEHOLDER_GAME, "NES"},
    {"flac", PLACEHOLDER_AUDIO, "AUDIO"},    {"fv", PLACEHOLDER_VIDEO, "VIDEO"},
    {"gb", PLACEHOLDER_GAME, "GAME BOY"},    {"gba", PLACEHOLDER_GAME, "GBA"},
    {"gbc", PLACEHOLDER_GAME, "GAME BOY"},   {"gen", PLACEHOLDER_GAME, "GENESIS"},
    {"gg", PLACEHOLDER_GAME, "SEGA"},        {"gif", PLACEHOLDER_IMAGE, "IMAGE"},
    {"gz", PLACEHOLDER_ARCHIVE, "ARCHIVE"},  {"ico", PLACEHOLDER_IMAGE, "IMAGE"},
    {"ini", PLACEHOLDER_CONFIG, "CONFIG"},   {"int", PLACEHOLDER_GAME, "ROM"},
    {"it", PLACEHOLDER_AUDIO, "AUDIO"},      {"jpeg", PLACEHOLDER_IMAGE, "IMAGE"},
    {"jpg", PLACEHOLDER_IMAGE, "IMAGE"},     {"json", PLACEHOLDER_CONFIG, "CONFIG"},
    {"k7", PLACEHOLDER_GAME, "ROM"},         {"lnk", PLACEHOLDER_SHORTCUT, "SHORTCUT"},
    {"lnx", PLACEHOLDER_GAME, "ROM"},        {"log", PLACEHOLDER_TEXT, "LOG"},
    {"m4a", PLACEHOLDER_AUDIO, "AUDIO"},     {"m4v", PLACEHOLDER_VIDEO, "VIDEO"},
    {"m5", PLACEHOLDER_GAME, "ROM"},         {"md", PLACEHOLDER_TEXT, "TEXT"},
    {"mid", PLACEHOLDER_AUDIO, "AUDIO"},     {"midi", PLACEHOLDER_AUDIO, "AUDIO"},
    {"min", PLACEHOLDER_GAME, "POKEMON"},    {"mkv", PLACEHOLDER_VIDEO, "VIDEO"},
    {"mobi", PLACEHOLDER_BOOK, "BOOK"},      {"mod", PLACEHOLDER_AUDIO, "AUDIO"},
    {"mov", PLACEHOLDER_VIDEO, "VIDEO"},     {"mp3", PLACEHOLDER_AUDIO, "AUDIO"},
    {"mp4", PLACEHOLDER_VIDEO, "VIDEO"},     {"mpeg", PLACEHOLDER_VIDEO, "VIDEO"},
    {"mpg", PLACEHOLDER_VIDEO, "VIDEO"},     {"msx", PLACEHOLDER_GAME, "ROM"},
    {"mx2", PLACEHOLDER_GAME, "ROM"},        {"mxd", PLACEHOLDER_GAME, "ROM"},
    {"nds", PLACEHOLDER_GAME, "DS/DSi"},     {"nes", PLACEHOLDER_GAME, "NES"},
    {"nfo", PLACEHOLDER_TEXT, "TEXT"},       {"ngc", PLACEHOLDER_GAME, "NEOGEO"},
    {"ngp", PLACEHOLDER_GAME, "NEOGEO"},     {"ogg", PLACEHOLDER_AUDIO, "AUDIO"},
    {"pce", PLACEHOLDER_GAME, "PCE"},        {"pcm", PLACEHOLDER_AUDIO, "AUDIO"},
    {"pdf", PLACEHOLDER_BOOK, "PDF"},        {"png", PLACEHOLDER_IMAGE, "IMAGE"},
    {"prg", PLACEHOLDER_GAME, "ROM"},        {"prv", PLACEHOLDER_SAVE, "SAVE"},
    {"pub", PLACEHOLDER_SAVE, "SAVE"},       {"rar", PLACEHOLDER_ARCHIVE, "ARCHIVE"},
    {"rpk", PLACEHOLDER_GAME, "ROM"},        {"rst", PLACEHOLDER_TEXT, "TEXT"},
    {"rtf", PLACEHOLDER_TEXT, "TEXT"},       {"rvid", PLACEHOLDER_VIDEO, "VIDEO"},
    {"s3m", PLACEHOLDER_AUDIO, "AUDIO"},     {"sav", PLACEHOLDER_SAVE, "SAVE"},
    {"savestate", PLACEHOLDER_SAVE, "SAVE"}, {"sfc", PLACEHOLDER_GAME, "SNES"},
    {"sg", PLACEHOLDER_GAME, "SEGA"},        {"sgb", PLACEHOLDER_GAME, "GAME BOY"},
    {"smc", PLACEHOLDER_GAME, "SNES"},       {"sms", PLACEHOLDER_GAME, "SEGA"},
    {"srl", PLACEHOLDER_GAME, "DS/DSi"},     {"srm", PLACEHOLDER_SAVE, "SAVE"},
    {"ss0", PLACEHOLDER_SAVE, "SAVE"},       {"ss1", PLACEHOLDER_SAVE, "SAVE"},
    {"ss2", PLACEHOLDER_SAVE, "SAVE"},       {"sta", PLACEHOLDER_SAVE, "SAVE"},
    {"state", PLACEHOLDER_SAVE, "SAVE"},     {"sv", PLACEHOLDER_GAME, "ROM"},
    {"svg", PLACEHOLDER_IMAGE, "IMAGE"},     {"tap", PLACEHOLDER_GAME, "ROM"},
    {"tar", PLACEHOLDER_ARCHIVE, "ARCHIVE"}, {"tga", PLACEHOLDER_IMAGE, "IMAGE"},
    {"tif", PLACEHOLDER_IMAGE, "IMAGE"},     {"tiff", PLACEHOLDER_IMAGE, "IMAGE"},
    {"toml", PLACEHOLDER_CONFIG, "CONFIG"},  {"txt", PLACEHOLDER_TEXT, "TEXT"},
    {"tzx", PLACEHOLDER_GAME, "ROM"},        {"url", PLACEHOLDER_SHORTCUT, "SHORTCUT"},
    {"wav", PLACEHOLDER_AUDIO, "AUDIO"},     {"webm", PLACEHOLDER_VIDEO, "VIDEO"},
    {"webp", PLACEHOLDER_IMAGE, "IMAGE"},    {"ws", PLACEHOLDER_GAME, "WONDERSWAN"},
    {"wsc", PLACEHOLDER_GAME, "WONDERSWAN"}, {"xex", PLACEHOLDER_GAME, "ATARI"},
    {"xm", PLACEHOLDER_AUDIO, "AUDIO"},      {"xml", PLACEHOLDER_CONFIG, "CONFIG"},
    {"xz", PLACEHOLDER_ARCHIVE, "ARCHIVE"},  {"yaml", PLACEHOLDER_CONFIG, "CONFIG"},
    {"yml", PLACEHOLDER_CONFIG, "CONFIG"},   {"z80", PLACEHOLDER_GAME, "ROM"},
    {"zip", PLACEHOLDER_ARCHIVE, "ARCHIVE"},
};

const char *placeholder_label(PlaceholderIcon icon)
{
    static const char *labels[] = {"FILE",     "FOLDER",  "GAME",     "APP",    "AUDIO",
                                   "VIDEO",    "BOOK",    "DOCUMENT", "IMAGE",  "SAVE DATA",
                                   "SETTINGS", "ARCHIVE", "SHORTCUT", "SYSTEM", "MISSING"};
    return (unsigned)icon < PLACEHOLDER_COUNT ? labels[icon] : "FILE";
}
EntryFileType entry_file_type(const Entry *entry)
{
    if (!entry)
        return (EntryFileType){PLACEHOLDER_FILE, ""};
    if (entry->kind == ITEM_INSTALLED_TITLE)
        return (EntryFileType){PLACEHOLDER_GAME, "DSiWare"};
    if (entry->kind == ITEM_INSTALLED_LIBRARY)
        return (EntryFileType){PLACEHOLDER_FOLDER, "NAND"};
    if (entry->kind == ITEM_DIRECTORY)
        return (EntryFileType){PLACEHOLDER_FOLDER, "FOLDER"};
    if (entry->kind == ITEM_CATEGORY) {
        PlaceholderIcon icon = entry->target == CATEGORY_GAMES    ? PLACEHOLDER_GAME
                               : entry->target == CATEGORY_APPS   ? PLACEHOLDER_APPLICATION
                               : entry->target == CATEGORY_SYSTEM ? PLACEHOLDER_SYSTEM
                                                                  : PLACEHOLDER_FOLDER;
        return (EntryFileType){icon, "LINK"};
    }
    if (entry->kind == ITEM_SYSTEM)
        return (EntryFileType){PLACEHOLDER_SYSTEM, "SYSTEM APP"};
    if (entry->kind == ITEM_MISSING)
        return (EntryFileType){PLACEHOLDER_MISSING, "MISSING SHORTCUT"};
    const char *extension = path_extension(*entry->name ? entry->name : entry->path);
    /* .md is also Markdown. Only the conventional ROM tree implies Genesis. */
    if (!strcasecmp(extension, "md"))
        for (const char *p = entry->path; *p; p++)
            if (*p == '/' && !strncasecmp(p + 1, "roms/", 5))
                return (EntryFileType){PLACEHOLDER_GAME, "GENESIS"};
    size_t first = 0, last = sizeof extensions / sizeof *extensions;
    while (first < last) {
        size_t middle = first + (last - first) / 2;
        int order = strcasecmp(extension, extensions[middle].extension);
        if (!order)
            return (EntryFileType){extensions[middle].icon, extensions[middle].label};
        if (order < 0)
            last = middle;
        else
            first = middle + 1;
    }
    return (EntryFileType){PLACEHOLDER_FILE, "FILE"};
}
