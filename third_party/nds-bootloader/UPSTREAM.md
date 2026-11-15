# Homebrew loader provenance

Loader: coderkei/nds-bootloader at 52fb19fc961a23643d7fe0601ff3d475aa21e121, from the AKMenu-Next submodule. Original linker script and LICENSE retained; the source has the marked compatibility backport below.

`source/boot.c` clears the previous ARM9 exception-handler pointer at
`0x02FFFD9C` before loading the next application. This backports
[RocketRobz's change, with discovery credited to Epicpkmn11](https://github.com/Deletecat/nds-bootloader/commit/00adcb7f0ff4943049b440859e26a8e51e2f1a58)
from Deletecat/nds-bootloader (reviewed at
`1aadf13bdc1f187351821d55ec53c8e984278eb5`), using a volatile store.
Otherwise a child application's exception can jump into the unloaded launcher's
overwritten code. Existing copyright/license notices and source formatting are retained.

Return stub: devkitPro/nds-hb-menu at 3c1f64c12da157fdd14a2d594db139f50e283261, bootstub/bootstub.s. Its source notice and COPYING are retained.

Local integration uses tools/build_homebrew_loader.py and arm9/source/launch_platform.c. The legacy loader is built with BlocksDS headers and links libnds7 for its now-external dmaSetParams function. No flashcart DLDI route is enabled by this integration. The wrapper bounds argument/VRAM/stub sizes and uses the DSi SD path only. This is a homebrew adapter, not retail-game compatibility.

The build defines upstream's `NO_DLDI` option. This removes the unused 32 KiB
flashcart patch reservation and keeps the complete return stub below
`0x02FFA000`. The larger build overlapped a current BlocksDS runtime area
and failed the return probe. The SD-only build passed the probe round trip.
The wrapper therefore enforces a 24 KiB combined stub/loader budget.
The `NO_DLDI` option itself needs no vendor source modifications.
