# Add-ons with unconfirmed redistribution terms

Gloam's packaging policy assumes redistribution is permitted for the specific
projects listed below. This is a project packaging assumption, not a verified
license grant, endorsement or permission from the upstream authors. Their
license status remains unconfirmed in the dependency lock and release manifests.

This policy covers the pinned releases below. It does not extend to other
held applications, commercial games, proprietary external BIOS, fonts or other
assets with separately identified restrictions. Noncommercial-only packages
still require the separate build option and retain their no-profit terms.

| Project | Recorded version | Package availability |
| --- | --- | --- |
| GBARunner3 | develop snapshot `ecaa817815d9761745606592f04affe5ee9c3731` | Third-party hicode ZIP pinned as a candidate; exact core source revision unidentified; not bundled. |
| FastVideoDS Player | v0.0.1 | Included under this assumption. |
| jEnesisDS Plus | v0.8.1 | Included under this assumption; original base is closed source. |
| S8DS | v1.1.8 | Included under this assumption. |
| NitroGrafx | v0.9.1 | Included under this assumption. |
| NitroSwan | v0.7.7 | Included under this assumption. |
| NGPDS | v0.6.0 | Included under this assumption. |
| LodjurDS | v0.1.5 | Included under this assumption. |
| WasabiDS | v0.2.6 | Included under this assumption. |
| nesDS | v2.1 | Included; legacy public-domain statement does not establish newer CPU-core terms. |

DS994a 2.9 is included only with `--noncommercial`, following its explicit
no-profit distribution statement. Its embedded game speech samples have no
separately established grant; the project assumes redistribution of the unchanged
release is permitted, without asserting clearance for those samples. This does
not extend to separately distributed recordings or games.

## Attribution and source

Keep upstream copyright notices, READMEs and any component licenses intact.
Gloam's GPL license applies to Gloam's original code; it does not license these
programs or fill a missing grant. Binary files are unchanged upstream release
members, recorded with their download URL and SHA-256 hash.

The companion `licenses.zip` contains available upstream notices and a copy of
this document under `addons/UNCONFIRMED_DISTRIBUTION.md`. Its filename does not
mean every included program has confirmed license coverage.

The combined `source.zip` contains the published project archives and pinned
submodule archives under `dependencies/addons/`, with exact revisions and
hashes in the manifests. To assemble a published source tree, extract its
`source.tar.gz`, then extract each `submodules/<path>.tar.gz` into that original
`<path>` in the tree, removing each archive's top-level directory. The per-project
`release.json` records submodule paths, repositories and commits.

For jEnesisDS Plus this is the available patch/injected
code, not the unpublished original jEnesisDS emulator source. No complete-source
claim is made for that base.

The supplied GBARunner3 hicode ZIP now has pinned archive/core hashes, but no
build manifest identifying its exact source revision. The recorded develop
snapshot is reference source only. The core and its 304 configs remain a manual
candidate until corresponding source is identified; this is independent of the
redistribution assumption. See [the core/frontend setup notes](ADDONS.md#gbarunner3-core-and-optional-browser).

nesDS includes the exact RP2A03 `d4b8d3dffaf9f0eced4cf707a2ce2208eb1bdaa0`
and nested ARM6502 `8b8b6c82ce29bd26cd76709c75167372b3711484` source archives.
Their pinned READMEs provide attribution, but neither pin has a project-wide
license file. Inclusion follows the stated assumption, rather than assigning
them the older nesDS public-domain dedication or a license from newer revisions.

## Maintaining this policy

Record an explicit upstream license or permission if one becomes available and
update the entry's status and retained notices. Do not silently turn an
`assumed` entry into `approved`, invent a license, or remove upstream conditions.
If an assumption needs to change, update the lock and regenerate the add-on and
its matching source/license companions.

The [dependency lock](../../config/addon-dependencies.json) records primary
release/source links, license findings and these decisions. See the
[full add-on inventory](ADDONS.md) for argv, BIOS and remaining package limits.
