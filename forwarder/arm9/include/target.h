#ifndef GLOAM_FORWARDER_TARGET_H
#define GLOAM_FORWARDER_TARGET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Reject missing/truncated executables and accidentally forwarding to ourselves. */
bool forwarder_target_valid(const unsigned char *header, size_t bytes, uint64_t file_size);
#endif
