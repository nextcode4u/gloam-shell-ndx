#ifndef FRONTEND_LAUNCH_H
#define FRONTEND_LAUNCH_H
#include "types.h"
bool launch_is_homebrew_header(const unsigned char header[16]);
/* Read a complete header and publish size only after metadata/read/close checks pass. */
bool launch_read_header(const char *path, void *header, size_t bytes, uint32_t *file_bytes);
/* False means inspection failed, rather than a valid non-homebrew result. */
bool launch_probe_homebrew(const char *path, bool *homebrew);
/* Bounded launch-only fallback for custom IDs with a valid linked DLDI header. */
bool launch_is_homebrew_file(const char *path);
/* Resolve a Slot-1 helper without applying content associations or retail backends. */
bool launch_prepare_slot1(const App *app, LaunchPlan *plan, char error[160]);
bool launch_prepare_installer(const App *app, LaunchPlan *plan, char error[160]);
bool launch_prepare(const App *app, const char *path, LaunchPlan *plan, char error[160]);
/* Revalidate a handler chosen by the resolver before using its argv/backend flags. */
bool launch_prepare_handler(const App *app, const char *path, int handler, LaunchPlan *plan,
                            char error[160]);
bool launch_platform(const LaunchPlan *plan, char error[160]);
#endif
