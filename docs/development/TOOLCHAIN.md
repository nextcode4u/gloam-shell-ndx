# Toolchain and SDK integration

The minimum native SDK is BlocksDS 1.25.0. Recorded build packages are listed in
`toolchain-packages.txt`; the default paths are `/opt/wonderful` and
`/opt/blocksds/core`. The Makefile accepts `BLOCKSDS` and ARM tool path overrides.
For installation follow [BlocksDS setup](https://blocksds.skylyrac.net/docs/setup/).

After an SDK update, clean-build, run host checks and review both version-pinned
integrations below. Successful compilation does not establish hardware compatibility.

## SD driver override

`third_party/libnds/sdmmc_compat.twl.c` and its internal header match BlocksDS
libnds revision `e478cc660a8c7254fbffc54bda3ce0b93ab14c15` (SDK 1.25.0).
Local changes select multi-block SD writes for the compatibility option and
initialize internal NAND block length with CMD16. DMA and NAND write command
selection remain upstream. The modified source retains its Zlib license.

`MELONDS_SD_COMPAT=1` enables the historical controller workaround by default;
`make clean && make MELONDS_SD_COMPAT=0` selects upstream SD write commands.
Review the source/API and repeat read/write checks before carrying this override
to a different SDK. This is an application dependency, not an emulator build.

## Native directory metadata ABI

`third_party/fatfs-headers` contains unmodified headers matching the FatFs in
BlocksDS 1.25.0. `directory_scan.c` uses the SDK's `libnds_f_*` directory symbols
to collect names, attributes, sizes and timestamps without per-entry pathname
searches. No separate filesystem implementation is built. This optimization
is version-gated to SDK 1.25.0; other SDKs fall back to POSIX. Review the exact
FatFs revision, configuration and symbol namespace when updating the SDK.
