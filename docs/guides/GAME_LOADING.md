# Game loading and nds-bootstrap

The release package includes official nds-bootstrap at
`/_nds/nds-bootstrap-release.nds`, plus its homebrew variant and version file.
Exact versions and source are identified in [the dependency lock](../../config/release-dependencies.json).
Gloam selects native homebrew, retail and DSiWare adapters from validated headers.

The adapter is experimental. Packaging the official backend does not establish
end-to-end compatibility; see [current support](../CURRENT_SUPPORT.md). Backend
first-run cache/preparation work may take longer than subsequent launches.
Gloam uses BlocksDS; upstream nds-bootstrap has its own devkitARM build system.
No private test patches are applied by the release builder.

In Advanced settings, START → Games & Apps → Game defaults opens global options. SELECT → Title
options opens overrides for a selected DS title. Up/Down selects, Left/Right
or A changes, Y resets and B/START saves and returns to the menu that opened
the editor. A failed save keeps the editor
open. Per-title `Use default` resolves through global defaults, then built-in
Auto values. Effective options are captured during launch preflight.

Options are mode, CPU/VRAM boosts, DMA/asynchronous reads, language, save slot,
save size and the nds-bootstrap in-game menu hotkey. These options apply to the
backend route, not arbitrary applications launched directly. Auto mode uses
the backend's default DSi-enabled policy; pure DS titles still need backend
compatibility. `QUIT_PATH` points to `sd:/BOOT.NDS`; actual in-game return is
not certified by generating this setting.

## Saves

Slot 0 recognizes adjacent `Title.sav`, `Title.nds.sav` or `saves/Title.sav`.
If more than one exists, preflight stops with an ambiguity message. No choice
is silently overwritten. Missing slot-0 saves default to `saves/Title.sav`.
Slots 1–9 use `saves/Title.sav1` through `.sav9`; they are independent files,
not copied over the base save. The editor never moves, deletes or resizes a save.

Auto creation uses the vendored game-code/save-memory table. Unknown games
require an explicit size, or an existing save. Existing nonempty saves are
reused when their size agrees with the configured/known size; mismatches stop
before handoff. New saves are filled with erased bytes through a separate
exclusive temporary file, then renamed only after the entire file is closed.
A failure stops the handoff and leaves any existing save unchanged. Sizes range
from no save through the database's 64 MiB maximum. This describes file
preparation; actual in-game saving and slot persistence depend on a successful backend launch.

## Shared configuration

Preflight is read-only. Commit happens after frontend preferences are saved,
immediately before platform handoff. The writer removes only the fields it
manages inside `[NDS-BOOTSTRAP]`, retains donor paths/other keys, comments and
other sections, and appends its effective values in that section. The pinned
backend's INI parser merges duplicate same-named sections. Input is bounded
at 64 KiB with 2 KiB lines; malformed section headers fail without replacing it.

The previous shared configuration is retained at
`_nds/dsi-frontend/bootstrap-previous.ini`; the next configuration is prepared
in `bootstrap-next.ini` and rename failure attempts restoration. A successful
backend handoff leaves the last launch configuration in place, as expected by
other frontends. No promise is made that their current launch selection is
retained. Other launchers are expected to write their own next request.

Active foreign `cheatData.bin` or `wideCheatData.bin` blocks this
route rather than applying an unrelated title's cheats or deleting another
launcher's payload. Our own previous payload is recognized by an exact private
snapshot and can be replaced. Revision-matched selection, exclusive groups and owned-payload preparation are supported; see CHEATS.md. Cheat effects and backend compatibility remain title-specific.

Per-title and global options are sparse, versioned records under
`_nds/dsi-frontend/games/<FNV64 identity>.ini`. `content=sd:/...` is verified
against the full normalized identity; global defaults use `content=defaults`.
Backup records recover interrupted/malformed current reads. See retained
third-party notices for the database's exact revision and GPL terms.


## DSiWare

DSi-only/DSiWare headers select the separate DSiWare adapter. It reads public
and private save sizes from the extended header, enforces DSi mode/CPU/VRAM
requirements and Auto save sizing, and retains the selected slot. Existing
adjacent `Title.pub`/`Title.prv`, `Title.nds.pub`/`.prv` and `saves/` files are
recognized independently. Ambiguous paths or mismatching sizes stop preflight.
No save is moved or resized. New files default to `saves/Title.pub` and `.prv`;
slots 1–9 use `.pub1`/`.prv1` through `.pub9`/`.prv9`.

New public/private saves are separate empty FAT12 volumes with initialized
reserved FAT entries, two FAT copies and bounded cluster geometry. Supported
new sizes are powers of two from 4 KiB through 32 MiB. An exclusive staging
file is fully written before rename. Existing correctly sized saves are reused
without rewriting their contents. A later preparation failure can leave a
newly created valid save available for retry; it never replaces an existing one.

The adapter obtains actual 8.3 names for every existing FAT path component via
BlocksDS `FAT_getShortNameFor`. It supplies `APP_PATH`, public `SAV_PATH` and
`PRV_PATH` to nds-bootstrap, with the full selected `NDS_PATH` retained. Aliases
must fit its 62-byte buffers. Excessive folder nesting fails with a clear
message instead of truncating a path. Optional banner-save and Slot-1-mode
fields are explicitly cleared/disabled in this route.

## Auto VRAM and damaged configuration

VRAM Auto removes `BOOST_VRAM` from the shared INI.
The v2.17.0 parser converts this field directly to bool, so -1 incorrectly
enabled boosting. Explicit Off/On still write 0/1, and the DSiWare adapter's
required On remains explicit. CPU/DMA/async settings retain their own backend
Auto semantics. The exact backend source revision is recorded in the dependency lock.

Commit validates the existing INI before C-string parsing. Binary control bytes,
read failures, malformed records and oversized input stop the operation without
replacing configuration or handing off. Ordinary UTF-8, tabs and CR/LF remain supported.

## Compatibility controls and recovery

| Capability | Current contract |
| --- | --- |
| Retail mode, boosts, DMA, asynchronous reads, language, save slots/sizing, hotkey | Explicit global/per-title editors; effective values are written for that launch. |
| Donor ROM paths | Existing backend donor fields are preserved. Gloam does not identify a suitable donor, extract its code or supply donor games. A configured path alone does not prove suitability. |
| Legacy homebrew RAM disks | Not currently selectable. The homebrew route explicitly clears `RAM_DRIVE_PATH` and `RAM_DRIVE_SIZE` to prevent stale cross-application reuse. Editing the shared INI is not a working override for this route. |
| SD DSiWare | Official nds-bootstrap route with header-derived public/private saves; no selectable alternate Unlaunch SD-DSiWare route. Installed NAND titles use native requests separately. |
| AP patches and title-specific backend behavior | The packaged backend provides its own built-in handling. Gloam does not expose a separate external AP-patch picker or promise compatibility from the frontend alone. |
| Return | The selected backend/application owns the return contract. `QUIT_PATH` is a request; it cannot fix a backend that does not reach its exit path. |

Missing retail/homebrew backends can be repaired with desktop **Setup → Update
Gloam + backends**, or by restoring the matching files from `sd.zip`. For a
missing emulator/player, use **Add-ons**, then check the file handler's path.
BIOS requirements vary by application: use the installer's BIOS column and
selected-app details. Gloam cannot determine every emulator's internal firmware
failure from a successful handoff.

For save ambiguity or size conflicts, back up the files first and choose the
intended save/slot and correct title options. Gloam never silently resizes or
replaces an existing save. For a launch that reaches a black screen, record the
route, backend version, boot environment and exact title revision; try defaults
before changing multiple options. Do not treat this as proof the save is bad.

Older homebrew requiring a RAM disk needs a separately verified launch route;
donor auto-configuration and alternate SD DSiWare routes remain explicit gaps,
not controls hidden in Simple settings. Flashcart/DLDI loader work is outside
this DSi-SD launcher's current scope.
