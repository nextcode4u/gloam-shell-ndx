#ifndef FRONTEND_ARGUMENTS_H
#define FRONTEND_ARGUMENTS_H
#include "types.h"
bool arguments_add(ArgumentList *args, const char *value);
const char *arguments_get(const ArgumentList *args, int index);
bool arguments_read(const char *path, ArgumentList *args, char error[160]);
#endif
