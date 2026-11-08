#ifndef FRONTEND_DIAGNOSTICS_H
#define FRONTEND_DIAGNOSTICS_H
#include "types.h"
typedef enum {
    LAUNCH_REPORT_REJECTED,
    LAUNCH_REPORT_PREPARATION_FAILED,
    LAUNCH_REPORT_HANDOFF_REQUESTED,
    LAUNCH_REPORT_LOADER_FAILED
} LaunchReportStage;
bool diagnostics_export(const App *app);
bool diagnostics_record_launch(const App *app, LaunchReportStage stage, const char *error);
#endif
