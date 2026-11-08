# Gloam DSi Menu forwarder

The optional Gloam Shell NDX tile returns from the DSi Menu to `sd:/BOOT.NDS`.
A system app can therefore exit to the DSi Menu, where Gloam is one selection
away. It does not intercept the system app's HOME/power controls or alter its
exit behavior. Installed DSiWare discovery inside Gloam is a separate feature.

## Files and installation

Development SD packages include:

- `_nds/dsi-frontend/install/gloam-forwarder.dsi`: the 114 KiB DSi-only tile.
- `_nds/dsi-frontend/install/gloam-forwarder.tmd`: matching unsigned metadata.
- `BOOT.NDS`: the actual SD launcher, updated independently of the tile.

Use [NTM](https://github.com/Epicpkmn11/NTM) or an equivalent established title
manager to install the `.dsi`. NTM supports homebrew installation and metadata
generation. Select the intended system NAND or hiyaCFW SDNAND according to your
existing setup. Keep your existing NAND backup. This build uses title ID
`00030004-474c4458` (`GLDX`); do not overwrite an unrelated title with that ID.
The forwarder and metadata are unsigned: system-NAND installation requires
Unlaunch's launcher patches enabled. Stock Menu/Memory Pit alone is insufficient.
SDNAND variants and physical installations require their own validation.

There is no auto-installer and Gloam does not write NAND to add the tile. The
standalone tile only reads SD, validates the target and writes a RAM autoload
request. Unlaunch consumes it at reboot and opens `sdmc:/BOOT.NDS`. It does not
change Unlaunch's saved boot choices or bundle its executable.

Keep Gloam at root `BOOT.NDS`. The tile follows that path, so replacing it with
another launcher changes the destination. A missing/truncated target or a copy
of the forwarder at that path produces a retry/menu screen rather than an
intentional reboot loop. A retries; B requests the DSi Menu. A usable modified
boot environment is still required to honor the request.

## Build and ownership

`make` builds both the launcher and optional forwarder. For just the tile:

```sh
python3 tools/build_forwarder.py
```

Outputs are in `dist/forwarder/`. The tool creates a single-content unsigned TMD
with the actual title ID, size and SHA-1; SHA-256 manifests tie packaged files
together. No Nintendo binary, signed metadata, console key or ticket is bundled.
The original crescent/doorway icon is authored for Gloam. Source lives under
`forwarder/arm9/`; the serializer is shared with `system_routes.c`, and the same
pinned BlocksDS ARM7/storage compatibility build is reused. Installer software
remains external; no NTM source or binaries are included.

## Compatibility

The tile requires an appropriate modified boot environment; it is not a stock
unsigned-title loader. Physical DSi and SDNAND installation remain unverified.
It does not install Unlaunch or change an application's own return protocol.
See [current support](../CURRENT_SUPPORT.md) and the separate
[installed NAND DSiWare library](INSTALLED_DSIWARE.md).
