# SPDX-License-Identifier: GPL-3.0-or-later
# Settings-page bindings. Loaded into the main WPF script after controls exist.
$settingCombos = @(
    @{ Control='SettingMode'; Key='settings_mode'; Labels=@('Simple','Advanced'); Values=@('simple','advanced') },
    @{ Control='SettingView'; Key='browser_style'; Labels=@('Classic','Extended','List (no icons)','Grid','Cover shelf','Library index'); Values=@('0','1','2','3','4','5') },
    @{ Control='SettingIcons'; Key='icon_size'; Labels=@('Small','Large'); Values=@('0','1') },
    @{ Control='SettingSort'; Key='sort_mode'; Labels=@('Name A-Z','Name Z-A','Type','Newest'); Values=@('0','1','2','3') },
    @{ Control='SettingSpeed'; Key='scroll_speed'; Labels=@('Slow','Normal','Fast'); Values=@('0','1','2') },
    @{ Control='SettingLanguage'; Key='title_language'; Labels=@('Japanese','English','French','German','Italian','Spanish','Chinese','Korean'); Values=@('0','1','2','3','4','5','6','7') },
    @{ Control='SettingMusicMode'; Key='music_mode'; Labels=@('Off','Theme music','Custom WAV'); Values=@('0','1','2') },
    @{ Control='SettingAutoLaunch'; Key='auto_launch_delay'; Labels=@('Off','3 seconds','5 seconds','10 seconds'); Values=@('0','3','5','10') }
)
$settingChecks = [ordered]@{ SimpleLock='simple_settings_lock'; SettingCovers='show_covers'; SettingMotion='reduced_motion';
    SettingUpdates='background_updates'; SettingClock='clock_12_hour'; SettingHideSaves='hide_save_folders'; SettingHideBios='hide_bios_folders';
    SettingHidden='show_hidden'; SettingSystem='show_system'; SettingExtensions='show_extensions';
    SettingKnown='filter_recognized'; SettingTitles='internal_titles'; SettingSounds='theme_sounds' }
$script:SettingsSnapshot = $null; $script:LoadingSettings = $false; $script:SettingsDirty = $false
foreach ($item in $settingCombos) { $ui[$item.Control].ItemsSource = $item.Labels }
$ui.SettingAccent.ItemsSource = @('Theme accent','Amber','Mint','Iris','Rose','Coral','Sky','Cyan','Lime','Gold','Peach','Violet','Pink','Teal','Blue','Slate','Silver')
$ui.SetupPreset.ItemsSource = @('Everyday','Simple locked','Quiet','Large icons'); $ui.SetupPreset.SelectedIndex = 0

function Set-GloamSettingsDirty {
    if (-not $script:LoadingSettings -and $null -ne $script:SettingsSnapshot) {
        $script:SettingsDirty = $true; $ui.SettingsInfo.Text = 'Unsaved settings. Save settings applies them to the SD card.'
    }
}

function Set-GloamSettingsUiValues($Values) {
    foreach ($item in $settingCombos) {
        if ($Values.Contains($item.Key)) { $ui[$item.Control].SelectedIndex = [Array]::IndexOf($item.Values, [string]$Values[$item.Key]) }
    }
    foreach ($control in $settingChecks.Keys) {
        if ($Values.Contains($settingChecks[$control])) { $ui[$control].IsChecked = $Values[$settingChecks[$control]] -eq '1' }
    }
    foreach ($item in @(@('SettingMusicVolume','music_volume'),@('SettingEffectsVolume','effects_volume'))) {
        if ($Values.Contains($item[1])) { $ui[$item[0]].Value = [double]$Values[$item[1]] }
    }
    if ($Values.Contains('music_path')) { $ui.SettingMusicPath.Text = $Values['music_path'] }
    if ($Values.Contains('theme_id')) { $ui.SettingTheme.SelectedValue = $Values['theme_id'] }
    if ($Values.Contains('startup_tab')) { $ui.SettingStartup.SelectedValue = $Values['startup_tab'] }
    if ($Values.Contains('theme_accent_override')) {
        $index = if ($Values['theme_accent_override'] -eq '1' -and $Values.Contains('theme')) { [int]$Values['theme'] + 1 } else { 0 }
        $ui.SettingAccent.SelectedIndex = $index
    }
    if ($ui.SimpleLock.IsChecked) { $ui.SettingMode.SelectedIndex = 0 }
    $ui.SettingMode.IsEnabled = -not $ui.SimpleLock.IsChecked
}

function Load-GloamSettingsUi {
    $script:LoadingSettings = $true
    try {
        Assert-GloamConfigurationReady $script:Root
        $snapshot = Read-GloamState $script:Root
        $themes = @([pscustomobject]@{ Id=''; Name='Built-in' })
        $folder = Get-GloamPath $script:Root '_nds/dsi-frontend/themes'
        if (Test-Path -LiteralPath $folder -PathType Container) {
            foreach ($theme in Get-ChildItem -LiteralPath $folder -Directory | Sort-Object Name) {
                if ($theme.Name -notmatch '^[a-zA-Z0-9_-]{1,63}$') { continue }
                $file = Get-GloamPath $script:Root ('_nds/dsi-frontend/themes/' + $theme.Name + '/theme.ini')
                if (Test-Path -LiteralPath $file -PathType Leaf) { $themes += [pscustomobject]@{ Id=$theme.Name; Name=$theme.Name } }
            }
        }
        if ($snapshot.Values.theme_id -notin @($themes | ForEach-Object Id)) { $themes += [pscustomobject]@{ Id=$snapshot.Values.theme_id; Name=('Missing: ' + $snapshot.Values.theme_id) } }
        $ui.SettingTheme.ItemsSource = $themes
        $tabs = @([pscustomobject]@{ Id=''; Label='Last tab' })
        foreach ($id in @('home','games','apps','media','system','files')) { $tabs += [pscustomobject]@{ Id=$id; Label=$id.Substring(0,1).ToUpperInvariant() + $id.Substring(1) } }
        foreach ($tab in $ui.Tabs.ItemsSource) { if ($tab.Id) { $tabs += [pscustomobject]@{ Id=$tab.Id; Label=$tab.Label } } }
        if ($snapshot.Values.startup_tab -notin @($tabs | ForEach-Object Id)) { $tabs += [pscustomobject]@{ Id=$snapshot.Values.startup_tab; Label=('Missing: ' + $snapshot.Values.startup_tab) } }
        $ui.SettingStartup.ItemsSource = $tabs
        $script:SettingsSnapshot = $snapshot
        Set-GloamSettingsUiValues $snapshot.Values
        $ui.SettingsForm.IsEnabled = $true; $ui.SaveSettings.IsEnabled = $true
        $ui.SettingsInfo.Text = if ($snapshot.Issues.Count) { 'Invalid values use defaults for display: ' + ($snapshot.Issues -join ', ') } else { 'Loaded from SD. Unedited records, favourites and saved folder positions are retained.' }
        $script:SettingsDirty = $false
    } catch {
        $script:SettingsSnapshot = $null; $ui.SettingsForm.IsEnabled = $false; $ui.SaveSettings.IsEnabled = $false
        $ui.SettingsInfo.Text = $_.Exception.Message; Write-Activity ('Settings: ' + $_.Exception.Message)
    } finally { $script:LoadingSettings = $false }
}

function Get-GloamSettingsUiChanges {
    if ($null -eq $script:SettingsSnapshot) { throw 'Load valid launcher settings first.' }
    $values = [ordered]@{}
    foreach ($item in $settingCombos) {
        $index = $ui[$item.Control].SelectedIndex
        if ($index -lt 0) { throw 'Choose a value for ' + $item.Key }
        $values[$item.Key] = $item.Values[$index]
    }
    foreach ($control in $settingChecks.Keys) { $values[$settingChecks[$control]] = [string][int][bool]$ui[$control].IsChecked }
    $values['theme_id'] = [string]$ui.SettingTheme.SelectedValue; $values['startup_tab'] = [string]$ui.SettingStartup.SelectedValue
    $values['music_volume'] = [string][int][Math]::Round($ui.SettingMusicVolume.Value)
    $values['effects_volume'] = [string][int][Math]::Round($ui.SettingEffectsVolume.Value)
    $values['music_path'] = $ui.SettingMusicPath.Text
    $values['theme_accent_override'] = if ($ui.SettingAccent.SelectedIndex -gt 0) { '1' } else { '0' }
    if ($ui.SettingAccent.SelectedIndex -gt 0) { $values['theme'] = [string]($ui.SettingAccent.SelectedIndex - 1) }
    $changes = [ordered]@{}
    foreach ($key in $values.Keys) { if ([string]$values[$key] -cne [string]$script:SettingsSnapshot.Values[$key]) { $changes[$key] = $values[$key] } }
    return $changes
}

foreach ($item in $settingCombos) { $ui[$item.Control].Add_SelectionChanged({ Set-GloamSettingsDirty }) }
foreach ($control in $settingChecks.Keys) { $ui[$control].Add_Checked({ Set-GloamSettingsDirty }); $ui[$control].Add_Unchecked({ Set-GloamSettingsDirty }) }
foreach ($control in @('SettingTheme','SettingStartup','SettingAccent')) { $ui[$control].Add_SelectionChanged({ Set-GloamSettingsDirty }) }
$ui.SettingMusicVolume.Add_ValueChanged({ Set-GloamSettingsDirty }); $ui.SettingEffectsVolume.Add_ValueChanged({ Set-GloamSettingsDirty })
$ui.SettingMusicPath.Add_TextChanged({ Set-GloamSettingsDirty })
$ui.SimpleLock.Add_Checked({ $ui.SettingMode.SelectedIndex = 0; $ui.SettingMode.IsEnabled = $false })
$ui.SimpleLock.Add_Unchecked({ $ui.SettingMode.IsEnabled = $true })
$ui.StagePreset.Add_Click({ Invoke-UiAction {
    if ($null -eq $script:SettingsSnapshot) { throw 'Load valid launcher settings first.' }
    Set-GloamSettingsUiValues (Get-GloamSetupPreset ([string]$ui.SetupPreset.SelectedItem))
    Set-GloamSettingsDirty
} })
$ui.SaveSettings.Add_Click({ Invoke-UiAction {
    Save-GloamSettings (Get-SelectedRoot) (Get-GloamSettingsUiChanges) $script:SettingsSnapshot.Hash
    Load-GloamSettingsUi; Write-Activity 'Launcher settings saved. Restart Gloam to reload them.'
} })
$ui.ReloadSettings.Add_Click({ Invoke-UiAction { Get-SelectedRoot | Out-Null; Load-GloamSettingsUi } })
$ui.PickMusic.Add_Click({ Invoke-UiAction {
    $root = Get-SelectedRoot; $dialog = [Windows.Forms.OpenFileDialog]::new(); $dialog.Filter = 'Background music WAV|*.wav'; $dialog.InitialDirectory = $root
    if ($dialog.ShowDialog() -eq 'OK') {
        Test-GloamMusic $dialog.FileName
        if ($dialog.FileName.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { $relative = ConvertTo-GloamRelative $root $dialog.FileName }
        else {
            $relative = 'media/music/' + [IO.Path]::GetFileName($dialog.FileName)
            $target = Get-GloamPath $root $relative
            if ((Test-Path -LiteralPath $target) -and (Get-GloamHash $target) -ne (Get-GloamHash $dialog.FileName)) { throw 'A different music file already has that name. Rename the PC file first.' }
            Copy-GloamContent $root @($dialog.FileName) 'media/music'
        }
        $ui.SettingMusicPath.Text = 'sd:/' + $relative; $ui.SettingMusicMode.SelectedIndex = 2
    }; $dialog.Dispose()
} })
$ui.ApplyTheme.Add_Click({ Invoke-UiAction {
    $root = Get-SelectedRoot
    if ($script:SettingsDirty) { throw 'Save or reload the pending settings before applying a theme.' }
    if ($null -eq $script:SettingsSnapshot -or $null -eq $ui.ThemeList.SelectedItem) { throw 'Choose an installed theme first.' }
    $changes = Get-GloamThemeSettings $root ([string]$ui.ThemeList.SelectedItem) ([bool]$ui.ThemeUseLayout.IsChecked)
    Save-GloamSettings $root $changes $script:SettingsSnapshot.Hash
    Load-GloamSettingsUi; Write-Activity 'Theme selection saved. Restart Gloam to load and validate the package.'
} })
$ui.CheckSetup.Add_Click({ Invoke-UiAction { Start-SetupJob 'CheckSetup' } })
$ui.OpenConfig.Add_Click({ Invoke-UiAction { Open-SdFolder '_nds/dsi-frontend' } })
$ui.MaintenanceBackups.Add_Click({ Invoke-UiAction { Open-SdFolder '_nds/dsi-frontend/setup-backups' } })
$ui.ExportConfiguration.Add_Click({ Invoke-UiAction {
    Get-SelectedRoot | Out-Null
    if ($script:SettingsDirty) { throw 'Save or reload pending settings before exporting a configuration.' }
    $dialog = [Windows.Forms.SaveFileDialog]::new(); $dialog.Filter = 'Gloam portable configuration|*.dsconfig'; $dialog.FileName = 'gloam-configuration.dsconfig'
    if ($dialog.ShowDialog() -eq 'OK') { Start-SetupJob 'ExportConfiguration' @() @{ Path=$dialog.FileName } }; $dialog.Dispose()
} })
$ui.ImportConfiguration.Add_Click({ Invoke-UiAction {
    Get-SelectedRoot | Out-Null
    if ($script:SettingsDirty) { throw 'Save or reload pending settings before importing a configuration.' }
    $dialog = [Windows.Forms.OpenFileDialog]::new(); $dialog.Filter = 'Gloam portable configuration|*.dsconfig'
    if ($dialog.ShowDialog() -eq 'OK') {
        $hash = Get-GloamHash $dialog.FileName; $records = Read-GloamConfigurationArchive $dialog.FileName
        $message = 'Import ' + $records.Count + " validated configuration records?`nCurrent files receive backups. Applications, themes, BIOS and saves stay in place."
        if ([Windows.MessageBox]::Show($window,$message,'Import configuration','YesNo','Question') -eq 'Yes') {
            Start-SetupJob 'ImportConfiguration' @() @{ Path=$dialog.FileName; Hash=$hash }
        }
    }; $dialog.Dispose()
} })

$ui.RestoreConsoleSettings.Add_Click({ Invoke-UiAction {
    $root = Get-SelectedRoot
    $message = 'Restore the previous console settings from state.bak? The current file receives a backup.'
    if ([Windows.MessageBox]::Show($window,$message,'Restore console settings','YesNo','Question') -eq 'Yes') {
        Restore-GloamStateBackup $root; Load-GloamSettingsUi; Write-Activity 'Previous console settings restored.'
    }
} })
