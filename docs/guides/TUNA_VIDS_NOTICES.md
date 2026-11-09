# tuna-viDS distribution and source notices

This package includes Michael “Chishm” Chisholm's unchanged tuna-viDS v1.2
release. Gloam's license does not replace the player's component licenses.

## Component notices

Helix DNA Client technology included. Copyright (c) RealNetworks, Inc.,
1995-2002. All rights reserved.

The Helix decoder files also retain their upstream 1995-2004 copyright notice.
The upstream README describes the ARM9 binary as GPL v2 and the ARM7 binary
as GPL v2 plus the RealNetworks Public Source License (RPSL). Individual GPL
source headers offer version 2 or later. Keep the original notices with each
component, including the Xvid authors and Helix contributors.

The companion `licenses.zip` includes the upstream README, GPL text and Helix
`LICENSE.txt`, `RPSL.txt` and `RCSL.txt` under `addons/tuna-vids/`.
RPSL is the stated default for Helix. The RCSL is supplied as upstream reference;
using that alternative requires a valid applicable agreement with RealNetworks.

## Obtaining source

Player and codec source is provided with this release in
`source.zip/dependencies/addons/tuna-vids/source.tar.gz`. Extract that archive
for the source, original build files and per-file notices. Its revision is
`f1f00243b2f969ebc9ded1a4942f610819f8c673`. The accompanying `release.json`
records download URLs and SHA-256 hashes.

- [Player release and credits](https://github.com/chishm/tuna-vids/releases/tag/v1.2)
- [Pinned player and codec source](https://github.com/chishm/tuna-vids/tree/f1f00243b2f969ebc9ded1a4942f610819f8c673)
- [Download the pinned source archive](https://codeload.github.com/chishm/tuna-vids/tar.gz/f1f00243b2f969ebc9ded1a4942f610819f8c673)
- [Helix RPSL](https://github.com/chishm/tuna-vids/blob/f1f00243b2f969ebc9ded1a4942f610819f8c673/arm7/source/helix/RPSL.txt)

## Recorded distribution assumption

The project includes the unchanged upstream binary under its
[documented distribution assumption](UNCONFIRMED_DISTRIBUTION.md), with these
companion notices and source. RPSL section 2.1(e) calls for source-availability
information in the executable and the Exhibit A object-code notice where
copyright notices appear. That Helix notice was not found in the stock binary
or its startup source. This companion document provides the information for
recipients; it does not change the executable or establish that its notice
requirement has been satisfied. The underlying GPL/RPSL compatibility question
also remains recorded rather than being declared resolved.

No downstream player or codec modifications are included. Future modified builds
must identify their changes and retain the applicable source and notices.
