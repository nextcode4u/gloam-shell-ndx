#ifndef FRONTEND_LAUNCH_H
#define FRONTEND_LAUNCH_H
#include "types.h"
bool launch_is_homebrew_header(const unsigned char header[16]);
/* Bounded launch-only fallback for custom IDs with a valid linked DLDI header. */
bool launch_is_homebrew_file(const char *path);
/* Resolve a Slot-1 helper without applying content associations or retail backends. */
bool launch_prepare_slot1(const App *app, LaunchPlan *plan, char error[160]);
bool launch_prepare_installer(const App *app, LaunchPlan *plan, char error[160]);
bool launch_prepare(const App *app, const char *path, LaunchPlan *plan, char error[160]);
bool launch_platform(const LaunchPlan *plan, char error[160]);
#endif
