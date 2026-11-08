#ifndef FRONTEND_CONFIGURATION_RESET_H
#define FRONTEND_CONFIGURATION_RESET_H
#include <stdbool.h>
bool configuration_reset(const char *root, char error[160]);
bool configuration_reset_recover(const char *root, char error[160]);
#endif
