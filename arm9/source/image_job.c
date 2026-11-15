/* Cooperative decoder context. Checkpoints occur only between complete
 * filesystem/libc calls or inside self-contained decoder loops. */
#include "image_job.h"
#include "artwork.h"
#include "image_decode.h"
#include "paths.h"
#include <string.h>

#if !defined(HOST_PREVIEW) || defined(FRONTEND_ASYNC_TEST)
#include <ucontext.h>
static ucontext_t caller, worker;
static unsigned char stack[32768] __attribute__((aligned(8)));
static Artwork output;
static char source[PATH_CAP];
static bool embedded_source, active, running, completed, decoded, cancelled;
static unsigned checkpoints, yields, slice_start;
static void (*task_callback)(void *);
static void *task_context;
static unsigned (*budget_clock)(void);
static unsigned clock_period, clock_budget;

void image_job_set_clock(unsigned (*clock)(void), unsigned period, unsigned budget)
{
    budget_clock = clock && period && budget && budget < period ? clock : NULL;
    clock_period = period;
    clock_budget = budget;
}

static void decode_source(void)
{
    if (task_callback)
        task_callback(task_context);
    else
        decoded = embedded_source ? embedded_album_art(&output, source)
                                  : artwork_decode_image(&output, source);
    completed = true;
    running = false;
}
bool image_job_supported(void)
{
    return true;
}
static bool start_worker(void)
{
    if (getcontext(&worker))
        return false;
    worker.uc_stack.ss_sp = stack;
    worker.uc_stack.ss_size = sizeof stack;
    worker.uc_stack.ss_flags = 0;
    worker.uc_link = &caller;
    makecontext(&worker, decode_source, 0);
    active = true;
    image_job_step();
    return true;
}
bool image_job_begin(const char *path, bool embedded)
{
    if (active || !path || !*path || strlen(path) >= sizeof source)
        return false;
    memset(&output, 0, sizeof output);
    path_copy(source, path, sizeof source);
    embedded_source = embedded;
    completed = decoded = cancelled = false;
    checkpoints = yields = 0;
    task_callback = NULL;
    return start_worker();
}
bool image_job_begin_task(void (*task)(void *), void *context)
{
    if (active || !task)
        return false;
    task_callback = task;
    task_context = context;
    completed = decoded = cancelled = false;
    checkpoints = yields = 0;
    return start_worker();
}
bool image_job_task_pending(void)
{
    return active && task_callback;
}
bool image_job_task_finish(void)
{
    if (!image_job_task_pending() || !completed)
        return false;
    active = completed = false;
    task_callback = NULL;
    task_context = NULL;
    return true;
}
void image_job_step(void)
{
    if (!active || completed)
        return;
    checkpoints = 0;
    slice_start = budget_clock ? budget_clock() % clock_period : 0;
    running = true;
    swapcontext(&caller, &worker);
    running = false;
}
void image_decode_checkpoint(void)
{
    if (!running || cancelled)
        return;
    checkpoints++;
    if (budget_clock) {
        unsigned now = budget_clock() % clock_period;
        unsigned elapsed =
            now >= slice_start ? now - slice_start : clock_period - slice_start + now;
        if (elapsed < clock_budget && checkpoints < 256)
            return;
    } else if (checkpoints < 4)
        return;
    yields++;
    running = false;
    swapcontext(&worker, &caller);
    running = true;
}
void image_job_cancel(void)
{
    if (active)
        cancelled = true;
}
bool image_job_cancelled(void)
{
    return running && cancelled;
}
bool image_job_busy(void)
{
    return active;
}
const Artwork *image_job_take(void)
{
    if (!active || !completed || task_callback)
        return NULL;
    output.ready = decoded;
    active = completed = false;
    return &output;
}
unsigned image_job_yields(void)
{
    return yields;
}
#else
void image_job_set_clock(unsigned (*clock)(void), unsigned period, unsigned budget)
{
    (void)clock;
    (void)period;
    (void)budget;
}
bool image_job_supported(void)
{
    return false;
}
bool image_job_begin(const char *path, bool embedded)
{
    (void)path;
    (void)embedded;
    return false;
}
bool image_job_begin_task(void (*task)(void *), void *context)
{
    (void)task;
    (void)context;
    return false;
}
bool image_job_task_pending(void)
{
    return false;
}
bool image_job_task_finish(void)
{
    return false;
}
void image_job_step(void) {}
void image_decode_checkpoint(void) {}
void image_job_cancel(void) {}
bool image_job_cancelled(void)
{
    return false;
}
bool image_job_busy(void)
{
    return false;
}
const Artwork *image_job_take(void)
{
    return NULL;
}
unsigned image_job_yields(void)
{
    return 0;
}
#endif

int image_decode_cancelled(void)
{
    return image_job_cancelled();
}
