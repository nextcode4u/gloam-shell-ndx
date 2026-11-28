#ifndef FRONTEND_DIAGNOSTICS_H
#define FRONTEND_DIAGNOSTICS_H
#include "types.h"
typedef enum {
    LAUNCH_REPORT_REJECTED,
    LAUNCH_REPORT_PREPARATION_FAILED,
    LAUNCH_REPORT_HANDOFF_REQUESTED,
    LAUNCH_REPORT_LOADER_FAILED
} LaunchReportStage;
/* Main-loop observations only; this module never reads hardware registers. */
void diagnostics_observe_hardware(uint32_t control, uint32_t data_regions,
                                  uint32_t instruction_regions, uint32_t scfg_clock);
void diagnostics_observe_frame(unsigned vblanks, unsigned frames, bool image_pending);
bool diagnostics_export(const App *app);
bool diagnostics_record_launch(const App *app, LaunchReportStage stage, const char *error);
#endif
