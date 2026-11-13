# Download-only add-ons

Applications with unconfirmed redistribution permission or unresolved component
coverage are excluded from Gloam's binary packages and dependency source/notice
collections. Their URLs, hashes and setup information remain in the repository.
The shared Python/PowerShell installer downloads their files directly from the
recorded upstream release or host onto a user-selected SD root. No redistribution
permission is inferred from availability, age or the absence of an explicit ban.

| Application | Version | Delivery |
| --- | --- | --- |
| ds994a | 2.9 | Direct download; not bundled. |
| fastvideods | v0.0.1 | Direct download; not bundled. |
| gbarunner3 | hicode (third-party snapshot) | Direct download; not bundled. |
| jenesisdsplus | v0.8.1 | Direct download; not bundled. |
| lodjurds | v0.1.5 | Direct download; not bundled. |
| picodrivetwl | v2.1.0 | Direct download; not bundled. |
| nesds | v2.1 | Direct download; not bundled. |
| ngpds | v0.6.0 | Direct download; not bundled. |
| nitrografx | v0.9.1 | Direct download; not bundled. |
| nitroswan | v0.7.7 | Direct download; not bundled. |
| s8ds | v1.1.8 | Direct download; not bundled. |
| tuna-vids | v1.2-clut | Direct download; not bundled. |
| wasabids | v0.2.6-audio-init.1 | Temporary startup-fix fork; direct download, not bundled. |

DS994a and PicoDriveTWL retain their noncommercial conditions: the downloader requires
`--noncommercial` / `-Noncommercial`. DS994a's embedded speech samples have no separately
established redistribution grant. Other unconfirmed component terms remain
recorded in the [dependency lock](../../config/addon-dependencies.json).

GBARunner3's direct argv path is statically verified in the supplied core, but
its exact source revision remains unidentified. See [its setup notes](GBARUNNER3_NOTICES.md).
For tuna-viDS, the executable Helix notice and GPL/RPSL issues remain recorded in
[its component notes](TUNA_VIDS_NOTICES.md). Direct downloading does not resolve
those source/license questions or authorize redistributing the downloaded files.

The installer also offers noncommercial packages omitted from a default build,
with the explicit noncommercial option. Apps without reviewed runtime pins are
listed for manual setup instead of guessed downloads. See [usage](ADDONS.md).
