# Homebrew loader provenance

Loader: coderkei/nds-bootloader at 52fb19fc961a23643d7fe0601ff3d475aa21e121, from the AKMenu-Next submodule. Original source, linker script and LICENSE retained without formatting changes.

Return stub: devkitPro/nds-hb-menu at 3c1f64c12da157fdd14a2d594db139f50e283261, bootstub/bootstub.s. Its source notice and COPYING are retained.

Local integration uses tools/build_homebrew_loader.py and arm9/source/launch_platform.c. The legacy loader is built with BlocksDS headers and links libnds7 for its now-external dmaSetParams function. No flashcart DLDI route is enabled by this integration. The wrapper bounds argument/VRAM/stub sizes and uses the DSi SD path only. This is a homebrew adapter, not retail-game compatibility.

The build defines upstream's `NO_DLDI` option. This removes the unused 32 KiB
flashcart patch reservation and keeps the complete return stub below
`0x02FFA000`. The larger build overlapped a current BlocksDS runtime area
and failed the return probe. The SD-only build passed the probe round trip.
The wrapper therefore enforces a 24 KiB combined stub/loader budget.
No vendor source modifications are needed for this option.
