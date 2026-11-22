# Library search

Search uses an optional index prepared on your PC. Gloam never scans the SD card
for this index at startup. Missing or invalid indexes leave ordinary browsing
available, including installations updated with only `BOOT.NDS`.

## Prepare the index

In Windows SD Setup, select the SD card once, open **Library**, then choose
**Build / refresh search index**. The worker scans Games, Apps, Media and valid
custom folder-tab roots from `tabs.ini`; nested/overlapping roots are indexed
once. Missing roots are reported. The operation reads names and filesystem
metadata, never ROM contents or ROM hashes. Progress appears in Activity and the
Cancel button remains available.

Save/BIOS folders, linked entries, known save types, README setup notes and conventional
cover sidecars are omitted. Unicode filenames are retained within the path limits;
oversized paths/names are included in the reported skipped count. This is a file index,
not a game database or a list of installed NAND titles. Multi-file application
collections keep their own in-app browsers. Search honors the console's
recognized-file, hidden/system and save/BIOS-folder filters. Folder hiding applies
below each assigned tab root, so an explicitly assigned root remains searchable.
The recognized-file rule is shared
with folder browsing: familiar document/media types remain visible even before
you install a matching app. Launching still requires an available handler.

The completed file is installed at
`/_nds/dsi-frontend/cache/library.gli`. Preparation happens on the PC; a failed
or cancelled scan keeps the installed index. Installation uses a same-directory
temporary and one rotating `library.gli.bak` recovery copy. User content, saves,
settings and theme files are not changed. This recoverable rename sequence is not
a guarantee against power loss during filesystem writes.

The status shows the index date and file count. Refresh after copying, renaming
or organizing content, or changing library roots. The tool does not depend on
Windows Search or automatically start a large scan after every file copy.
Close Gloam before editing its card on the PC, then restart it after reinsertion.

## Search on the console

Open **SELECT → Search library**, press **X** in the SELECT menu, or choose it
in Quick controls. Games, Apps,
Media and custom tabs initially search their assigned library; Home, Files and
System initially search all configured libraries.

- Type with touch, or move around the keyboard with the D-pad and press A.
- The bottom keyboard row shows Space, Backspace and Search as symbols.
- Select **Search**, or press X while editing. An empty query lists indexed files.
- Words match the indexed path in any order. ASCII, Latin-1, basic Greek and
  Cyrillic uppercase/lowercase pairs are folded; other Unicode text matches
  exactly. The initial on-console keyboard contains Latin letters, digits,
  spaces and basic punctuation, without a Japanese IME.
- Up/down selects results; L/R or left/right pages through matches. Touch the
  page controls for the same action. Only 16 matches are retained at a time.
- SELECT or the **All/Tab** button changes scope; X returns to editing.
- A or a second tap launches through the existing preflight/backend route. The
  shared-extension chooser and launch errors return to results when cancelled.
  Y locates the selected file in its folder for title options or browsing; it can
  continue cooperatively through later 512-entry folder batches. Y erases a
  character while editing.
- B, START or the bottom back area closes search, including while a query runs,
  and returns to the menu that opened it.

The PC also prepares a filter for each 64-file block. Search words of three or
more characters let Gloam skip blocks that cannot match, without reading their
file records. Short words remain valid but can require more index reads. This
uses no Windows Search database and never reads ROM contents on the console.

Queries run in bounded slices: at most 64 record/block work units, a 4 KiB file-
record budget and 16 KiB filter-table budget, with a bounded single-unit overrun
and a cooperative CPU-time checkpoint. Only one result page and query state are
allocated (under 14 KiB); the full index stays on SD. Previous pages can rescan
earlier filters/records and remain cancellable. The query limit is 63 UTF-8 bytes.
Missing/moved files are reported instead of launched. Newly added files require
an index refresh. Header, block and record checks are incremental, not a full
integrity scan when opening search.

## Index format

`GLIDX002` uses little-endian integers and UTF-8 paths. Maximum size is 64 MiB,
with at most 1,000,000 files. These bounds cover search, not the normal browser.

The 32-byte header contains magic (8), record count (u32), file-record payload
size (u32), UTC Unix timestamp (u64), block count (u32), and FNV-1a-32 of the first
28 bytes. The block count is `ceil(record_count / 64)`.

A contiguous block directory follows the header, before the file records. Each
276-byte entry contains its absolute file offset (u32), first record number
(u32), count (u32), file-record byte size (u32), a 256-byte filter, and FNV-1a-32
of the preceding 272 bytes. Entries describe consecutive groups of up to 64
records; offsets/counts/lengths are validated before using them.

Filters contain folded Unicode codepoint trigrams from each indexed path. For
codepoints `a,b,c`, start with `h=2166136261`, then apply `h=(h^cp)*16777619` for
each codepoint using unsigned 32-bit arithmetic. Set bits `h & 2047` and
`(h >> 11) & 2047`, least-significant bit first in each byte. Folding matches the
native search's documented ranges. Queries require the union of within-word
trigrams (plus the current scope's path trigrams). A missing required bit safely
excludes a block. False positives cause ordinary record checks; filters do not
select a result by themselves.

Each file record contains path byte length (u16), hidden/system flags (u16, bits
0/1), FNV-1a-32 of the two flag bytes followed by the path bytes (u32), then the
path. Paths are SD-relative without a terminator. Traversal, device prefixes,
embedded NUL/control bytes and invalid UTF-8 are rejected before exposing results.

The original unfiltered `GLIDX001` layout is also accepted: reserved zero at
header offset 24 and records immediately after the header, without a directory.
It is scanned cooperatively. New PC builds produce version 2.

The reader/writer contract contains names and paths, never game contents,
firmware, NAND data or keys.
