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

`MELONDS_SD_COMPAT=0` is the hardware/release default and selects upstream SD
write commands. `make MELONDS_SD_COMPAT=1` explicitly enables historical
emulator workarounds for local diagnosis only. Switching
between `0` and `1` invalidates both custom ARM7 objects automatically, including
switching back without cleaning. CI builds both modes and restores `0` before packaging.
Review the source/API and repeat read/write checks before carrying this override
to a different SDK. This is an application dependency, not an emulator build.

## Native directory metadata ABI

`third_party/fatfs-headers` contains unmodified headers matching the FatFs in
BlocksDS 1.25.0. `directory_scan.c` uses the SDK's `libnds_f_*` directory symbols
to collect names, attributes, sizes and timestamps without per-entry pathname
searches. No separate filesystem implementation is built. This optimization
is version-gated to SDK 1.25.0; other SDKs fall back to POSIX. Review the exact
FatFs revision, configuration and symbol namespace when updating the SDK.

## HTTPS library

The frontend builds Mbed TLS 3.6.8 locally, independently of the SDK's optional
`blocksds-mbedtls` package. `make` runs `tools/build_mbedtls.py`, which verifies
its pinned archive, applies the retained BlocksDS port patch and caches ARM9
libraries under `build/mbedtls`. Compiler/SDK identity, headers and library hashes
control reuse. No system package is replaced.

Pins, license and port attribution are under `third_party/mbedtls`. Release source
packages include the exact upstream archive; license packages include its notice.
Run `python3 tools/check_tls_transport.py --offline` after populating the source
cache. It uses authored loopback DNS/TLS servers and the actual transport with
sanitizers; no external requests are made by the client. It checks trust/hostname
failures, success, malformed responses, cancellation and deadlines. DSi Wi-Fi,
entropy and timing still need hardware checks.
