#ifndef FRONTEND_IMAGE_JOB_H
#define FRONTEND_IMAGE_JOB_H
#include "types.h"
/* One decoder owns its stack and source; it never reads application state. */
bool image_job_supported(void);
/* Optional periodic budget clock. Hardware access stays in the frame loop;
 * tools without a clock use a conservative work-unit limit. */
void image_job_set_clock(unsigned (*clock)(void), unsigned period, unsigned budget);
bool image_job_begin(const char *path, bool embedded);
void image_job_step(void);
bool image_job_busy(void);
void image_job_cancel(void);
bool image_job_cancelled(void);
const Artwork *image_job_take(void);
unsigned image_job_yields(void);
/* Decoder checkpoints are no-ops outside the explicitly resumed worker. */
void image_decode_checkpoint(void);
int image_decode_cancelled(void);
#endif
