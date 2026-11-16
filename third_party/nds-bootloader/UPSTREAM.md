# Homebrew loader provenance

Loader: coderkei/nds-bootloader at 52fb19fc961a23643d7fe0601ff3d475aa21e121, from the AKMenu-Next submodule. Original linker script and LICENSE retained; the source has the marked compatibility backports below.

The reset patches below were authored by [RocketRobz](https://github.com/RocketRobz)
and retained in the Deletecat fork by [lifehackerhansol](https://github.com/lifehackerhansol).
[Epicpkmn11](https://github.com/Epicpkmn11) discovered the exception-handler issue.
The following Gloam commits adapt those patches; they do not originate the fixes.

| Gloam adaptation | Original commit in Deletecat/nds-bootloader |
| --- | --- |
| [`df8562c`](https://github.com/nextcode4u/gloam-shell-ndx/commit/df8562c): exception pointer | [`00adcb7`](https://github.com/Deletecat/nds-bootloader/commit/00adcb7f0ff4943049b440859e26a8e51e2f1a58) |
| [`82a7943`](https://github.com/nextcode4u/gloam-shell-ndx/commit/82a7943): sound capture | [`ba79fa3`](https://github.com/Deletecat/nds-bootloader/commit/ba79fa3592e56354ed68f9e6333d0720121eab73) |
| [`55a233a`](https://github.com/nextcode4u/gloam-shell-ndx/commit/55a233a): ARM7/ARM9 IPC | [`3a251ec`](https://github.com/Deletecat/nds-bootloader/commit/3a251ec562f376ec8c44e539a3c90b7e88b75de1), [`ba79fa3`](https://github.com/Deletecat/nds-bootloader/commit/ba79fa3592e56354ed68f9e6333d0720121eab73) |
| [`3bba447`](https://github.com/nextcode4u/gloam-shell-ndx/commit/3bba447): NDMA | [`d5171ee`](https://github.com/Deletecat/nds-bootloader/commit/d5171eeba7fb36370015ba8098656a1876ec25cc) |
| [`0ba7e61`](https://github.com/nextcode4u/gloam-shell-ndx/commit/0ba7e61): geometry interrupts | [`63d3130`](https://github.com/Deletecat/nds-bootloader/commit/63d313003c3ac607601894f345a63e441b91acd7) |

`source/boot.c` clears the previous ARM9 exception-handler pointer at
`0x02FFFD9C` before loading the next application. This backports
[RocketRobz's change, with discovery credited to Epicpkmn11](https://github.com/Deletecat/nds-bootloader/commit/00adcb7f0ff4943049b440859e26a8e51e2f1a58)
from Deletecat/nds-bootloader (reviewed at
`1aadf13bdc1f187351821d55ec53c8e984278eb5`), using a volatile store.
Otherwise a child application's exception can jump into the unloaded launcher's
overwritten code. Existing copyright/license notices and source formatting are retained.

`source/boot.c` also disables both sound-capture channels, then clears their
destination and length registers before clearing RAM. This backports the capture
reset from [RocketRobz's Memory Pit fix](https://github.com/Deletecat/nds-bootloader/commit/ba79fa3592e56354ed68f9e6333d0720121eab73),
preventing inherited capture activity from writing into the next application.

Both CPU reset routines clear IPC sync, empty their outgoing FIFO and disable
FIFO communication before handing over to the next application. These backport
RocketRobz's [ARM7 reset](https://github.com/Deletecat/nds-bootloader/commit/3a251ec562f376ec8c44e539a3c90b7e88b75de1)
and [ARM9 reset](https://github.com/Deletecat/nds-bootloader/commit/ba79fa3592e56354ed68f9e6333d0720121eab73),
preventing stale messages and interrupt enables from carrying over.

Both CPU reset routines clear all four DSi NDMA channels, based on
[RocketRobz's NDMA reset](https://github.com/Deletecat/nds-bootloader/commit/d5171eeba7fb36370015ba8098656a1876ec25cc).
The local adaptation uses the SDK register names and disables every channel
before clearing any source, destination, length, block, delay or fill registers.
This keeps inherited transfers from running while their configuration changes.

The ARM9 reset writes zero to `GFX_STATUS`, backporting
[RocketRobz's graphics-status reset](https://github.com/Deletecat/nds-bootloader/commit/63d313003c3ac607601894f345a63e441b91acd7)
to disable inherited geometry FIFO interrupt settings. This is not a complete
geometry FIFO flush. Gloam retains coderkei's 16-bit video-register clearing
loop that skips `VCOUNT`, together with the existing linker and return-stub layout.

The pinned coderkei base already contains two additional fixes:
- [coderkei's large-ARM9 argv fallback](https://github.com/coderkei/nds-bootloader/commit/52fb19fc961a23643d7fe0601ff3d475aa21e121).
- [Deletecat's VCOUNT-skip adaptation](https://github.com/coderkei/nds-bootloader/commit/1fd73c4d635ce4f949e95a4da255e5538d98fcb9),
  citing [RocketRobz's TWiLight Menu++ change](https://github.com/DS-Homebrew/TWiLightMenu/commit/38bb948144540ea2d48e6c79f3bd11ce2e0f9e78),
  which credits [Antonio Niño Díaz's BlocksDS fix](https://github.com/blocksds/libnds/commit/2beb4bb6048c44a31b5eb7d4386096fa003303ec).

Return stub: devkitPro/nds-hb-menu at 3c1f64c12da157fdd14a2d594db139f50e283261, bootstub/bootstub.s. Its source notice and COPYING are retained.

Local integration uses tools/build_homebrew_loader.py and arm9/source/launch_platform.c. The legacy loader is built with BlocksDS headers and links libnds7 for its now-external dmaSetParams function. No flashcart DLDI route is enabled by this integration. The wrapper bounds argument/VRAM/stub sizes and uses the DSi SD path only. This is a homebrew adapter, not retail-game compatibility.

The build defines upstream's `NO_DLDI` option. This removes the unused 32 KiB
flashcart patch reservation and keeps the complete return stub below
`0x02FFA000`. The larger build overlapped a current BlocksDS runtime area
and failed the return probe. The SD-only build passed the probe round trip.
The wrapper therefore enforces a 24 KiB combined stub/loader budget.
The `NO_DLDI` option itself needs no vendor source modifications.
