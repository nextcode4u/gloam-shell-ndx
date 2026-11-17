"""Native PowerShell catalog/install fixtures; no network or physical SD required."""

import base64
import copy
import hashlib
import io
import json
from pathlib import Path
import unittest
from PIL import Image
import test_gloam_setup_windows as setup_fixtures

AVAILABLE, win, literal = setup_fixtures.AVAILABLE, setup_fixtures.win, setup_fixtures.literal


def blob(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


@unittest.skipUnless(AVAILABLE, "Requires native Windows PowerShell")
class ThemeDirectoryTests(unittest.TestCase):
    def setUp(self):
        self.fixture = setup_fixtures.GloamSetupTests("runTest")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root, self.sd = self.fixture.root, self.fixture.sd
        self.cache = self.root / "catalog-cache"
        self.cache.mkdir()
        self.fixture.preamble += (
            ". "
            + literal(win(self.fixture.installer / "gloam-setup-themes.ps1"))
            + "; $cache="
            + literal(win(self.cache))
            + "; "
        )
        self.run_ps = self.fixture.run_ps
        image = io.BytesIO()
        Image.new("RGB", (256, 396), "#7865ab").save(image, "PNG")
        self.contents = {
            "themes/demo/theme.ini": b"[theme]\nversion=2\nname=Fixture\n[layout]\ntop_layout=gallery\n",
            "themes/demo/CREDITS.md": b"Authored inert fixture credits.",
            "LICENSE": b"Authored inert license fixture.",
            "LICENSES.md": b"Authored inert notice fixture.",
            "themes/demo/preview.png": image.getvalue(),
        }
        files = []
        for path, data in self.contents.items():
            if path.endswith("preview.png"):
                continue
            relative = {
                "LICENSE": "licenses/theme-directory-GPL-3.0.txt",
                "LICENSES.md": "licenses/theme-directory-credits.md",
            }.get(path, path.removeprefix("themes/demo/"))
            files.append(dict(Path=path, Relative=relative, Sha=blob(data), Bytes=len(data)))
        data = self.contents["themes/demo/preview.png"]
        self.theme = dict(
            Id="demo",
            Name="Demo",
            Author="Fixture author",
            Description="Synthetic theme",
            License="GPL-3.0-or-later; CC0-1.0",
            Requirements="Gloam format 2",
            Preview=dict(Path="themes/demo/preview.png", Sha=blob(data), Bytes=len(data)),
            Files=files,
        )
        self.catalog = dict(Format=1, Revision="a" * 40, Themes=[self.theme])
        self.save_catalog()
        for data in self.contents.values():
            target = self.cache / "blobs" / blob(data)
            target.parent.mkdir(exist_ok=True)
            target.write_bytes(data)

    def save_catalog(self):
        (self.cache / "catalog.json").write_text(json.dumps(self.catalog))

    def install(self, replace=False, nc=False):
        self.run_ps(
            "Install-GloamCatalogTheme $root $cache demo demo $"
            + str(replace).lower()
            + " $"
            + str(nc).lower()
            + " $true"
        )

    def test_gui_cached_preview_uses_worker_and_does_not_write_sd(self):
        gui = literal(win(self.fixture.installer / "gloam-setup.ps1"))
        output = self.run_ps(
            "& "
            + gui
            + " -Target $root -Offline -ThemeDirectoryCache $cache -SmokeTest -ScreenshotPage Themes -SmokeAction PreviewCatalogTheme"
        )
        self.assertIn("GUI worker exit: 0", output)
        self.assertIn("GUI theme preview loaded: 256x396", output)
        self.assertEqual(list(self.sd.iterdir()), [])

    def test_large_preview_close_and_escape_return_to_directory(self):
        markup = literal(win(self.fixture.installer / "gloam-setup.xaml"))
        helpers = literal(win(self.fixture.installer / "gloam-setup-themes-ui.ps1"))
        self.run_ps(
            "Add-Type -AssemblyName PresentationFramework; Add-Type -AssemblyName PresentationCore; Add-Type -AssemblyName WindowsBase; "
            + "[xml]$markup=Get-Content -Raw "
            + markup
            + "; $window=[Windows.Markup.XamlReader]::Load([Xml.XmlNodeReader]::new($markup)); $ui=@{}; "
            + "foreach ($node in $markup.SelectNodes('//*[@*[local-name()=\"Name\"]]')) { $n=$node.GetAttribute('Name','http://schemas.microsoft.com/winfx/2006/xaml'); $ui[$n]=$window.FindName($n) }; "
            + "$script:Root=$root; $ThemeDirectoryCache=$cache; $SmokeTest=$false; function Invoke-UiAction([scriptblock]$Action) { & $Action }; . "
            + helpers
            + "; "
            + "$ui.ThemeCatalogList.SelectedIndex=0; $window.Show(); "
            + "try { Open-GloamLargeThemePreview; $preview=$script:ThemePreviewWindow; if (-not $preview.IsVisible -or -not $window.IsVisible) { throw 'preview did not open modelessly' }; "
            + "$preview.Content.Children[0].RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent)); if ($preview.IsVisible -or -not $window.IsVisible) { throw 'close did not return to directory' }; "
            + "Open-GloamLargeThemePreview; $preview=$script:ThemePreviewWindow; $event=[Windows.Input.KeyEventArgs]::new([Windows.Input.Keyboard]::PrimaryDevice,[Windows.PresentationSource]::FromVisual($preview),0,[Windows.Input.Key]::Escape); $event.RoutedEvent=[Windows.Input.Keyboard]::KeyDownEvent; $preview.RaiseEvent($event); if ($preview.IsVisible) { throw 'Escape did not close preview' } "
            + "} finally { if ($script:ThemePreviewWindow) { $script:ThemePreviewWindow.Close() }; $window.Close() }"
        )
        self.assertEqual(list(self.sd.iterdir()), [])

    def test_offline_preview_never_touches_sd_and_install_retains_notices(self):
        self.run_ps(
            "function Get-GloamThemeNetwork { throw 'Network must not be used' }; Get-GloamCatalogThemePreview $cache demo $true | Out-Null"
        )
        self.assertEqual(list(self.sd.iterdir()), [])
        self.install()
        destination = self.sd / "_nds/dsi-frontend/themes/demo"
        self.assertTrue((destination / "theme.ini").exists())
        self.assertTrue((destination / "licenses/theme-directory-GPL-3.0.txt").exists())
        self.assertFalse((destination / "preview.png").exists())
        self.assertFalse((self.sd / "BOOT.NDS").exists())

    def test_existing_theme_is_preserved_until_replacement_is_explicit(self):
        self.install()
        target = self.sd / "_nds/dsi-frontend/themes/demo/theme.ini"
        target.write_text("User customization")
        self.run_ps(
            "Install-GloamCatalogTheme $root $cache demo demo $false $false $true", success=False
        )
        self.assertEqual(target.read_text(), "User customization")
        self.install(replace=True)
        backups = list(
            (self.sd / "_nds/dsi-frontend/setup-backups/themes").glob("*/demo/theme.ini")
        )
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_text(), "User customization")
        self.assertNotEqual(target.read_text(), "User customization")

    def test_bad_hash_and_bad_manifest_leave_no_partial_install(self):
        entry = self.theme["Files"][0]
        cached = self.cache / "blobs" / entry["Sha"]
        cached.write_bytes(b"broken cache")
        self.run_ps(
            "Install-GloamCatalogTheme $root $cache demo demo $false $false $true", success=False
        )
        self.assertEqual(list(self.sd.iterdir()), [])
        invalid = b"[theme]\nversion=99\n"
        cached.unlink()
        entry.update(Sha=blob(invalid), Bytes=len(invalid))
        (self.cache / "blobs" / blob(invalid)).write_bytes(invalid)
        self.save_catalog()
        self.run_ps(
            "Install-GloamCatalogTheme $root $cache demo demo $false $false $true", success=False
        )
        self.assertFalse((self.sd / "_nds/dsi-frontend/themes/demo").exists())
        self.assertEqual(list((self.sd / "_nds/dsi-frontend").glob(".theme-install-*")), [])

    def test_cancellation_cleans_staging_and_keeps_existing_theme(self):
        destination = self.sd / "_nds/dsi-frontend/themes/demo"
        destination.mkdir(parents=True)
        (destination / "custom.txt").write_text("Keep this")
        self.run_ps(
            "$script:checks=0; function Assert-GloamThemeNotCancelled { $script:checks++; if ($script:checks -eq 3) { throw [OperationCanceledException]::new('fixture cancel') } }; Install-GloamCatalogTheme $root $cache demo demo $true $false $true",
            success=False,
        )
        self.assertEqual((destination / "custom.txt").read_text(), "Keep this")
        self.assertEqual(list((self.sd / "_nds/dsi-frontend").glob(".theme-install-*")), [])

    def test_noncommercial_acceptance_and_alternate_install_id(self):
        self.theme["License"] = "CC-BY-NC-SA-2.5"
        self.save_catalog()
        self.run_ps(
            "Install-GloamCatalogTheme $root $cache demo demo $false $false $true", success=False
        )
        self.assertEqual(list(self.sd.iterdir()), [])
        self.run_ps("Install-GloamCatalogTheme $root $cache demo demo-copy $false $true $true")
        self.assertTrue((self.sd / "_nds/dsi-frontend/themes/demo-copy/theme.ini").exists())

    def test_catalog_rejects_unsafe_files_collisions_and_missing_notices(self):
        original = copy.deepcopy(self.catalog)
        mutations = [
            ("Relative", "../escape.ini"),
            ("Path", "themes/other/theme.ini"),
            ("Sha", "bad"),
            ("Relative", "run.ps1"),
        ]
        for key, value in mutations:
            with self.subTest(key=key, value=value):
                self.catalog = copy.deepcopy(original)
                self.catalog["Themes"][0]["Files"][0][key] = value
                self.save_catalog()
                self.run_ps("Read-GloamThemeCatalog $cache | Out-Null", success=False)
        self.catalog = copy.deepcopy(original)
        self.catalog["Themes"][0]["Files"].append(copy.deepcopy(self.theme["Files"][0]))
        self.save_catalog()
        self.run_ps("Read-GloamThemeCatalog $cache | Out-Null", success=False)
        self.catalog = copy.deepcopy(original)
        self.catalog["Themes"][0]["Files"] = self.catalog["Themes"][0]["Files"][:2]
        self.save_catalog()
        self.run_ps("Read-GloamThemeCatalog $cache | Out-Null", success=False)
        self.assertEqual(list(self.sd.iterdir()), [])

    def test_missing_preview_does_not_prevent_installing(self):
        self.theme["Preview"] = None
        self.save_catalog()
        self.run_ps("Get-GloamCatalogThemePreview $cache demo $true", success=False)
        self.install()

    def test_online_refresh_pins_catalog_tree_and_preview_to_one_revision(self):
        source = dict(
            format_version=1,
            themes=[
                dict(
                    id="demo",
                    path="themes/demo",
                    name="Demo",
                    author="Fixture author",
                    description="Synthetic theme",
                    configuration_license="GPL-3.0-or-later",
                    asset_license="CC0-1.0",
                    requires_gloam="Gloam format 2",
                )
            ],
        )
        contents = self.contents | {"catalog.json": json.dumps(source).encode()}
        tree = dict(
            truncated=False,
            tree=[
                dict(path=path, type="blob", mode="100644", sha=blob(data), size=len(data))
                for path, data in contents.items()
            ],
        )
        repo = "https://api.github.com/repos/nextcode4u/gloam-shell-theme-directory"
        revision = "b" * 40
        replies = {
            repo + "/commits/main": json.dumps(dict(sha=revision)).encode(),
            repo + "/git/trees/" + revision + "?recursive=1": json.dumps(tree).encode(),
        }
        replies.update(
            {
                "https://raw.githubusercontent.com/nextcode4u/gloam-shell-theme-directory/"
                + revision
                + "/"
                + path: data
                for path, data in contents.items()
            }
        )
        lookup = self.root / "replies.json"
        lookup.write_text(
            json.dumps({key: base64.b64encode(value).decode() for key, value in replies.items()})
        )
        self.run_ps(
            "$script:replies=Get-Content -Raw "
            + literal(win(lookup))
            + " | ConvertFrom-Json; function Get-GloamThemeNetwork([string]$Url,[long]$Limit) { $v=$script:replies.PSObject.Properties[$Url]; if (-not $v) { throw ('Unexpected URL '+$Url) }; return ,([Convert]::FromBase64String($v.Value)) }; $c=Update-GloamThemeCatalog $cache; if ($c.Revision -cne '"
            + revision
            + "') { throw 'wrong revision' }"
        )
        self.assertEqual(
            json.loads((self.cache / "catalog.json").read_text())["Revision"], revision
        )
        self.assertEqual(list(self.sd.iterdir()), [])
