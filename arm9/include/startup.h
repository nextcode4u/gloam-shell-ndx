#ifndef FRONTEND_STARTUP_H
#define FRONTEND_STARTUP_H
#include "types.h"
void startup_prepare(App *app);
void startup_cancel(App *app);
void startup_tick(App *app);
#endif
