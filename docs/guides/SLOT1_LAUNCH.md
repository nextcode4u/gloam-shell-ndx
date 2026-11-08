# Slot-1 cartridges

**System → Launch Slot-1 card** opens Simple DS Slot-1 Launcher through Gloam's
validated native homebrew handoff. The SD package installs its official release
at `/_nds/dsi-frontend/loaders/slot1launch.nds`, with attribution and GPL notices.
The matching source accompanies the SD artifact as `dependencies-source.zip`.

## Icon and title

When the action is visible, Gloam cooperatively reads the inserted cartridge's
header/banner. The browser and top preview use the game icon; the preview uses
its localized title. DSi banner animation respects Reduced motion. List keeps
text-only rows while showing the selected icon above.

Removal clears cached metadata; reinsertion starts a new read. Invalid,
unsupported or unavailable banners retain the original cartridge symbol.
The reader performs at most one bounded card transfer per frame and never
issues save-memory writes. It uses a validated NTR key seed left in runtime
ITCM by the boot chain; no BIOS key table or Nintendo artwork is shipped.
Locked SCFG or missing runtime seed prevents banner discovery without blocking
the helper's independent launch initialization. Physical-card compatibility
remains unverified.

Protocol: [GBATEK DS cartridge documentation](https://problemkaputt.de/gbatek.htm#dscartridgeprotocol).

## Helper selection

The first existing regular file is selected:

1. `/_nds/dsi-frontend/loaders/slot1launch.nds`
2. `/_nds/slot1launch.nds`
3. `/_nds/akmenunext/slot1launch.nds`
4. `/_nds/TWiLightMenu/slot1launch.srldr`
5. `/_nds/TWiLightMenu/slot1launch.nds`

An invalid preferred helper produces an error instead of silently trying another.
The helper must pass binary/homebrew identity checks. B cancels pending launch;
buttons must be released before handoff. Gloam saves navigation first. Per-file
associations cannot redirect this system action.

## Source and credit

[Simple DS Slot-1 Launcher](https://github.com/Lorenzooone/Simple-DS-Slot-1-Launcher)
is by **Lorenzooone**, GPL-3.0-or-later, and credits TWiLight Menu++,
Apache Thunder's NTR Launcher and Michael “Chishm” Chisholm's NitroHax.
It resets the card, configures the execution environment and loads its programs.
Gloam delegates cartridge execution to that work; its icon reader is separate.

Release/source pins are in [release-dependencies.json](../../config/release-dependencies.json).
To build the helper, use the exact checkout under `build/release-sources/slot1launcher`
after packaging, or extract its matching dependency source. Follow its upstream
BlocksDS/Pillow build instructions. Preserve all upstream notices when redistributing.
