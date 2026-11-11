# Bounded folder browsing

The native browser enumerates at most 16 directory records per frame, counting
hidden and unsupported entries too. Until a batch is complete, the bottom
screen says Reading folder and does not publish a partially sorted list. Input,
tab changes and Back remain available between steps. Changing folders/tabs
closes the previous enumeration; SD loss cancels it. Icon, title and artwork
work follows directory completion rather than competing with each scan step.

Each displayed batch holds up to 512 entries. SELECT → Next folder batch or
Previous folder batch reaches additional entries without allocating an entire
large directory. Each batch sorts folders first using the current sorting
choice. Batch membership follows filesystem enumeration order: sorting is
within a batch, not a globally sorted index of the entire directory. Unchanged
folder contents produce repeatable batches; adding/removing files can shift
membership. Previous batches are scanned incrementally to reach later batches.
The next action wraps to the first batch from the last batch; the contextual
Next action is enabled only when another batch exists. Current batch numbers
are saved with tab IDs; entering a new folder or its parent starts at batch 0.

Host snapshot consumers still request a completed list through `app_scan`.
The same begin/step/cancel functions are exercised explicitly by
`tests/browser_jobs_test.c`, including all 1,000 files across two batches,
per-step work bounds, cancellation, saved batch state and storage loss.

File pickers use the same 16-record work budget and publish completed batches
of up to 512 matching entries. L/R changes batches; Left/Right moves by seven
rows within a batch. Touch the header arrows to change batches. Hidden and
unsupported records count against the work budget, and the hidden-file setting
also applies to pickers. Back, cancellation and SD loss close their scan handle.
The originating browser location and pending scan are preserved while a picker
is open. `tests/picker_jobs_test.c` checks 1,000 supported files interspersed with
1,000 unsupported files, batch coverage, work bounds and cancellation.

Image decoding yields cooperatively through a separate job; see
[artwork cache](ARTWORK_CACHE.md). Bounded work is not a physical timing claim.


Optional auto-launch is Off by default. Browser & Home offers 3/5/10-second
startup countdowns for the most recent request. Any input cancels and is
consumed for that frame. Cancellation also wins after the countdown has prepared
a request but before handoff. Early SELECT recovery, missing storage and startup
warning panels bypass automatic launching; a failed preflight shows its error
without restarting the countdown. Imported configuration reloads do not start
a new countdown. Settings changes take effect at the next normal startup.
The delay uses native frames, so blocking work can lengthen the visible wait.

## Fast navigation and session positions

Hold Y and press Up/Down in a browser to jump to a different initial-letter
group in the current sorted batch. Digits share one group; ASCII and Latin-1
case pairs share initials. This follows the displayed order, including custom
sort modes; it does not search unloaded batches. Release Y without a jump to
change the details view. L/R continues to switch tabs.

Eight recently visited folder positions (including their batch index) are kept
in session memory. Back restores the parent position and reselects the folder
by path when it is still present. Reopening a remembered folder restores its
position. Existing per-tab locations continue to persist across restarts;
the extra eight-folder history is not written to the SD card.
