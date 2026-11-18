---
hide:
  - navigation
---

# Set up your SD card

You need a DSi that can already run homebrew from its SD card.
If yours cannot yet, follow the [DSi homebrew guide](https://dsi.cfw.guide/) first,
then return here to install Gloam.
Back up your card first. Gloam does not install Unlaunch.

## SD Setup for Windows

1. Open **PowerShell** and paste this command:

    ```powershell
    irm https://raw.githubusercontent.com/nextcode4u/gloam-shell-ndx/main/setup.ps1 | iex
    ```

2. Choose your SD card and click **Use folder**.
3. Select **Install / repair setup**.
4. Safely eject the card, put it in your DSi and launch **BOOT.NDS**.

## Without the setup tool

1. Download **sd.zip** from the [latest release](https://github.com/nextcode4u/gloam-shell-ndx/releases/latest).
2. Extract its contents onto your SD card, keeping existing saves and settings.
3. Launch **BOOT.NDS** using your console's homebrew boot method.

Games are not included. For emulators, open **Add-ons** in the Windows tool.

## Updating

Open the same setup tool and choose **Update Gloam + backends**. It keeps your
settings and saves. For a manual update, check the latest release notes.

[Next: add your files and start playing →](USING.md)
