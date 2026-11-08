# ROM save-memory table

Source: DS-Homebrew/TWiLightMenu, universal/include/ROMList.h
Revision: 22979a2b800f1d8a598d1a1778ee6d7ea079d5fe
Copyright 2016–2021 Arisotura; GPL-3.0-or-later. Full license: COPYING.

Local change: the table declaration is `static const` for a private, read-only C
wrapper. Entries and notices are unchanged. The wrapper supplies the original
`u32`/`ROMListEntry` type names. Save-type byte sizes follow the same revision's
universal/include/saveMap.h. No game or save data is included.
