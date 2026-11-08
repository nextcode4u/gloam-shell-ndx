# Optional DSi Menu return tile

`gloam-forwarder.dsi` is the small Gloam Shell NDX shortcut. Install it with your
established DSi title manager, such as NTM, to add Gloam to the DSi Menu. Its
matching unsigned `gloam-forwarder.tmd` is supplied for managers requiring one.

Requires Unlaunch with launcher patches enabled. This is unsigned homebrew and
will not install/run on an unmodified DSi Menu or with Memory Pit alone. This
folder does not contain an automatic NAND installer. Installation is optional.

Keep the full launcher at `sd:/BOOT.NDS`. Updates replace that SD file; the tile
normally stays installed. Do not put the forwarder itself at `BOOT.NDS`.

Expected return path: system app → DSi Menu → Gloam tile → SD launcher. This
provides a menu shortcut; it does not add an exit hotkey to Nintendo's apps.

Setup: see docs/guides/GLOAM_FORWARDER.md in the source or SD package. Physical
DSi and SDNAND validation remain pending.
