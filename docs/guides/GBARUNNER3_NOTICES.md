# GBARunner3 distribution and source notes

Gloam's optional add-ons include Gericom's GBARunner3 core from the supplied
[Deletecat hicode archive](https://files.deletecat.com/GBARunner3-hicode.zip).
The separate gbar3-frontend browser is not included. The core and all game
configs are unchanged archive members, pinned by SHA-256 in the
[dependency lock](https://github.com/nextcode4u/gloam-shell-ndx/blob/main/config/addon-dependencies.json).

## Runtime files

- Core: `/_nds/dsi-frontend/plugins/apps/GBARunner3.nds`.
- Game-specific configuration: 304 JSON files under `/_gba/configs/`.
- User-supplied firmware: original 16 KiB GBA BIOS at `/_gba/bios.bin`.

Gloam's `.gba` association passes the selected ROM directly to this core.
Static disassembly confirms the supplied binary selects `argv[1]` when an
argument is present. This does not establish launch/save compatibility for
every ROM or DSi boot environment. No BIOS or games are supplied; compatibility
with a replacement BIOS is not established. Preserve customized game configs
when merging an updated overlay, since supplied files can replace the same paths.

## Attribution and distribution status

Credit Gericom and the GBARunner3 contributors. The source includes additional
component notices, including the ARM7 MMC driver and mini-printf; retain those
notices with redistributed source. The reference source's original notices and
pinned libtwl submodule notices accompany this package in `licenses.zip`.

The project includes this third-party build under its documented assumption
for unconfirmed redistribution terms. No project-wide upstream license grant
has been established. Gloam's GPL does not license or relicense GBARunner3.

## Reference source, not verified corresponding source

The supplied ZIP has no source revision/build manifest. Its filename identifies
it as a hicode build but does not identify the exact commit, build flags or
additional patches. The dependency manifest therefore records
`binary_source_commit: null` and `source_role: reference-only`.

The source companion contains
`dependencies/addons/gbarunner3/reference-source.tar.gz`, from Gericom/GBARunner3
commit `ecaa817815d9761745606592f04affe5ee9c3731`, plus its pinned reference
submodule archives under `dependencies/addons/gbarunner3/submodules/`.
These are available reference sources, not a claim that rebuilding them
reproduces the distributed hicode binary. The core/config download hashes,
reference source URLs and submodule pins are in the accompanying `release.json`.

- [Upstream project](https://github.com/Gericom/GBARunner3)
- [Audited reference revision](https://github.com/Gericom/GBARunner3/tree/ecaa817815d9761745606592f04affe5ee9c3731)
- [Upstream hicode branch](https://github.com/Gericom/GBARunner3/tree/feature/cache-hicode)

If exact corresponding source is identified later, update the source role and
pins explicitly. Do not relabel this third-party download as an official release.
