# Artwork preparation and caching

PNG/JPEG and embedded MP3 artwork still use the existing source-priority rules.
Icons remain separate. Drawing consumes prepared pixels and does no file I/O.

## RAM and disk responsibilities

Four prepared results, including decode failures, remain in RAM. A successful
image can also populate `/_nds/dsi-frontend/cache/artwork/`. Sixty-four fixed
`art-00.bin` through `art-3f.bin` slots hold thumbnails of at most 96×104 RGB555
pixels. Source-path hashing chooses a slot; collisions replace disposable
thumbnails, never artwork originals. The full source path and embedded/external
mode are checked when reading, so a hash collision cannot select another image.

Source inode/cluster, size and modification time must match. Every record has
a version, validated dimensions/lengths and an FNV-1a integrity checksum over
its header, path and pixels. Truncated, corrupt, stale or unknown-format records
fall back to the original decoder. A checksum detects accidental damage; it is
not a cryptographic signature.

The logical maximum per slot is 56 + 767 + 19,968 bytes. FAT allocation overhead
depends on cluster size. Temporary writes use the same fixed 64 `.tmp` names;
an interrupted temporary file is ignored and reused, so repeated interruptions
cannot create an unbounded collection of filenames. Replacement is disposable
cache maintenance, not a power-loss-safe transaction for user data. No startup
scan/index or full-library artwork pass is required.

The existing private `cache/` parent is supplied by normal staging. If it is
missing, read-only or full, artwork still decodes and uses RAM normally. Cache
writes are disabled in recovery and after detected SD loss.

## Refresh and clearing

Maintenance → Clear caches discards prepared RAM results and the known disk
slots/temporary names. Refresh metadata also clears disk thumbnails, so an edit
with unchanged size inside FAT's timestamp interval can be forced to reload.
Ordinary browsing, sorting and RAM eviction retain valid disk thumbnails.

Clearing preserves unknown files, source covers, embedded music files, explicit
assignments, application configurations and game saves. Failed optional cache
writes do not prevent displaying a successfully decoded cover.

## Record format

All integers and pixel words are little-endian. There is no serialized native
C structure or implicit padding.

| Offset | Meaning |
| --- | --- |
| 0–7 | `DFART001` |
| 8 / 16 / 24 | 64-bit source size / mtime / inode |
| 32 / 34 | 16-bit width / height |
| 36 / 40 / 44 | 32-bit embedded flag / path bytes / pixel bytes |
| 48 | 32-bit FNV-1a checksum, calculated with this field zero |
| 52–55 | Reserved zero bytes written by version 1 |
| 56 onward | Exact path bytes without a terminator, then RGB555 pixels |

Readers validate the full record and reject trailing bytes before exposing it.
The source label is supplied by normal artwork resolution, not taken from an
untrusted cache record. Maintenance exports decode and disk-hit counters.

## First-time decoding

The native frame loop now resumes a single cooperative decoder context.
Source reads are in 4 KiB chunks; switching occurs after a filesystem call
returns, never inside libc/FatFs. PNG/JPEG decoder loops and final thumbnail
scaling yield through optional checkpoints. Rendering consumes only complete
prepared images. The worker owns a fixed 32 KiB stack, one source path and one
output buffer, and never reads application/navigation state.

Selection changes invalidate the job and stop further source reads. Existing
read-only handles close during cleanup. Decoder cancellation follows existing
error cleanup paths. Cache clears, navigation and SD loss advance a generation
so old results cannot replace a new selection or write a stale disk thumbnail.
Recovery still bypasses artwork. Four RAM slots and 64 optional disk slots
remain unchanged. No background library scan or extra CPU thread is started.

Synchronous decoding remains available for tooling and explicit theme/bitmap
operations. The host context suite tests ownership, pixel equivalence and
invalidation separately from ASan, which does not fully support swapcontext.
The native artwork probe measures first slices/updates and cleanup on real
production objects. The native main loop supplies a periodic budget clock
(32 of 263 scanlines, about 2 ms), keeping hardware access outside the worker. These work-unit checkpoints are not a timer deadline:
individual allocations, file calls and metadata/cache operations can still
exceed a frame budget on a slow card. Physical timings remain unmeasured.

## Performance limits

Persistent reuse avoids repeated full-image decode. First-time decoding is
cooperative; file calls and allocations can still exceed a frame budget on a
slow card. Physical SD timing and transient memory peaks remain unmeasured.
