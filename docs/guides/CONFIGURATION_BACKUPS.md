# Portable configuration backups

In Advanced settings, START → Maintenance → Export configuration saves
`_nds/dsi-frontend/configuration/export.dsconfig`. Its previous revision is
retained as `.bak`. Import configuration opens an SD file picker. A first
validates the entire selected archive and shows the checked record count;
A again confirms the import. Selecting another file or folder clears that
confirmation. START cancels. Successful imports reload the frontend immediately,
including its theme/audio, without a later autosave overwriting imported state.
Recovery sessions cannot use these ordinary appearance/configuration controls.

The native format is `DSCFG001`: a little-endian record count followed by
bounded name-length/flags/size/CRC32 headers, ASCII paths and uncompressed
record bytes. Reserved flags must be zero. There is no executable payload or
compression. CRC32 detects corruption; this is not an authenticated format.
The native importer accepts `.dsconfig`, not ZIP files.

The computer tool reads/writes the same native format and also retains its ZIP
format with a versioned JSON manifest and per-record SHA-256:

```sh
python3 tools/configuration.py export --sd /path/to/sd --archive frontend-config.dsconfig
python3 tools/configuration.py import --sd /path/to/sd --archive frontend-config.dsconfig
python3 tools/configuration.py export --sd /path/to/sd --archive frontend-config.zip
python3 tools/configuration.py recover --sd /path/to/sd
```

Close the launcher/emulator before changing its SD contents from a computer.
The portable whitelist includes frontend state, folder tabs, dashboard/history,
three appearance slots, per-content preferences, game options/cheat selections
and personal extension overrides. Theme packages, music, artwork, applications,
databases, Nintendo files, BIOS files, backend configurations and saves are
excluded. Copy referenced assets/applications separately at the same SD paths.

Both importers validate the whole archive before replacing records. Unknown
paths, traversal, duplicate names, non-text/oversized records, unsupported flags
and failed checksums stop the operation. Limits are 1,024 records, 64 KiB per
record and 4 MiB total record data. Imports merge listed records; extra existing
records are retained. Individual settings modules still apply their ordinary
value/path/version checks when reloading these files.

## Native rollback and recovery

Before native replacement, `configuration/rollback.dsconfig` captures the
current portable records. A checked independent copy of the incoming archive
is published as `configuration/pending.dsconfig` before the first replacement.
Success removes that pending marker; the rollback archive remains available
in the same import picker. Importing that rollback archive first copies it
independently before making a new backup.

Detected write failures attempt rollback. At the next startup an unfinished
pending transaction is also rolled back before preferences load. Recovery first
checks every affected destination against its old/imported checksum and refuses
to overwrite a subsequent external edit. New records absent from the backup
are removed only within the transaction's declared portable scope. Files outside
that scope, including saves, are untouched.

Exclusive staging files avoid overwriting an unexpected file or redirected
host test path. A preserved interrupted `.config-tmp` file or external edit can
require manual recovery. Keep the pending archive, rollback archive and original
SD backup; a recovery failure disables normal frontend storage actions instead
of loading a mixture and writing over it. These mechanisms do not guarantee
atomicity across FAT/device-level power loss.

## Computer-side rollback

Computer imports retain originals plus a SHA-256 transaction journal under
`configuration-backups/<timestamp>-<unique suffix>/`. Detected failures roll
back; `recover` handles incomplete computer journals while preserving external
edits. Completed backups remain available without automatic pruning. The
computer `recover` command does not interpret a native pending transaction.

## Reset and maintenance scopes

Appearance reset, browser-option reset, clear recent history and clear prepared
memory caches have separate controls. Browser reset changes sorting, visibility,
extension/title labels, banner language and scroll speed while retaining tabs,
favorites, navigation and appearance. Appearance reset preserves packages,
presets and source images. Individual cover/application choices have separate
Clear actions. START → Maintenance → Reset configuration archives all frontend preferences,
tabs, pins/history, appearance presets, game/cheat/item overrides and personal
associations. A second A confirms; moving to another row/page or B cancels.
Recovery sessions and missing/lost storage cannot reset. Successful resets reload
the launcher immediately. Themes, music, artwork, loaders, BIOS files, game saves,
configuration exports and diagnostic records remain in place. This is a settings
reset; it does not erase installed assets or libraries.

Each reset moves original records into a new `configuration/reset-backup-NNN/`
(000–999), never overwriting an earlier reset. Backups include fallback `.bak`
records, so old preferences cannot silently reappear. A valid minimal state file
prevents re-importing the first prototype's legacy preferences. Backup entries
use these fixed names: `00` state.ini, `01` state.bak, `02` tabs.ini, `03` tabs.bak,
`04` dashboard.ini, `05` dashboard.bak, `06` recent.ini, `07` recent.bak,
`08` appearance-1.ini, `09` appearance-1.bak, `10` appearance-2.ini,
`11` appearance-2.bak, `12` appearance-3.ini, `13` appearance-3.bak,
`14` games/, `15` items/, `16` plugins/user/. Restore while the launcher/emulator
is stopped, retaining a copy of current settings before replacing any destinations.
Ordinary exported `.dsconfig` archives can also be imported after a reset.

Before completion, moves stay under `configuration/reset-pending/`. Startup
rolls these back before reading preferences. Conflicting external edits stop
recovery and disable ordinary storage actions; the backup is retained. This
uses bounded same-volume renames, not a promise of FAT-level power-loss atomicity.

Export diagnostics writes `diagnostics.txt` with observed frontend/build state.
It excludes content paths, file contents, BIOS/NAND data and console keys.

Games & Apps and Maintenance are Advanced settings pages. Switch from Simple
using the settings mode control; a configured Simple settings lock prevents
that switch until its owner changes the configuration. SELECT actions remain
available according to their own content and storage requirements.
