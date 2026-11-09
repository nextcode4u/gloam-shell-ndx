# Simple DS Slot-1 Launcher

Author: **Lorenzooone**, Copyright (C) 2024. **GPL-3.0-or-later** according to
its source headers; the complete GPL text is in COPYING.

Upstream: https://github.com/Lorenzooone/Simple-DS-Slot-1-Launcher
Release: 1.4.1.6, source `c372cf061dad8f52ef13f1bce9beaa24c8c40253`.

Upstream credits TWiLight Menu++'s slot1launch, Apache Thunder's NTR Launcher
and Michael “Chishm” Chisholm's NitroHax. Inherited notices remain intact in
the matching source archive. Gloam invokes this separate helper rather than
claiming its cartridge-boot implementation as original frontend code.

`tools/build_release.py` downloads the official binary with pinned hashes and
installs only slot1launch.nds. Full attribution/GPL text accompanies it as
`_nds/dsi-frontend/loaders/slot1launch.txt` inside `licenses.zip`. Matching complete upstream source,
including its build instructions, ships under `dependencies/` in `source.zip`,
beside `sd.zip`. The dependency lock records release and source identities.
