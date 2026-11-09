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
| GBARunner3 | develop snapshot `ecaa817815d9761745606592f04affe5ee9c3731` | No official release or public CI artifact available; no binary bundled. |
| FastVideoDS Player | v0.0.1 | Included under this assumption. |
| jEnesisDS Plus | v0.8.1 | Included under this assumption; original base is closed source. |
| S8DS | v1.1.8 | Included under this assumption. |
| NitroGrafx | v0.9.1 | Included under this assumption. |
| NitroSwan | v0.7.7 | Included under this assumption. |
| NGPDS | v0.6.0 | Included under this assumption. |
| LodjurDS | v0.1.5 | Included under this assumption. |
| WasabiDS | v0.2.6 | Included under this assumption. |

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

GBARunner3 has no automatically selected binary. The redistribution assumption
is recorded independently of this availability limit; a future package needs a
specific build, hash and accurately identified source before it can be added.

## Maintaining this policy

Record an explicit upstream license or permission if one becomes available and
update the entry's status and retained notices. Do not silently turn an
`assumed` entry into `approved`, invent a license, or remove upstream conditions.
If an assumption needs to change, update the lock and regenerate the add-on and
its matching source/license companions.

The [dependency lock](../../config/addon-dependencies.json) records primary
release/source links, license findings and these decisions. See the
[full add-on inventory](ADDONS.md) for argv, BIOS and remaining package limits.
