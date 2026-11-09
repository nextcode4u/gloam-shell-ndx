/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GLOAM_SCUMMVM_H
#define GLOAM_SCUMMVM_H
#include "types.h"

/* Opt-in native ScummVM discovery; does not edit ScummVM's game configuration. */
bool scummvm_prepare_arguments(const char *root, LaunchPlan *plan, char error[160]);
#endif
