#ifndef FRONTEND_BOOTSTRAP_H
#define FRONTEND_BOOTSTRAP_H
#include "types.h"
bool bootstrap_prepare(const App *app, const char *target, LaunchPlan *plan, char error[160]);
bool bootstrap_prepare_homebrew(const App *app, const char *target, const char *argument,
                                LaunchPlan *plan, char error[160]);
bool bootstrap_commit(const char *root, const LaunchPlan *plan, char error[160]);
#endif
