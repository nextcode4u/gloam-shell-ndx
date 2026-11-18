# Gloam Shell NDX

A customizable launcher for your **Nintendo DSi SD card**. Browse games,
homebrew and media, and open system apps from one place.

“Gloam” means twilight—a nod to the DSi's **TWL** codename.

**[Website & SD Setup](https://nextcode4u.github.io/gloam-shell-ndx/)**
· [Download Gloam](https://github.com/nextcode4u/gloam-shell-ndx/releases/latest)
· [Setup guide](docs/INSTALL.md)
· [Browse themes](https://github.com/nextcode4u/gloam-shell-theme-directory)

<p align="center">
  <img src="docs/images/theme-variants.png" width="768" alt="Phosphor, Eclipse and Prism themes showing Gloam's two-screen browser">
</p>

Theme previews with example content. [See more themes](docs/images/theme-personalities.png).

## What you can do

- Browse your collection using lists, grids or a cover shelf.
- Make it yours with themes, backgrounds, music, sounds and startup animations.
- Open games with cheats, save slots and individual game settings.
- Launch homebrew, emulators, books, music and videos using compatible apps.
- Keep favorites, recent items and your own folder tabs close at hand.
- Access installed DSiWare, DSi system apps and game cartridges.

See [compatibility and known issues](docs/CURRENT_SUPPORT.md) for what works
with your setup.

## Install

Your DSi must already be able to run homebrew from its SD card.
Not set up yet? Follow the [DSi homebrew guide](https://dsi.cfw.guide/) first.

Some features also need Unlaunch. Gloam does not install Unlaunch or modify your console's setup.
Back up your SD card before installing.

### Windows setup tool

Open PowerShell and paste:

```powershell
irm https://raw.githubusercontent.com/nextcode4u/gloam-shell-ndx/main/setup.ps1 | iex
```

Choose your SD card, then install Gloam. The tool can also install emulators,
download themes and covers, and help organize your folders.
[Windows setup instructions](docs/guides/WINDOWS_SETUP.md).

### Install manually

1. Download `sd.zip` from the [latest release](https://github.com/nextcode4u/gloam-shell-ndx/releases/latest).
2. Extract its contents to the root of your SD card, keeping your existing games,
   saves and settings.
3. Start `BOOT.NDS` using your console's homebrew boot method.

Games are not included. For emulators and media players, use the setup tool or
follow the [add-on guide](docs/guides/ADDONS.md). Some emulators need BIOS files;
the guide explains which ones.

## Add your files

| Content | Folder on your SD card |
| --- | --- |
| Games | `/roms/` |
| Homebrew games | `/roms/Homebrew Games/` |
| Apps and utilities | `/apps/` |
| Books, music and videos | `/media/books/`, `/media/music/`, `/media/videos/` |

[Emulator setup](docs/guides/EMULATORS.md) explains supported file types and folders.

## Basic controls

| Button | Action |
| --- | --- |
| D-pad | Move between items |
| A / B | Open / go back |
| L / R | Switch tabs |
| START | Open settings |
| SELECT | Open item options |
| Y | Show item details |

Tap an item to select it, then tap again to open it.
[Full controls](docs/GETTING_STARTED.md#controls).

## Update Gloam

Use the setup tool, or download `BOOT.NDS` from the latest release and replace
the one on your SD card. Your games, saves and settings stay in place.

Read the release notes before updating: some versions also need updated themes
or other files. Follow the [upgrade guide](docs/GETTING_STARTED.md#install-or-upgrade)
when more than `BOOT.NDS` is needed.

## Help and customization

- [Themes and appearance](docs/guides/THEME_PACKAGES.md)
- [Games, saves and cheats](docs/guides/GAME_LOADING.md)
- [Backups and recovery](docs/guides/CONFIGURATION_BACKUPS.md)
- [All guides](docs/README.md)
- [Report a problem](https://github.com/nextcode4u/gloam-shell-ndx/issues)

## Credits

Thanks to the DS homebrew community, including the developers of AKMenu Next,
TWiLight Menu++, Pico Launcher and HBMenu. Gloam's two-screen organization also
draws on Firmux, an earlier deprecated project by me.

[Full credits](CREDITS.md) · [License](LICENSE) · [Contributing and building](CONTRIBUTING.md)

## Developer note

This project was developed with assistance from AI language models. Some source
code was generated or refined using AI, then reviewed, modified and integrated
by the project author where appropriate.
