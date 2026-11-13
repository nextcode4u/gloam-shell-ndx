# Configuration files

The JSON files use `_nds/dsi-frontend/` because that SD directory was established
before the project had its Gloam name. We retained it after naming the project
to keep existing settings, themes, artwork, associations and loader paths working.
These are literal filesystem paths, not outdated project branding.

The later name, **Gloam**, is an older word for twilight—a play on the Nintendo
DSi's **TWL** codename. The name changed; the established SD directory stayed.

Keep the package catalog and dependency destinations aligned with the runtime's
SD layout. Changing just these JSON paths would break packaging or place files
where Gloam does not look for them. See [SD layout](../docs/guides/SD_LAYOUT.md)
and [building and packaging](../docs/development/BUILDING.md).

`addon-dependencies.json` records audited emulator/player releases, argv evidence,
BIOS requirements and distribution conditions. Only entries with an explicitly
pinned package and an eligible distribution status can enter the optional
add-on build. `download-only` entries retain their pins for the shared Python
and PowerShell installer, but their binaries and third-party source/notice
archives are excluded from our release companions. `permission_confirmed: false`
also prevents bundling. No absence-of-license assumption grants permission.

Default emulator/player executables live in `/_nds/dsi-frontend/plugins/apps/`.
Keep that application directory identical in the preset catalog and add-on lock
so generated associations resolve files installed by `addons.zip`.

Frontend settings use `/_nds/dsi-frontend/state.ini`, written at runtime rather
than shipped over existing preferences. `settings_mode=simple` or `advanced`
selects the UI; `simple_settings_lock=1` forces Simple mode and hides its mode
switch. The lock defaults to `0` and can only be changed in the config, not the
settings UI. See [setup and locking](../docs/GETTING_STARTED.md#simple-and-advanced-settings)
for defaults, upgrade behavior and editing instructions.

`background_updates=1` in the frontend's SD `state.ini` enables delayed release
checks; `0` disables them. This is a normal setting, separate from the Simple
settings lock. The disposable update cache is not configuration and is not
exported. The HTTPS trust bundle ships in the SD template as `update-ca.pem`;
keep it alongside the frontend's other support files.
