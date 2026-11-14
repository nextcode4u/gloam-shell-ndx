#ifndef FRONTEND_SYSTEM_ROUTES_H
#define FRONTEND_SYSTEM_ROUTES_H
#include "types.h"
/* Build a native Launcher MENU warmboot request, distinct from loader exit. */
int system_destination(const char *uri);
const char *system_destination_uri(int target);
const char *system_destination_name(int target);
void system_menu_request(uint32_t request[8]);
void system_card_request(uint32_t request[8]);
/* Unlaunch SD forwarder: always follows the installed root entry point. */
void system_frontend_autoload(unsigned char request[0x400]);
void system_card_autoload(unsigned char request[0x400]);
bool system_title_request(uint32_t request[8], const NativeTitle *title);
/* Unlaunch consumes this alongside the stock Launcher request. */
bool system_title_autoload(unsigned char request[0x400], const NativeTitle *title);
/* Settings enters through Launcher; other titles retain direct Unlaunch boot. */
bool system_title_handoff(uint32_t warmboot[8], unsigned char autoload[0x400],
                          const NativeTitle *title, const SystemCatalog *catalog);
/* Prefer the validated language-patched NAND menu for both return and Settings. */
bool system_launcher_autoload(unsigned char request[0x400], const SystemCatalog *catalog);
/* Structural validation only; does not authenticate Nintendo signatures. */
bool native_title_content(const char *directory, uint32_t high, uint32_t low,
                          uint32_t *boot_content, bool allow_unsigned);
void system_catalog_scan(SystemCatalog *catalog, const char *nand_root);
#endif
