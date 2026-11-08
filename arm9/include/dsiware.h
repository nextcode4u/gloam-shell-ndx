#ifndef FRONTEND_DSIWARE_H
#define FRONTEND_DSIWARE_H
#include "types.h"
bool dsiware_prepare(const App *app, const char *target, const unsigned char header[0x240],
                     LaunchPlan *plan, char error[160]);
bool dsiware_create_save(const char *path, uint32_t bytes, char error[160]);
bool dsiware_alias(const char *root, const char *path, char output[64]);
#endif
