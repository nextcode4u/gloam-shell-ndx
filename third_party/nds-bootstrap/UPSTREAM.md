# nds-bootstrap

Source: https://github.com/DS-Homebrew/nds-bootstrap
Release: v2.17.0, commit `b9044ff2e66d5a522f9678c185f421a603f97d7a`.

Upstream uses GPLv3 with GPL-3.0-or-later notices in its main loader/cardengine
sources; inherited GPL-2.0-or-later and permissive file notices remain applicable.
Preserve each notice, not only the root license. LICENSE and UPSTREAM-README.md
are unmodified exports from this pin. COPYING.libeasysave retains Jonathan
Archer's MIT notice. The upstream README credits the authors and contributors.

The release builder downloads official binaries, verifies the lock's archive and
member hashes, and installs the retail/homebrew NDS executables and version file.
It applies no backend patches. Normal development and compatibility validation
use these same unmodified official binaries. Patched backend experiments are
not a substitute for correcting emulator behavior and are not part of the
supported test baseline. Matching complete upstream source and its build
instructions are included under `dependencies/` in `source.zip`, distributed
beside the SD archive. Backend source requires its upstream devkitARM
environment; it is separate from Gloam's BlocksDS build.

GPLv3 section 6(d) permits this distribution with equivalent access to matching
Corresponding Source. Publish the source download beside the binary download,
preserve original notices, and identify any future modifications accurately.
The dependency lock and package manifest record exact source/binary identities.
Bundling establishes availability, not application compatibility; see
[the support limits](../../docs/CURRENT_SUPPORT.md).
