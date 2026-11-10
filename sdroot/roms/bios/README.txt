bios
========================================

SD folder: /roms/bios/

Support firmware only, not a game collection. Use each emulator's exact
filename and lookup path. Original commercial BIOS files are not supplied. GBA
firmware belongs at /_gba/bios.bin, not in this folder.

a5200ds: Altirra 5200 replacement is already compiled in; some games need
user-supplied original 5200.rom in /roms/bios or /data/bios.

a7800ds: No BIOS required normally. Optional 7800.rom and highscore.rom go in
/roms/bios, /data/bios or beside emulator.

a8ds: Built-in Altirra OS 3.41 and BASIC 1.58. Optional originals:
atarixl.rom, atariosb.rom, ataribas.rom, a5200.rom in /roms/bios or
/data/bios.

colecods: ColecoVision requires coleco.rom; Sord M5 requires sordm5.rom (JP)
or sordm5p.rom (PAL). MSX1 has built-in C-BIOS for cartridges, without
BASIC/cassette/disk support. Additional machine BIOS files are user-supplied.

dracods: User supplies dragon32.rom or bas12.rom/extbas11.rom; disk11.rom for
CoCo disks. Look in /roms/bios or beside emulator.

ds994a: Requires 994aROM.bin and 994aGROM.bin in /roms/bios or /roms/ti99;
994aDISK.bin is needed for disk support.

dslibris: No BIOS required. Fonts: /data/dslibris/font/. Books: /media/books/.
New settings: /data/dslibris/dslibris.xml; an existing legacy root settings
file remains in use unless managed settings exist.

gameyob-dsi: No BIOS required for normal GB/GBC play. Optional SameBoot v1.0.5
(MIT/Expat) is supplied at /roms/bios/gbc_bios.bin. Set
biosfile=/roms/bios/gbc_bios.bin under [general] in /_nds/gameyobds.ini and
enable GBC Bios in GameYob if wanted; existing BIOS/config files are
preserved. Hardware compatibility remains unverified.

gbarunner3: Requires original 16 KiB GBA BIOS at /_gba/bios.bin. Fixed-offset
relocation/patches prevent assuming a replacement BIOS is compatible.

gimlids: Requires KERNAL.ROM, BASIC.ROM, CHAR.ROM and 1541.ROM in /roms/bios
or /roms/c64. MEGA65 Open ROMs are not a verified complete replacement set for
GimliDS.

hachibitto: Stock binary embeds MSX/MSX2/MSX2+ and disk firmware. Optional
Kanji font is user-supplied. C-BIOS is not a verified drop-in full
replacement.

lodjurds: Requires a user-supplied Atari Lynx BIOS, selected inside LodjurDS.
Upstream explicitly says it is needed to run games; not included.

microds: Requires MC10.BIN or MC10.ROM; optional MCX.BIN/MCX.ROM. Put them in
/roms/bios or beside emulator.

nesds: NES cartridges need no external BIOS. FDS content requires disksys.rom;
user-supplied.

ngpds: Upstream says a real NGP/NGPC BIOS is needed for in-game saving. Select
it in emulator settings; not included.

nintv-ds: Requires exec.bin and grom.bin; optional ivoice.bin, ecs.bin and
Tutorvision firmware. Configure BIOS location in emulator; /roms/bios is the
recommended collection.

nitrografx: PCE cartridge route needs no BIOS. CD-ROM games require a user-
supplied CD system BIOS; CD files are not default argv associations.

nitroswan: Built-in startup handling permits games without original firmware;
optional WS/WSC/Crystal BIOS can be selected in the emulator.

picodrivetwl: Genesis cartridges need no separate BIOS; Sega CD is not a
configured route.

pokemini: Upstream replacement FreeBIOS is already compiled in; original
Nintendo BIOS is not needed for basic operation.

s8ds: Configured SMS/GG/SG-1000 cartridge routes normally bypass BIOS. Other
emulated machines can require their own BIOS selected inside S8DS.

scummvm: No general console BIOS. Supply the required data files for each
supported game; do not bundle commercial game data.

snemulds-shim: No console BIOS needed for the selected SNEmulDS builds. Keep
shim INI and NTR/TWL executables together.

speccyse: Requires 48.ROM and 128.ROM beside emulator or in /roms/bios. OpenSE
BASIC is a candidate for 48K only, not a verified full replacement for this
required pair.

stellads: No separate Atari 2600 BIOS required.

sugards: Amstrad CPC firmware and ParaDOS are embedded in the stock binary,
with separate terms; no extra download is needed, but embedding is not a free-
BIOS license.

wasabids: No separate Supervision BIOS required.

Full setup and compatibility audit:
https://github.com/nextcode4u/gloam-shell-ndx/blob/main/docs/guides/ADDONS.md
