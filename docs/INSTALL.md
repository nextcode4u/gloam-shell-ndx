---
hide:
  - navigation
---

# Install Gloam

You need a DSi that can already run homebrew from its SD card.
Back up your card first. Gloam does not install Unlaunch.

## On Windows

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

Use the setup tool to update Gloam. You can also replace **BOOT.NDS** yourself;
check the release notes in case other files need updating too.

[Next: add your files and start playing →](USING.md)
