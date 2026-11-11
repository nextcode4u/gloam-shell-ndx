/* nds-bootstrap adapter. Preflight is read-only; commit runs just before handoff. */
#include "bootstrap.h"
#include "arguments.h"
#include "cheats.h"
#include "dsiware.h"
#include "game_options.h"
#include "launch.h"
#include "paths.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static uint32_t le32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint16_t header_crc(const unsigned char *data, size_t size)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    return crc;
}
static bool regular_size(const char *path, uint32_t *size)
{
    struct stat status;
    if (stat(path, &status) || !S_ISREG(status.st_mode) || status.st_size < 0 ||
        (uint64_t)status.st_size > UINT32_MAX)
        return false;
    *size = (uint32_t)status.st_size;
    return true;
}
static bool save_path(const App *app, const char *target, LaunchPlan *plan, char error[160])
{
    char directory[PATH_CAP], stem[PATH_CAP], candidate[PATH_CAP], name[PATH_CAP];
    path_copy(directory, target, PATH_CAP);
    char *slash = strrchr(directory, '/');
    if (!slash)
        return false;
    path_copy(stem, slash + 1, PATH_CAP);
    slash[1] = 0;
    char *dot = strrchr(stem, '.');
    if (dot)
        *dot = 0;
    int slot = plan->options.values[GAME_SLOT];
    if (slot) {
        if (snprintf(name, sizeof name, "saves/%s.sav%d", stem, slot) >= (int)sizeof name ||
            !path_join(plan->save, directory, name))
            return false;
    } else {
        int found = 0;
        for (int variant = 0; variant < 3; variant++) {
            int length = snprintf(name, sizeof name,
                                  variant == 0   ? "%s.sav"
                                  : variant == 1 ? "%s.sav"
                                                 : "saves/%s.sav",
                                  variant == 1 ? strrchr(target, '/') + 1 : stem);
            if (length >= (int)sizeof name || !path_join(candidate, directory, name))
                return false;
            struct stat status;
            if (!stat(candidate, &status)) {
                if (!S_ISREG(status.st_mode)) {
                    snprintf(error, 160, "A save path is occupied by a directory.");
                    return false;
                }
                path_copy(plan->save, candidate, PATH_CAP);
                found++;
            }
        }
        if (found > 1) {
            snprintf(error, 160,
                     "Multiple slot-0 saves exist. Keep the intended save at one recognized path; "
                     "no files were changed.");
            return false;
        }
        if (!found) {
            if (snprintf(name, sizeof name, "saves/%s.sav", stem) >= (int)sizeof name ||
                !path_join(plan->save, directory, name))
                return false;
        }
    }
    uint32_t existing;
    if (regular_size(plan->save, &existing)) {
        if (existing == 0 || existing > 64 * 1024 * 1024) {
            snprintf(error, 160,
                     "Existing save is empty or exceeds 64 MiB. Back it up and restore a valid "
                     "copy before retrying; it was left unchanged.");
            return false;
        }
        if (plan->save_bytes && existing != plan->save_bytes) {
            snprintf(error, 160,
                     "Existing save size differs from the expected size. Check title options; no "
                     "resize was attempted.");
            return false;
        }
        plan->save_bytes = existing;
    }
    (void)app;
    return true;
}
bool bootstrap_prepare(const App *app, const char *target, LaunchPlan *plan, char error[160])
{
    if (!path_is_within_root(target, app->root) || strchr(target, '\n') || strchr(target, '\r'))
        return false;
    unsigned char header[0x240];
    FILE *file = fopen(target, "rb");
    if (!file) {
        snprintf(error, 160,
                 "Game is missing or unreadable. Check the SD card and file path, then refresh "
                 "this folder.");
        return false;
    }
    uint32_t size;
    bool valid =
        fread(header, 1, sizeof header, file) == sizeof header && regular_size(target, &size);
    fclose(file);
    if (!valid) {
        snprintf(error, 160, "Game has a truncated header.");
        return false;
    }
    if (header_crc(header, 0x15E) != (uint16_t)(header[0x15E] | header[0x15F] << 8)) {
        snprintf(error, 160, "Game header CRC does not match. Check the selected file.");
        return false;
    }
    if (!cheats_backend_available(app->root, error))
        return false;
    for (int cpu = 0; cpu < 2; cpu++) {
        uint32_t offset = le32(header + 0x20 + cpu * 16), length = le32(header + 0x2C + cpu * 16);
        if (!length || offset < 0x200 || offset > size || length > size - offset) {
            snprintf(error, 160, "Game has invalid ARM binary file ranges.");
            return false;
        }
    }
    bool ware = header[0x12] == 3 || (header[0x12] && (header[0x1BF] & 0x10));
    plan->route = LAUNCH_BOOTSTRAP;
    path_copy(plan->target, target, PATH_CAP);
    game_options_effective(app->root, target, &plan->options);
    if (ware) {
        uint32_t extended_size = le32(header + 0x210);
        if ((extended_size && extended_size > size) ||
            !dsiware_prepare(app, target, header, plan, error))
            return false;
    } else {
        bool known = false;
        plan->save_bytes = plan->options.values[GAME_SAVE_SIZE] < 0
                               ? game_save_size(le32(header + 12), &known)
                               : game_option_save_bytes(plan->options.values[GAME_SAVE_SIZE]);
        bool wants_save =
            plan->options.values[GAME_SAVE_SIZE] != 0 && !(known && plan->save_bytes == 0);
        if (wants_save && !save_path(app, target, plan, error))
            return false;
        uint32_t existing_save;
        if (plan->options.values[GAME_SAVE_SIZE] < 0 && !known &&
            !regular_size(plan->save, &existing_save)) {
            snprintf(
                error, 160,
                "Unknown save type. Set the correct size in Title options before creating a save.");
            return false;
        }
    }
    uint32_t existing;
    if (!path_join(plan->application, app->root, "_nds/nds-bootstrap-release.nds"))
        return false;
    if (!regular_size(plan->application, &existing)) {
        snprintf(error, 160,
                 "Missing /_nds/nds-bootstrap-release.nds. Run desktop Setup > Update Gloam + "
                 "backends, or copy it from sd.zip.");
        return false;
    }
    return true;
}
static bool sd_path(char output[PATH_CAP], const char *root, const char *path)
{
    if (!path_is_within_root(path, root) || strchr(path, '\n') || strchr(path, '\r'))
        return false;
    const char *relative = path + strlen(root);
    while (*relative == '/')
        relative++;
    return path_resolve_sd(output, "sd:/", relative);
}
bool bootstrap_prepare_homebrew(const App *app, const char *target, const char *argument,
                                LaunchPlan *plan, char error[160])
{
    char reference[PATH_CAP];
    if (!sd_path(reference, app->root, target) || strlen(reference) >= 256 ||
        (*argument && (!sd_path(reference, app->root, argument) || strlen(reference) >= 256))) {
        snprintf(
            error, 160,
            "The homebrew backend requires SD application/content paths shorter than 256 bytes.");
        return false;
    }
    FILE *file = fopen(target, "rb");
    unsigned char header[0x200];
    uint32_t bytes;
    bool valid = file && fread(header, 1, sizeof header, file) == sizeof header &&
                 regular_size(target, &bytes);
    if (file)
        fclose(file);
    valid = valid && memcmp(header + 12, "KBSE", 4) && launch_is_homebrew_file(target);
    for (int cpu = 0; valid && cpu < 2; cpu++) {
        uint32_t offset = le32(header + 0x20 + cpu * 16), length = le32(header + 0x2C + cpu * 16);
        valid = length && offset >= 0x200 && offset <= bytes && length <= bytes - offset;
    }
    if (!valid) {
        snprintf(error, 160, "The selected HB backend target is not readable supported homebrew.");
        return false;
    }
    if (*argument && !regular_size(argument, &bytes)) {
        snprintf(error, 160,
                 "Homebrew content is missing or unreadable. Check its path in the selected file "
                 "handler.");
        return false;
    }
    if (!path_join(plan->application, app->root, "_nds/nds-bootstrap-hb-release.nds") ||
        !regular_size(plan->application, &bytes)) {
        snprintf(error, 160,
                 "Missing /_nds/nds-bootstrap-hb-release.nds. Run desktop Setup > Update Gloam + "
                 "backends, or copy it from sd.zip.");
        return false;
    }
    plan->route = LAUNCH_BOOTSTRAP_HB;
    path_copy(plan->target, target, PATH_CAP);
    path_copy(plan->homebrew_argument, argument, PATH_CAP);
    game_options_effective(app->root, target, &plan->options);
    return true;
}
static bool make_save(const LaunchPlan *plan, char error[160])
{
    if (!plan->save_bytes)
        return true;
    uint32_t existing;
    if (regular_size(plan->save, &existing)) {
        if (existing == plan->save_bytes)
            return true;
        snprintf(error, 160, "Save changed since preflight. It was left untouched.");
        return false;
    }
    struct stat status;
    if (!stat(plan->save, &status))
        return false;
    char directory[PATH_CAP], temporary[PATH_CAP];
    path_copy(directory, plan->save, PATH_CAP);
    char *slash = strrchr(directory, '/');
    if (!slash)
        return false;
    *slash = 0;
    if (mkdir(directory, 0777) &&
        (errno != EEXIST || stat(directory, &status) || !S_ISDIR(status.st_mode)))
        return false;
    if (snprintf(temporary, sizeof temporary, "%s.frontend-new", plan->save) >=
        (int)sizeof temporary)
        return false;
    /* Exclusive creation protects another installation's unfinished save. */
    FILE *file = fopen(temporary, "wx");
    if (!file) {
        snprintf(error, 160,
                 "Cannot create save temporary file. Check SD space or an interrupted launch.");
        return false;
    }
    unsigned char erased[4096];
    memset(erased, 0xFF, sizeof erased);
    uint32_t remaining = plan->save_bytes;
    bool written = true;
    while (remaining) {
        size_t count = remaining < sizeof erased ? remaining : sizeof erased;
        if (fwrite(erased, 1, count, file) != count) {
            written = false;
            break;
        }
        remaining -= count;
    }
    if (fclose(file))
        written = false;
    if (!written || !stat(plan->save, &status) || rename(temporary, plan->save)) {
        remove(temporary);
        snprintf(error, 160, "Could not finish the new save. Existing saves were not changed.");
        return false;
    }
    return true;
}
static char *trim(char *text)
{
    while (isspace((unsigned char)*text))
        text++;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        *--end = 0;
    return text;
}
static bool valid_ini_text(FILE *file)
{
    unsigned char block[512];
    size_t total = 0, count;
    while ((count = fread(block, 1, sizeof block, file)) != 0) {
        total += count;
        if (total > 65536)
            return false;
        for (size_t i = 0; i < count; i++)
            if (block[i] < 0x20 && block[i] != '\t' && block[i] != '\r' && block[i] != '\n')
                return false;
    }
    return !ferror(file) && fseek(file, 0, SEEK_SET) == 0;
}
static bool owned_key(const char *key, LaunchRoute route)
{
    if (route == LAUNCH_BOOTSTRAP_HB &&
        (!strcasecmp(key, "RAM_DRIVE_PATH") || !strcasecmp(key, "RAM_DRIVE_SIZE")))
        return true;
    static const char *keys[] = {
        "NDS_PATH",   "SAV_PATH", "QUIT_PATH",     "DSI_MODE",        "BOOST_CPU",
        "BOOST_VRAM", "LANGUAGE", "CARD_READ_DMA", "ASYNC_CARD_READ", "HOTKEY",
        "APP_PATH",   "PRV_PATH", "HOMEBREW_ARG",  "BNR_PATH",        "DSIWARE_SLOT1_MODE"};
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (!strcasecmp(key, keys[i]))
            return true;
    return false;
}
bool bootstrap_commit(const char *root, const LaunchPlan *plan, char error[160])
{
    if (plan->route == LAUNCH_DIRECT)
        return true;
    error[0] = 0;
    char target[PATH_CAP], save[PATH_CAP] = "", current[PATH_CAP], temporary[PATH_CAP],
                           backup[PATH_CAP];
    if (!sd_path(target, root, plan->target) || (*plan->save && !sd_path(save, root, plan->save)) ||
        !path_join(current, root, "_nds/nds-bootstrap.ini") ||
        !path_prepare_preferences_directory(root) ||
        !path_preferences_file(temporary, root, "bootstrap-next.ini") ||
        !path_preferences_file(backup, root, "bootstrap-previous.ini"))
        goto failure;
    char app_alias[64] = "", private_alias[64] = "", public_alias[64] = "";
    if (plan->route == LAUNCH_DSIWARE) {
        if (!dsiware_create_save(plan->save, plan->save_bytes, error) ||
            !dsiware_create_save(plan->private_save, plan->private_bytes, error))
            goto failure;
        if (!dsiware_alias(root, plan->target, app_alias) ||
            !dsiware_alias(root, plan->save, public_alias) ||
            !dsiware_alias(root, plan->private_save, private_alias)) {
            snprintf(error, 160,
                     "DSiWare requires existing FAT short paths shorter than 62 bytes. Shorten "
                     "folder nesting; saves stay in place.");
            goto failure;
        }
    }
    FILE *original = fopen(current, "r");
    if (!original && errno != ENOENT)
        goto failure;
    if (original && !valid_ini_text(original)) {
        fclose(original);
        snprintf(error, 160,
                 "Backend INI is unreadable, oversized or contains binary data. Restore a known "
                 "good backup; the original was left unchanged.");
        goto failure;
    }
    FILE *output = fopen(temporary, "w");
    if (!output) {
        if (original)
            fclose(original);
        goto failure;
    }
    bool in_section = false, output_in_section = false, line_ended = true, valid = true;
    char line[2048], parsed[2048];
    size_t total = 0;
    while (original && fgets(line, sizeof line, original)) {
        total += strlen(line);
        if (total > 65536 || (!strchr(line, '\n') && !feof(original))) {
            valid = false;
            break;
        }
        path_copy(parsed, line, sizeof parsed);
        char *text = trim(parsed);
        if (*text == '[') {
            char *end = strchr(text, ']');
            if (!end || *trim(end + 1)) {
                valid = false;
                break;
            }
            *end = 0;
            in_section = !strcasecmp(trim(text + 1), "NDS-BOOTSTRAP");
            if (in_section)
                continue;
            output_in_section = false;
        } else if (in_section && *text != ';' && *text != '#') {
            char *equals = strchr(text, '=');
            if (equals) {
                *equals = 0;
                if (owned_key(trim(text), plan->route))
                    continue;
            }
        }
        if (in_section) {
            if (!*text)
                continue;
            if (!output_in_section) {
                fputs("[NDS-BOOTSTRAP]\n", output);
                output_in_section = true;
            }
        }
        fputs(line, output);
        line_ended = line[strlen(line) - 1] == '\n';
    }
    if (original) {
        valid = valid && !ferror(original);
        fclose(original);
    }
    /* Reuse the final backend section when possible. Empty managed sections
     * are omitted above so repeated launches do not grow the shared INI. */
    if (!line_ended)
        fputc('\n', output);
    if (!output_in_section)
        fputs("[NDS-BOOTSTRAP]\n", output);
    fprintf(output, "NDS_PATH=%s\nSAV_PATH=%s\nQUIT_PATH=sd:/BOOT.NDS\n", target,
            plan->route == LAUNCH_DSIWARE ? public_alias
            : plan->save_bytes            ? save
                                          : "");
    const GameOptions *o = &plan->options;
    fprintf(output, "DSI_MODE=%d\nBOOST_CPU=%d\n",
            o->values[GAME_MODE] < 0 ? (plan->route == LAUNCH_BOOTSTRAP_HB ? 0 : 1)
                                     : o->values[GAME_MODE],
            o->values[GAME_CPU]);
    /* Unlike CPU/DMA options, nds-bootstrap parses BOOST_VRAM as a bool:
     * -1 would enable it. Omit Auto so the backend supplies its actual default. */
    if (o->values[GAME_VRAM] >= 0)
        fprintf(output, "BOOST_VRAM=%d\n", o->values[GAME_VRAM]);
    fprintf(output, "LANGUAGE=%d\nCARD_READ_DMA=%d\nASYNC_CARD_READ=%d\nHOTKEY=%X\n",
            o->values[GAME_LANGUAGE], o->values[GAME_DMA], o->values[GAME_ASYNC],
            game_option_hotkey(o->values[GAME_HOTKEY]));
    if (plan->route == LAUNCH_DSIWARE)
        fprintf(output, "APP_PATH=%s\nPRV_PATH=%s\nBNR_PATH=\nDSIWARE_SLOT1_MODE=0\n", app_alias,
                private_alias);
    if (plan->route == LAUNCH_BOOTSTRAP_HB) {
        char argument[PATH_CAP] = "";
        if (*plan->homebrew_argument && !sd_path(argument, root, plan->homebrew_argument))
            valid = false;
        fprintf(output, "HOMEBREW_ARG=%s\nRAM_DRIVE_PATH=\nRAM_DRIVE_SIZE=0\n", argument);
    }
    valid = valid && !ferror(output);
    if (fclose(output))
        valid = false;
    if (!valid) {
        remove(temporary);
        goto failure;
    }
    if (plan->route != LAUNCH_BOOTSTRAP_HB &&
        ((plan->route != LAUNCH_DSIWARE && !make_save(plan, error)) ||
         !cheats_backend_commit(root, plan->target, error))) {
        remove(temporary);
        goto failure;
    }
    struct stat status;
    bool had_current = !stat(current, &status);
    if (had_current) {
        if (remove(backup) && errno != ENOENT)
            goto failure;
        if (rename(current, backup))
            goto failure;
    }
    if (rename(temporary, current)) {
        if (had_current)
            rename(backup, current);
        goto failure;
    }
    return true;
failure:
    if (!*error)
        snprintf(
            error, 160,
            "Could not prepare nds-bootstrap configuration. Check SD access; no handoff occurred.");
    return false;
}
