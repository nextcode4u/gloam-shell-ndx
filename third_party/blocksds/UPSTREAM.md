# Linked BlocksDS notices

Copied without modification from BlocksDS 1.25.0
`libs/libnds/licenses/LICENSE` and `LICENSE.FatFs`. These retain the SDK's
libnds and filesystem notices alongside the linked ROM. The existing ARM7
and modified SD driver notices remain in `arm7/COPYING` and
`third_party/libnds/LICENSE`. Toolchain provenance is in
`docs/development/TOOLCHAIN.md`. These dependency terms are separate from
the original Gloam code's GPL-3.0-or-later license.

The SDK CRT license texts are copied unchanged from `sys/crts/`. Covered CRT
source is available from https://codeberg.org/blocksds/sdk/src/commit/da93ad5cda6f3aea01769959b3ef7b3c5dd93673/sys/crts
and is an external SDK build dependency. Picolibc and GCC runtime notices below
come from the primary sources linked by the SDK license guide. They are full
notice collections; they do not identify every object in the locally built C
runtime or replace individual upstream notices. Downloaded on 2026-11-08.

- `picolibc-COPYING`: https://codeberg.org/WonderfulToolchain/wf-picolibc/src/commit/770d868896622d726129fa99168d5d20160d0f55/COPYING.picolibc; SHA-256 `552f9ae0e5717d3a0c28ff24db792971d1047d771992e40ea4d19b3d0042d78b`. This is the exact source revision identified by installed Wonderful package `1.8.13.r26367.770d86889-1`; the previous archived GitHub-mirror notice was superseded during the distribution audit.
- `gcc-COPYING.RUNTIME`: https://raw.githubusercontent.com/gcc-mirror/gcc/master/COPYING.RUNTIME; SHA-256 `9d6b43ce4d8de0c878bf16b54d8e7a10d9bd42b75178153e3af6a815bdc90f74`.

The optional Slot-1 helper Makefiles also list Maxmod and DSWifi. Full notices
from installed BlocksDS 1.25.0 are retained as `maxmod-COPYING` (ISC),
`dswifi-COPYING` (MIT), `dswifi-COPYING.lwip` (BSD-3-Clause), and
`dswifi-COPYING.mbedtls` (Apache-2.0 OR GPL-2.0-or-later). Sources:
https://codeberg.org/blocksds/maxmod/src/commit/8f7e2faa7db4ef5f7824af3aa601188595a5ebc3
and https://codeberg.org/blocksds/dswifi/src/commit/84d91e8c517bd7d15d38aff039640beaf359c230.
All four retained notice files match those SDK-selected source revisions byte-for-byte.
Use the actual helper build's SDK/submodule revisions for corresponding-source
publication. A link-list entry alone does not establish that every library
object was linked. Gloam's current custom ARM7 build does not define
USE_MAXMOD/USE_DSWIFI and links only libnds7/libc; do not describe these optional
helper libraries as active Gloam services.

BlocksDS SDK v1.25.0 resolves to commit
`da93ad5cda6f3aea01769959b3ef7b3c5dd93673` (verified against the public tag
during this audit). Its submodule selections, rather than current library
branches, identify the SDK dependency versions used by a pinned build.
