# tuna-viDS source and component notes

The shared downloader fetches the unchanged [DS-Homebrew v1.2-clut release](https://github.com/DS-Homebrew/tuna-vids/releases/tag/v1.2-clut)
directly from upstream to `/_nds/dsi-frontend/plugins/apps/tuna-vids.nds`.
Gloam does not bundle the binary or third-party source/notice archives while
component obligations remain unresolved. Credit Michael “Chishm” Chisholm and
the fork contributors; Gloam's GPL does not replace their component terms.

The release consumes the video path from argv. Optional screen filters use
`/_nds/colorLut/currentSetting.txt` and its named `.lut` file; filter assets are
not required or supplied. Later branch-only touch/build fixes are not claimed
present in the released binary.

The [pinned source](https://github.com/DS-Homebrew/tuna-vids/tree/6781a1a7ac723cb1274de6bd8734b21a9fbbebc5)
includes player and codecs. The upstream README describes GPL v2 ARM9 and
GPL v2/RPSL ARM7 code. Preserve original GPL, Xvid and Helix notices when using
that source. RCSL is an alternative requiring an applicable valid agreement,
not an automatic replacement for the RPSL.

[Helix RPSL section 2.1(e)](https://github.com/DS-Homebrew/tuna-vids/blob/6781a1a7ac723cb1274de6bd8734b21a9fbbebc5/arm7/source/helix/RPSL.txt)
calls for an executable source-availability notice and the Exhibit A object-code
notice where copyright notices appear. That notice was not found in the stock
binary/startup source. GPL/RPSL compatibility also remains recorded rather than
declared resolved. These are the reasons this app is download-only; documentation
or direct downloading is not asserted to settle those questions.
