# SPDX-License-Identifier: GPL-3.0-or-later
$script:ThemeCache = $ThemeDirectoryCache
$script:ThemeDirectory = $null
$script:ThemePreviewWindow = $null
function Update-GloamCatalogThemeStatus {
    if (-not $script:ThemeDirectory) { return }
    foreach ($item in $ui.ThemeCatalogList.ItemsSource) {
        $item.Installation = 'Not installed'
        if ($script:Root) {
            $path = Get-GloamPath $script:Root ('_nds/dsi-frontend/themes/' + $item.Id)
            if ([IO.File]::Exists((Join-Path $path 'theme.ini'))) { $item.Installation = 'Installed' }
            elseif ([IO.Directory]::Exists($path)) { $item.Installation = 'Folder exists' }
        }
    }
    $ui.ThemeCatalogList.Items.Refresh()
}
function Show-GloamCatalogPreview([string]$Path) {
    $ui.CatalogThemePreview.Source = $null
    $stream = [IO.File]::OpenRead($Path)
    try {
        $image = [Windows.Media.Imaging.BitmapImage]::new(); $image.BeginInit()
        $image.CacheOption = [Windows.Media.Imaging.BitmapCacheOption]::OnLoad
        $image.StreamSource = $stream; $image.EndInit(); $image.Freeze()
        $ui.CatalogThemePreview.Source = $image
        $ui.EnlargeThemePreview.IsEnabled = $true
        [Windows.Media.RenderOptions]::SetBitmapScalingMode($ui.CatalogThemePreview, [Windows.Media.BitmapScalingMode]::NearestNeighbor)
    } finally { $stream.Dispose() }
}
function Show-GloamCatalogSelection {
    $ui.CatalogThemePreview.Source = $null
    $ui.EnlargeThemePreview.IsEnabled = $false
    $item = $ui.ThemeCatalogList.SelectedItem
    if (-not $item) { return }
    $ui.CatalogThemeId.Text = $item.Id
    $ui.ReplaceCatalogTheme.IsChecked = $false
    $ui.ThemeCatalogDetails.Text = $item.Name + "`nBy " + $item.Author + "`n`n" + $item.Description + "`n`nRequirements: " + $item.Requirements + "`n`nTerms: " + $item.License
    $ui.ThemePreviewStatus.Text = 'Click Preview selected theme to fetch only preview.png. Nothing is installed.'
    if ($null -eq $item.Catalog.Preview) { $ui.ThemePreviewStatus.Text = 'No preview.png supplied by this contributor. Installation is still available.'; return }
    try {
        $path = Get-GloamCatalogThemePreview $script:ThemeCache $item.Id $true
        Show-GloamCatalogPreview $path
        $ui.ThemePreviewStatus.Text = 'Cached contributor preview. No SD files changed.'
    } catch { } # An uncached preview is fetched only on the explicit Preview action.
}
function Load-GloamThemeDirectory {
    try { $script:ThemeDirectory = Read-GloamThemeCatalog $script:ThemeCache }
    catch { $ui.ThemeCatalogSummary.Text = $_.Exception.Message; return }
    $selected = if ($ui.ThemeCatalogList.SelectedItem) { $ui.ThemeCatalogList.SelectedItem.Id } else { '' }
    $ui.ThemeCatalogList.ItemsSource = @($script:ThemeDirectory.Themes | ForEach-Object {
        [pscustomobject]@{ Id=$_.Id; Name=$_.Name; Author=$_.Author; Description=$_.Description; License=$_.License; Requirements=$_.Requirements; Catalog=$_; Installation='' }
    })
    Update-GloamCatalogThemeStatus
    $ui.ThemeCatalogSummary.Text = @($script:ThemeDirectory.Themes).Count.ToString() + ' themes from the community directory; revision ' + $script:ThemeDirectory.Revision.Substring(0, 12) + '. Preview downloads use the PC cache only.'
    if ($selected) { $ui.ThemeCatalogList.SelectedItem = @($ui.ThemeCatalogList.ItemsSource | Where-Object Id -CEQ $selected)[0] }
}
function Complete-GloamThemeJob($Job) {
    if ($Job.Action -eq 'ThemeCatalog') { Load-GloamThemeDirectory; $ui.Status.Text = 'Theme directory refreshed. No SD files changed.' }
    elseif ($Job.Action -eq 'ThemePreview') {
        $result = Get-Content -LiteralPath (Join-Path $Job.Folder 'theme-preview.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($ui.ThemeCatalogList.SelectedItem -and $ui.ThemeCatalogList.SelectedItem.Id -ceq $result.Id) {
            Show-GloamCatalogPreview $result.Path
            if ($SmokeTest) { Write-Host ('GUI theme preview loaded: ' + $ui.CatalogThemePreview.Source.PixelWidth + 'x' + $ui.CatalogThemePreview.Source.PixelHeight) }
            $ui.ThemePreviewStatus.Text = 'Downloaded contributor preview. No theme installed; no SD files changed.'
        }
        $ui.Status.Text = 'Preview ready in the PC cache. Select Install when you want the theme on your SD.'
    } elseif ($Job.Action -eq 'ThemeInstall') {
        Update-GloamCatalogThemeStatus
        $ui.Status.Text = 'Theme installed. Select it in Gloam settings, or apply it from the installed list below.'
    }
}
function Open-GloamLargeThemePreview {
    if (-not $ui.CatalogThemePreview.Source) { return }
    if ($script:ThemePreviewWindow) { $script:ThemePreviewWindow.Close() }
    $dialog = [Windows.Window]::new(); $dialog.Owner = $window
    $dialog.Title = 'Theme preview - ' + $ui.ThemeCatalogList.SelectedItem.Name
    $dialog.Background = [Windows.Media.Brushes]::Black
    $dialog.Width = [Math]::Min(460, [Windows.SystemParameters]::WorkArea.Width - 40)
    $dialog.Height = [Math]::Min(700, [Windows.SystemParameters]::WorkArea.Height - 40)
    $dialog.WindowStartupLocation = 'CenterOwner'
    $layout = [Windows.Controls.DockPanel]::new()
    $close = [Windows.Controls.Button]::new(); $close.Content = 'Close preview / return (Esc)'; $close.Margin = [Windows.Thickness]::new(12)
    $close.Add_Click({ $dialog.Close() }.GetNewClosure())
    [Windows.Controls.DockPanel]::SetDock($close, 'Bottom'); $layout.Children.Add($close) | Out-Null
    $image = [Windows.Controls.Image]::new(); $image.Source = $ui.CatalogThemePreview.Source
    $image.Stretch = 'Uniform'; $image.Margin = [Windows.Thickness]::new(12)
    [Windows.Media.RenderOptions]::SetBitmapScalingMode($image, [Windows.Media.BitmapScalingMode]::NearestNeighbor)
    $layout.Children.Add($image) | Out-Null; $dialog.Content = $layout
    $dialog.Add_KeyDown({ param($sender, $eventArgs) if ($eventArgs.Key -eq 'Escape') { $dialog.Close(); $eventArgs.Handled = $true } }.GetNewClosure())
    $script:ThemePreviewWindow = $dialog
    $dialog.Show() # Modeless: the directory remains usable while comparing a preview.
}
$ui.EnlargeThemePreview.Add_Click({ Invoke-UiAction { Open-GloamLargeThemePreview } })
$ui.RefreshThemeCatalog.Add_Click({ Invoke-UiAction { Start-SetupJob 'ThemeCatalog' @() @{ Cache=$script:ThemeCache } } })
$ui.ThemeCatalogList.Add_SelectionChanged({ Invoke-UiAction { Show-GloamCatalogSelection } })
$ui.PreviewCatalogTheme.Add_Click({ Invoke-UiAction {
    $item = $ui.ThemeCatalogList.SelectedItem
    if (-not $item) { throw 'Select a theme in the directory first.' }
    Start-SetupJob 'ThemePreview' @() @{ Cache=$script:ThemeCache; Id=$item.Id }
} })
$ui.InstallCatalogTheme.Add_Click({ Invoke-UiAction {
    $item = $ui.ThemeCatalogList.SelectedItem
    if (-not $item) { throw 'Select a theme in the directory first.' }
    $id = $ui.CatalogThemeId.Text.Trim()
    if ($id -cnotmatch '^[a-zA-Z0-9_-]{1,63}$') { throw 'Use a theme folder ID with up to 63 letters, digits, hyphens or underscores.' }
    if ($item.License -match '(?i)(non.?commercial|BY-NC|NC-SA)' -and -not $ui.AllowNcThemes.IsChecked) { throw 'Read and accept the noncommercial theme terms before installing.' }
    if ($ui.ReplaceCatalogTheme.IsChecked -and [Windows.MessageBox]::Show($window, 'Replace the existing theme folder? The previous folder will be kept under setup-backups/themes. Settings and other themes stay in place.', 'Replace theme', 'OKCancel', 'Question') -ne 'OK') { return }
    Start-SetupJob 'ThemeInstall' @() @{ Cache=$script:ThemeCache; Id=$item.Id; InstallId=$id; Replace=[bool]$ui.ReplaceCatalogTheme.IsChecked; AllowNoncommercial=[bool]$ui.AllowNcThemes.IsChecked }
} })
$ui.OpenThemeDirectory.Add_Click({ Start-Process 'https://github.com/nextcode4u/gloam-shell-theme-directory' })
Load-GloamThemeDirectory
