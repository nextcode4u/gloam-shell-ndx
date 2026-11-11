# Third-party notices

## Source and license inventory

These links identify upstream sources; local files and
per-component notes below identify the actual retained versions/modifications.
Licenses apply per component, not simply according to Gloam's root LICENSE.

| Component | Source/version | License and distribution status |
| --- | --- | --- |
| Native homebrew loader | [coderkei/nds-bootloader, 52fb19fc](https://github.com/coderkei/nds-bootloader/tree/52fb19fc961a23643d7fe0601ff3d475aa21e121) | GPLv3 project; inherited GPL-2.0-or-later files retained. Required source/notices are in third_party/nds-bootloader. |
| Return bootstub | [devkitPro/nds-hb-menu, 3c1f64c1](https://github.com/devkitPro/nds-hb-menu/tree/3c1f64c12da157fdd14a2d594db139f50e283261) | Dave Murphy; GPL-2.0-or-later, compatible with distribution under GPLv3. Source/COPYING retained with loader. |
| Slot-1 helper | [Lorenzooone/Simple-DS-Slot-1-Launcher, c372cf06](https://github.com/Lorenzooone/Simple-DS-Slot-1-Launcher/tree/c372cf061dad8f52ef13f1bce9beaa24c8c40253) | GPL-3.0-or-later; separate official executable bundled by the SD release builder. See third_party/slot1launcher. |
| nds-bootstrap | [DS-Homebrew/nds-bootstrap, b9044ff2](https://github.com/DS-Homebrew/nds-bootstrap/tree/b9044ff2e66d5a522f9678c185f421a603f97d7a) | GPLv3 project with GPL-3.0-or-later main-source notices and inherited licenses; official release bundled by the SD release builder. See third_party/nds-bootstrap. |
| BlocksDS / libnds / SD driver | [BlocksDS SDK](https://codeberg.org/blocksds/sdk), [libnds e478cc66](https://github.com/blocksds/libnds/tree/e478cc660a8c7254fbffc54bda3ce0b93ab14c15) | Zlib-style notices plus the linked FatFs terms; local driver changes marked. SDK build is 1.25.0. |
| CRT / C and compiler runtimes | Sources/pins in third_party/blocksds/UPSTREAM.md | CRT Zlib/MPL-2.0; picolibc per-file BSD/permissive collection; GCC runtime GPLv3 with Runtime Library Exception. Full notices retained. |
| Optional helper SDK libraries | [Maxmod](https://codeberg.org/blocksds/maxmod), [DSWifi](https://codeberg.org/blocksds/dswifi) | ISC; MIT with lwIP BSD and Mbed TLS Apache-2.0 OR GPL-2.0-or-later components. Full installed-SDK notices retained; verify actual helper link inputs at release. |
| PNG decoder | [LodePNG](https://github.com/lvandeve/lodepng), 20230410 | Zlib; locally modified cooperative decoding. Source and standalone LICENSE retained. |
| JPEG decoder | [nothings/stb, 4e9b368b](https://github.com/nothings/stb/tree/4e9b368bfc071e97206ea5c6adba68dd0cbf8897) | MIT OR Unlicense; locally modified cooperative decoding. Both notices retained in header and LICENSE. |
| ROM save table | [TWiLightMenu, 22979a2b](https://github.com/DS-Homebrew/TWiLightMenu/tree/22979a2b800f1d8a598d1a1778ee6d7ea079d5fe) | GPL-3.0-or-later; table/source and COPYING retained. |
| Cheat-format reference | [AKMenu Next, c884c092](https://github.com/coderkei/akmenu-next/tree/c884c092e1bf1b172f9a2464f74c13ce53124400) | GPL-3.0-or-later reference; retained attribution below. No third-party cheat database is bundled. |
| Packed/built-in fonts | [DejaVu fonts](https://dejavu-fonts.github.io/) | Bitstream Vera font terms / public-domain DejaVu changes; full notice below and alongside theme fonts. |
| Copper music | [Sentinel by pixelpion](https://pixabay.com/music/video-games-sentinel-455122/) | Pixabay Content License, integrated theme music; not GPL or CC0. Credits/terms/conversion hashes retained. |
| Optional SameBoot GBC firmware | [SameBoy v1.0.5, ea667ad7](https://github.com/LIJI32/SameBoy/tree/ea667ad79aa28409f034b1fd6da7304e1844750d) | MIT/Expat; unmodified cgb_boot.bin staged as roms/bios/gbc_bios.bin by the optional GameYob package. Source and notices accompany it. |
| Original theme art and synthesized cues | Authored tools/assets in this tree | Per-theme CC0 notices; external fonts and Copper music retain their own terms. |

The optional Gloam DSi Menu tile is authored source under the project's license;
its linked SDK/runtime dependencies retain the notices above. Optional emulators/players and their free replacement firmware are distributed
separately in the add-on package. Games, proprietary external BIOS files and
vendor reference material are not part of the distributed frontend.


## BlocksDS

The ROM links against the locally installed BlocksDS SDK/libnds and uses a modified minimal ARM7 core derived from the BlocksDS source. Its license is in `arm7/COPYING`. Linked libnds and FatFs notices are retained
in `third_party/blocksds/`, together with CRT, picolibc and GCC runtime notices
and upstream source references. The modified SD driver retains its separate notice.

## LodePNG

`third_party/lodepng/lodepng.c` and `.h` are based on LodePNG 20230410 by Lode Vandevenne, retained via the BlocksDS PNG example. Their complete license is retained in both source files. Encoder and ancillary-chunk support are disabled at compile time.

## DejaVu Sans Mono

Copper, Tide and Prism bundle native bitmap packs generated from DejaVu Sans Mono Bold. Nocturne and Eclipse bundle Latin-1 packs from DejaVu Sans Mono. Each external font has its full license beside the pack.

`arm9/source/ui/font.h` is rasterized from DejaVu Sans Mono by `tools/make_font.py`. Font license follows.

```text
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: DejaVu fonts
Upstream-Author: Stepan Roh <src@users.sourceforge.net> (original author),
                  see /usr/share/doc/fonts-dejavu-core/AUTHORS for full list
Source: https://dejavu-fonts.github.io/

Files: *
Copyright: Copyright (c) 2003 by Bitstream, Inc. All Rights Reserved. 
 Bitstream Vera is a trademark of Bitstream, Inc.
 DejaVu changes are in public domain.
License: bitstream-vera
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of the fonts accompanying this license ("Fonts") and associated
 documentation files (the "Font Software"), to reproduce and distribute the
 Font Software, including without limitation the rights to use, copy, merge,
 publish, distribute, and/or sell copies of the Font Software, and to permit
 persons to whom the Font Software is furnished to do so, subject to the
 following conditions:
 .
 The above copyright and trademark notices and this permission notice shall
 be included in all copies of one or more of the Font Software typefaces.
 .
 The Font Software may be modified, altered, or added to, and in particular
 the designs of glyphs or characters in the Fonts may be modified and
 additional glyphs or characters may be added to the Fonts, only if the fonts
 are renamed to names not containing either the words "Bitstream" or the word
 "Vera".
 .
 This License becomes null and void to the extent applicable to Fonts or Font
 Software that has been modified and is distributed under the "Bitstream
 Vera" names.
 .
 The Font Software may be sold as part of a larger software package but no
 copy of one or more of the Font Software typefaces may be sold by itself.
 .
 THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF COPYRIGHT, PATENT,
 TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL BITSTREAM OR THE GNOME
 FOUNDATION BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, INCLUDING
 ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM OTHER DEALINGS IN THE
 FONT SOFTWARE.
 .
 Except as contained in this notice, the names of Gnome, the Gnome
 Foundation, and Bitstream Inc., shall not be used in advertising or
 otherwise to promote the sale, use or other dealings in this Font Software
 without prior written authorization from the Gnome Foundation or Bitstream
 Inc., respectively. For further information, contact: fonts at gnome dot
 org.

Files: debian/*
Copyright: (C) 2005-2006 Peter Cernak <pce@users.sourceforge.net> 
           (C) 2006-2011 Davide Viti <zinosat@tiscali.it>
           (C) 2011-2013 Christian Perrier <bubulle@debian.org>
           (C) 2013 Fabian Greffrath <fabian+debian@greffrath.com>
License: GPL-2+
 This program is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later
 version.
 .
 This program is distributed in the hope that it will be
 useful, but WITHOUT ANY WARRANTY; without even the implied
 warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the GNU General Public License for more
 details.
 .
 You should have received a copy of the GNU General Public
 License along with this package; if not, write to the Free
 Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 Boston, MA  02110-1301 USA
 .
 On Debian systems, the full text of the GNU General Public
 License version 2 can be found in the file
 /usr/share/common-licenses/GPL-2'.

```

## SD write compatibility driver

`third_party/libnds/sdmmc_compat.twl.c` is based on BlocksDS libnds commit `e478cc660a8c7254fbffc54bda3ce0b93ab14c15`, the submodule used by BlocksDS v1.25.0, originally by profi200. It is locally modified to optionally use multi-block commands for SD writes, including one-sector writes, and to select the matching error recovery. This avoids a reproduced SD write hang in melonDS 1.1. NAND read/write command selection is unchanged. A second compatibility change sends CMD16 with a 512-byte block length for internal NAND initialization: melonDS 1.1 otherwise leaves this byte-addressed device at a zero-byte transfer size. This configures the controller without writing NAND contents. Normal DMA paths remain enabled. The Zlib license is in `third_party/libnds/LICENSE`.

Upstream: https://github.com/blocksds/libnds/blob/e478cc660a8c7254fbffc54bda3ce0b93ab14c15/source/arm7/storage/sdmmc.twl.c

`third_party/libnds/common/libnds_internal.h` is retained unmodified from that same upstream revision to supply the internal emulator-detection helper required by the driver.

Copper’s background music is “Sentinel” by pixelpion, from [Pixabay](https://pixabay.com/music/video-games-sentinel-455122/), under the Pixabay Content License. See `sdroot/_nds/dsi-frontend/themes/copper/CREDITS.md` for license links, source/conversion hashes and conversion settings. The UI cues remain original synthesized assets generated by `tools/make_audio_samples.py`.

The game-code/save-memory table under `third_party/rom-save-table` is from TWiLightMenu revision `22979a2b800f1d8a598d1a1778ee6d7ea079d5fe`, originally copyright 2016–2021 Arisotura, GPL-3.0-or-later. Full GPL text and the documented `static const` declaration change are retained there.

R4 cheat-format compatibility follows [AKMenu Next cheat.cpp](https://github.com/coderkei/akmenu-next/blob/c884c092e1bf1b172f9a2464f74c13ce53124400/arm9/source/cheat.cpp) at revision `c884c092e1bf1b172f9a2464f74c13ce53124400` (portions copyright 2008 Normmatt, Smiths and bliss; copyright 2009 yellow wood goblin; GPL-3.0-or-later). The implementation validates bounds and stores selections separately. Full GPL-3 text is retained at `third_party/rom-save-table/COPYING`. Test databases/code words are original synthetic parser fixtures.

`arm9/source/ui/font_latin.h` is generated from DejaVu Sans Mono Bold (size 10, baseline 10) using `tools/pack_theme_font.py`; its license is included in the Copper font package and reproduced above.


## Cooperative image decoding changes

LodePNG has optional `FRONTEND_DECODE_PROGRESS` checkpoints in CRC/Adler work,
Huffman inflation, unfiltering and scalar/bulk color conversion. Cancellation
leaves through existing error/cleanup paths; partial color conversion is never
published because the wrapper rejects cancelled results. The decoder's source
and original notices remain under `third_party/lodepng`; see its UPSTREAM.md.
`stb_image.h` has corresponding opt-in JPEG progress/cancellation hooks; see
`third_party/stb/UPSTREAM.md`. Defaults remain no-ops for other users. No format
validation or checksum check is bypassed. Native and host-context tests compare
prepared pixels against the synchronous decoder.

## Slot-1 helper

The SD release builder fetches the unmodified official Simple DS Slot-1 Launcher
1.4.1.6 binary from [Lorenzooone's release](https://github.com/Lorenzooone/Simple-DS-Slot-1-Launcher/releases/tag/1.4.1.6),
source commit `c372cf061dad8f52ef13f1bce9beaa24c8c40253`.
Copyright (C) 2024 Lorenzooone; GPL-3.0-or-later. Upstream credits TWiLight Menu++,
Apache Thunder's NTR Launcher and Michael “Chishm” Chisholm's NitroHax.

The full notice is in `third_party/slot1launcher/COPYING` and
`sdroot/_nds/dsi-frontend/loaders/slot1launch.txt`; release copies accompany
the binary download in `licenses.zip`. Matching complete upstream
source is retained under `dependencies/` in `source.zip`. Gloam invokes the
helper as a separate executable and makes no authorship claim over its implementation.

## Original sample-theme assets

Folio, Nocturne, Tide, Phosphor, Eclipse and Prism use original geometric artwork and synthesized audio,
generated by `tools/make_theme_samples.py`. These original assets are dedicated
to the public domain under CC0 1.0; per-package CREDITS.md files document this.
Their packed DejaVu fonts remain under the font terms reproduced above and in
each package's `fonts/LICENSE.txt`. No external music or Nintendo artwork is
included in these six sample themes.

## nds-bootstrap release distribution

The release builder includes unmodified official nds-bootstrap v2.17.0,
source commit `b9044ff2e66d5a522f9678c185f421a603f97d7a`. The root GPL license,
upstream README/credits and nested libeasysave MIT notice are retained under
`third_party/nds-bootstrap`; embedded source notices remain in the complete
matching `source.zip`, under `dependencies/`. No private experimental backend
patches are applied to the packaged executable.

All binary release downloads must be accompanied by the matching combined
`source.zip` archive with equivalent access, per GPLv3 section 6(d).
The package manifest records exact release/source URLs and archive/member hashes;
link the source download beside `sd.zip` when publishing. A generic repository
link is not a substitute for the corresponding revision and local modifications.
See [building and distribution](docs/development/BUILDING.md).

## Optional emulator/player overlay

`tools/build_addons.py` uses the reviewed pins in
`config/addon-dependencies.json`. These standalone applications are not linked
into Gloam. Exact release/source URLs, revisions, SHA-256 hashes, argv evidence,
BIOS requirements and distribution decisions are in the
[add-on inventory](docs/guides/ADDONS.md).

The reviewed-license portion includes DSXP (nextcode4u's SD-argv fork,
GPL-2.0-or-later), GameYob
(MIT), A7800DS, A8DS and StellaDS (GPL-2.0-or-later with inherited notices),
PokeMini (GPLv3 and constituent notices), snemulds-shim (GPL-3.0-or-later with
vendored notices) and its exact matching SNEmulDS payload (GPL-2.0-or-later).
Altirra firmware notices and PokeMini replacement-BIOS source are retained.

A5200DS, DracoDS, MicroDS, SpeccySE and ColecoDS require the explicit `--noncommercial`
option. Their no-profit conditions remain applicable to those programs;
Gloam's GPL does not replace their licenses. All original notice/README material,
the full GPLv2 text, nested DSXP/libmad license and firmware notices accompany
`addons.zip` in the updated `licenses.zip`. Exact upstream source archives,
including the released DSXP source with libmad, are retained in the updated
combined `source.zip` under `dependencies/addons/`.

No games or proprietary external BIOS files are supplied. Applications with
unconfirmed permission are not bundled; no grant is inferred from an absent
prohibition. Their [download-only record](docs/guides/DOWNLOAD_ONLY_ADDONS.md)
retains source links and unresolved findings. The shared Python/PowerShell
installer downloads pinned runtime files directly from upstream to a chosen SD
root. Their third-party source/notice archives are not collected in our release
companions. Direct downloading does not authorize redistributing those files.
