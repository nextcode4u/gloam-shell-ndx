#ifndef GLOAM_UPDATE_TRANSPORT_H
#define GLOAM_UPDATE_TRANSPORT_H
#include "update_checker.h"
/* wait yields to the UI and returns false on cancellation/deadline. DNS is network order. */
bool update_fetch(const char *ca_file, uint32_t dns, bool (*wait)(void), char tag[UPDATE_TAG_CAP]);
#endif
