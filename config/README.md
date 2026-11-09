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
reviewed package and permitted terms can enter the optional add-on build.
