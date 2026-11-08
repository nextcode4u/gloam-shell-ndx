# Homebrew, argv and system applications

Gloam provides native DSi-SD homebrew handoff. Application-specific storage and
return behavior still depend on the application build.

## User behavior

Open a native homebrew `.nds`, `.dsi`, `.srl` or `.app` file, an `.argv` shortcut,
or content with a configured plugin association. Preferences are saved before
handoff. A failed save or preflight leaves the browser available with an error.
Existing application files, saves and other frontend configurations are not
rewritten.

A shortcut is a text file containing an SD application/content path and optional
arguments. SD-absolute paths are recommended:

```text
"sd:/apps/Homebrew Apps/My Application.nds" --option "two words"
```

Single/double quotes preserve spaces. Lines beginning with `#` or `;` outside a
token are comments. No shell is involved. The first token is resolved against
the SD root; subsequent tokens are passed literally and are not rebased.
There are at most 16 arguments including argv[0], with 4 KiB total command data.
Malformed quotes, oversized files and empty arguments are rejected. Empty
arguments are rejected because runtime parsers do not preserve them consistently.

Plugin profiles use `_nds/dsi-frontend/plugins/*.ini`, including the supplied
preconfigured profiles. `argv=1` sends the selected content path as argv[1];
`argv=0` starts the application without it. `dsipath` selects the DSi override.
Profiles requesting `bootstrap=1` or `hbargv=1` use the separate optional
`_nds/nds-bootstrap-hb-release.nds` adapter. It supports one content argument,
not arbitrary argument lists. Storage and return compatibility remain application-specific; see
[backend limits](GAME_LOADING.md). Personal extension defaults precede supplied profiles; supplied shared extensions still use the
alphabetically first profile filename. See [the association editor](ASSOCIATION_EDITOR.md).

The direct route accepts the known homebrew game-code markers `####`, `GYOB`
and `TGDS` plus nds-bootstrap’s `KBSE` marker and validates CPU binary ranges, DSi extension ranges and argv/stub
reservations. Custom IDs can also qualify through validated linked DLDI metadata or the
SDK's exact padded `HOMEBREW` title. Known retail IDs retain backend routing.
Unsupported headers or very large ARM9 images can still fail preflight; a
header marker is not authentication.

## Return to this frontend

A return stub reloads root `BOOT.NDS`. Install this frontend as that file. The
stub does not return to the stock DSi System Menu. The frontend restores its
saved folder, selected entry and tab on return, subject to the chosen startup
tab preference.

The launched application must retain the stub and invoke a compatible runtime
exit/reset path. Compatible BlocksDS applications can do this by returning from `main()`. An application that replaces the stub, never exits, or uses a different
reset protocol needs separate integration. A successful probe does not certify
all installed emulator return paths.

The pinned SD-only loader and its notices are in `third_party/nds-bootloader`.
Its build excludes the unused 32 KiB DLDI patch area, keeping the return code
below current runtime exception memory. The combined stub/loader is capped at
24 KiB. See `UPSTREAM.md` there for revisions and build details.

## System application icons

System entries reuse the banner from the console's discovered, validated boot
content on read-only NAND. No copied Nintendo artwork is bundled. Discovery runs
when the System view is first visited; icon reads use the normal bounded cache,
outside rendering. The original fallback symbols cover Settings, Camera, Sound,
PictoChat, Download Play, system-menu return and Slot-1 when a banner is missing,
unsupported or NAND cannot be accessed. A fallback does not mean the app is
available to launch.

Classic, Extended and Grid show row/tile icons. List remains text-only below,
while the selected system icon appears in the top-screen preview when covers are
enabled. Slot-1 uses the inserted cartridge's banner when readable, with its
title in the preview; see [cartridge metadata](SLOT1_LAUNCH.md#cartridge-icon-and-title).
Native animated banners use the existing playback and reduced motion settings.

NAND is mounted with `nandInit(true)`; see the official
[BlocksDS filesystem guide](https://blocksds.skylyrac.net/docs/guides/filesystem/).
No NAND writes or icon-export step is needed.

For an optional installed DSi Menu shortcut back to the SD launcher, see
[Gloam forwarder](GLOAM_FORWARDER.md). It is separate from an application return stub.

## Explicit DSi Menu return under Unlaunch

The frontend discovers Launcher (`00030017`) through the same bounded,
read-only metadata checks as other system titles. The explicit Return to DSi
System Menu action now supplies its NAND content path as well as the stock
zero-ID TLNC record. This bypasses Unlaunch's default frontend target when a
valid Launcher is discovered. Ambiguous/missing metadata never produces a
guessed path; stock firmware retains its ordinary TLNC fallback. The optional
forwarder's missing-SD retry screen still has its separately implemented Menu
request and is not certified by this frontend control.
