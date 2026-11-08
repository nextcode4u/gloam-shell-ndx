# Installed DSiWare

Open **System → Installed DSiWare** to browse titles on the console's internal
NAND. This library is separate from DSiWare files on SD. It requires DSi mode,
readable NAND and a boot environment that honors native title requests.

Discovery mounts NAND read-only and scans `title/00030004` only when requested.
It reads one directory entry/title per frame, supports up to 128 titles, and
reports truncated scans or unsupported metadata. Names come from localized
banners, falling back to a title ID. Icons use the existing lazy banner cache:
animated banners animate, static banners stay static, and Reduced motion freezes
animation. List view still shows the selected icon on the other screen.

B returns to System. Settings → Maintenance → Refresh metadata rescans the library. Navigation
remembers the selected title ID rather than its position in the sorted list.
Settings, Camera, Sound, PictoChat and Download Play retain their separate System
entries; installed games and unsigned homebrew with compatible metadata appear
in this library.

## Launch and saves

A prepares both the native title-ID request and Unlaunch's NAND content path.
Metadata, header identity and content size are checked again before reboot.
The boot content comes from the TMD's boot index; it is not assumed to be
`00000000.app`. This is structural validation, not signature authentication.

The frontend does not install, export, rename or modify NAND titles or saves.
It never opens the title's `data` directory during discovery. The native NAND
route leaves public/private save handling to the firmware/Unlaunch title loader,
using the installed title's existing save locations. SD save slots, cheats,
per-file emulator associations and nds-bootstrap settings do not apply here;
those actions are disabled for installed entries.

There is no injected return hotkey. A title's own Quit behavior controls its
exit. The optional [Gloam Menu tile](GLOAM_FORWARDER.md) provides a way back from
the DSi Menu. Stock firmware cannot launch unsigned homebrew simply because its
metadata is listed. No installer, Nintendo icons, games, NAND or saves are bundled.

## Compatibility

Discovery does not establish that a title can launch in the current boot
environment. Physical DSi, SDNAND, private saves and title-specific exit behavior
require further validation. See [current support](../CURRENT_SUPPORT.md).

Protocol references: [Unlaunch](https://problemkaputt.de/unlaunch.htm) and
[GBATEK autoload layout](https://mgba-emu.github.io/gbatek/#dsi-autoload-on-warmboot).
