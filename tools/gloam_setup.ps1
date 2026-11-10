# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [string]$Target,
    [string]$Manifest,
    [string]$JobDirectory = (Join-Path $env:LOCALAPPDATA 'Gloam/Setup/jobs'),
    [string]$ReleaseDirectory = (Join-Path $env:LOCALAPPDATA 'Gloam/Setup/releases/current'),
    [switch]$Offline,
    [switch]$SmokeTest,
    [ValidateSet('Setup','Addons','Library','Themes')][string]$ScreenshotPage = 'Setup',
    [ValidateSet('','CheckSupport')][string]$SmokeAction,
    [string]$ScreenshotPath
)
$ErrorActionPreference = 'Stop'
if (-not $Manifest) { $Manifest = Join-Path $PSScriptRoot 'downloads.json' }
if ([Threading.Thread]::CurrentThread.ApartmentState -ne 'STA') { throw 'Start with Windows PowerShell -STA, or use Start Gloam Setup.cmd.' }
. (Join-Path $PSScriptRoot 'gloam-setup-core.ps1')
Add-Type -AssemblyName PresentationFramework
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase
Add-Type -AssemblyName System.Windows.Forms

[xml]$markup = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'gloam-setup.xaml') -Raw
$window = [Windows.Markup.XamlReader]::Load([Xml.XmlNodeReader]::new($markup))
$ui = @{}
foreach ($node in $markup.SelectNodes('//*[@*[local-name()="Name"]]')) {
    $ui[$node.GetAttribute('Name','http://schemas.microsoft.com/winfx/2006/xaml')] = $window.FindName($node.GetAttribute('Name','http://schemas.microsoft.com/winfx/2006/xaml'))
}
$script:Root = ''; $script:Job = $null; $script:LastLog = ''
$ui.Workspace.IsEnabled = $false
$recommended = @('gameyob-dsi','nesds','snemulds','snemulds-shim','dslibris','dsxp','s8ds','nitrografx','pokemini')

function Write-Activity([string]$Text) { $ui.Activity.AppendText($Text + "`r`n"); $ui.Activity.ScrollToEnd() }
function Invoke-UiAction([scriptblock]$Action, [object[]]$Arguments = @()) {
    try { & $Action @Arguments } catch { if ($SmokeTest) { throw }; Write-Activity $_.Exception.Message; $ui.Status.Text = 'Action could not be completed.'; [Windows.MessageBox]::Show($window, $_.Exception.Message, 'Gloam Setup', 'OK', 'Warning') | Out-Null }
}
function Get-SelectedRoot {
    $root = Get-GloamRoot $ui.TargetPath.Text
    if ($root -ne $script:Root) { throw 'Click Use folder before continuing.' }
    return $root
}
function Show-Page([string]$Name) {
    foreach ($page in @('Setup','Addons','Library','Themes')) {
        $ui['Page' + $page].Visibility = if ($page -eq $Name) { 'Visible' } else { 'Collapsed' }
        $ui['Nav' + $page].Background = if ($page -eq $Name) { '#544568' } else { '#28313F' }
    }
    $animation = [Windows.Media.Animation.DoubleAnimation]::new(0.65, 1, [Windows.Duration]::new([TimeSpan]::FromMilliseconds(120)))
    $ui.Workspace.BeginAnimation([Windows.UIElement]::OpacityProperty, $animation)
}
function Load-Sd {
    $script:Root = Get-GloamRoot $ui.TargetPath.Text; $ui.TargetPath.Text = $script:Root
    $profiles = @([pscustomobject]@{ Id = ''; Name = '+ New emulator / player'; Extensions = ''; Application = ''; DsApplication = ''; Folder = 'roms/New System'; Bootstrap = $false; Hbargv = $false }) + @(Get-GloamProfiles $script:Root)
    $ui.Profiles.ItemsSource = $profiles; $ui.Profiles.SelectedIndex = 0
    $data = Read-GloamIni (Get-GloamPath $script:Root '_nds/dsi-frontend/tabs.ini')
    $tabs = @([pscustomobject]@{ Id = ''; Label = '+ New folder tab'; Folder = 'roms/New System' })
    foreach ($key in $data.Keys) {
        if ($key.StartsWith('tab.')) { $tabs += [pscustomobject]@{ Id = $key.Substring(4); Label = $data[$key]['label']; Folder = $data[$key]['path'] } }
    }
    $ui.Tabs.ItemsSource = $tabs; $ui.Tabs.SelectedIndex = 0
    foreach ($pair in @(@('games','GamesRoot','roms'),@('apps','AppsRoot','apps'),@('media','MediaRoot','media'))) {
        $ui[$pair[1]].Text = if ($data.Contains('roots') -and $data['roots'][$pair[0]]) { $data['roots'][$pair[0]] } else { $pair[2] }
    }
    $folder = Get-GloamPath $script:Root '_nds/dsi-frontend/themes'; $ui.ThemeList.Items.Clear()
    if (Test-Path -LiteralPath $folder) { foreach ($theme in Get-ChildItem -LiteralPath $folder -Directory) { $ui.ThemeList.Items.Add($theme.Name) | Out-Null } }
    $ui.Workspace.IsEnabled = $true
    $ui.Status.Text = 'Ready: ' + $script:Root; Write-Activity ('Selected SD folder: ' + $script:Root)
}
function Open-SdFolder([string]$Relative) {
    $root = Get-SelectedRoot; $folder = if ($Relative) { Get-GloamPath $root $Relative } else { $root }
    [IO.Directory]::CreateDirectory($folder) | Out-Null
    Start-Process explorer.exe -ArgumentList ('"' + $folder + '"') | Out-Null
}
function Start-SetupJob([string]$Action, [string[]]$Sources = @()) {
    if ($null -ne $script:Job) { throw 'Wait for the current operation to finish.' }
    $root = Get-SelectedRoot
    $ui.AddonList.CommitEdit() | Out-Null
    $selected = @($ui.AddonList.ItemsSource | Where-Object Selected | ForEach-Object Id)
    if ($Action -eq 'CheckSupport') { $selected = @() } # Always inspect every installed app.
    if ($Action -in @('InstallAddons','UpdateAddons','CheckAddons') -and -not $selected.Count) { throw 'Select at least one app.' }
    $chosen = @($ui.AddonList.ItemsSource | Where-Object Selected)
    if ($Action -in @('InstallAddons','UpdateAddons') -and @($chosen | Where-Object Noncommercial).Count -and -not $ui.AllowNc.IsChecked) { throw 'Enable Allow noncommercial apps for the selected no-profit applications.' }
    $folder = Join-Path $JobDirectory ([Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($folder) | Out-Null
    $request = Join-Path $folder 'request.json'
    $record = @{ Action = $Action; Target = $root; Manifest = $Manifest; Cache = (Join-Path $env:USERPROFILE '.gloam-addons-cache');
        ReleaseDirectory = $ReleaseDirectory; Selected = $selected; Noncommercial = [bool]$ui.AllowNc.IsChecked;
        IncludeAddons = [bool]$ui.IncludeAddons.IsChecked; Offline = [bool]$Offline; Sources = $Sources; ContentFolder = $ui.ContentFolder.Text }
    [IO.File]::WriteAllText($request, ($record | ConvertTo-Json -Depth 10), [Text.UTF8Encoding]::new($false))
    $worker = Join-Path $PSScriptRoot 'gloam-setup-worker.ps1'
    $command = "[Console]::OutputEncoding = [Text.UTF8Encoding]::new(); & '" + $worker.Replace("'","''") + "' -Request '" + $request.Replace("'","''") + "'"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $process = Start-Process (Join-Path $PSHOME 'powershell.exe') -ArgumentList @('-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',$encoded) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $folder 'output.log') -RedirectStandardError (Join-Path $folder 'error.log')
    $null = $process.Handle # Keep a process handle so ExitCode remains available after exit.
    $script:Job = @{ Process = $process; Folder = $folder }; $script:LastLog = ''
    $ui.Workspace.IsEnabled = $false; $ui.TargetBar.IsEnabled = $false; $ui.Progress.Visibility = 'Visible'; $ui.Progress.IsIndeterminate = $true
    $ui.Status.Text = 'Working: ' + $Action; Write-Activity ('Started: ' + $Action)
}
$timer = [Windows.Threading.DispatcherTimer]::new(); $timer.Interval = [TimeSpan]::FromMilliseconds(250)
$timer.Add_Tick({
    if ($null -eq $script:Job) { return }
    try {
        $text = ''
        foreach ($name in @('output.log','error.log')) {
            $file = Join-Path $script:Job.Folder $name
            if (Test-Path -LiteralPath $file) { $text += [IO.File]::ReadAllText($file) }
        }
        if ($text -ne $script:LastLog) {
            $lines = @($text -split "`r?`n"); $ui.Activity.Text = ($lines | Select-Object -Last 500) -join "`r`n"; $ui.Activity.ScrollToEnd(); $script:LastLog = $text
        }
        if ($script:Job.Process.HasExited) {
            $script:Job.Process.WaitForExit()
            $code = $script:Job.Process.ExitCode; $script:Job.Process.Dispose(); $script:Job = $null
            $ui.Workspace.IsEnabled = $true; $ui.TargetBar.IsEnabled = $true; $ui.Progress.Visibility = 'Collapsed'
            if ($code -eq 0) { Load-Sd; $ui.Status.Text = 'Complete. You can safely remove the SD card after Windows finishes writing.' }
            else { $ui.Status.Text = 'Operation failed. See the activity log; existing backups were kept.' }
            if ($SmokeTest -and $SmokeAction) { Write-Host ('GUI worker exit: ' + $code); $window.Close() }
        }
    } catch { Write-Activity $_.Exception.Message }
})
$timer.Start()
$window.Add_Closing({ param($sender,$eventArgs) if ($null -ne $script:Job) { $eventArgs.Cancel = $true; $ui.Status.Text = 'Wait for the SD operation to finish before closing.' } })
$window.Add_Closed({ $timer.Stop() })
foreach ($name in @('Setup','Addons','Library','Themes')) {
    $ui['Nav' + $name].Tag = $name
    $ui['Nav' + $name].Add_Click({ param($sender,$eventArgs) Show-Page ([string]$sender.Tag) })
}
$ui.BrowseTarget.Add_Click({ Invoke-UiAction {
    $dialog = [Windows.Forms.FolderBrowserDialog]::new(); $dialog.Description = 'Choose the SD root or a staging folder'; $dialog.ShowNewFolderButton = $true
    if ($dialog.ShowDialog() -eq 'OK') { $ui.TargetPath.Text = $dialog.SelectedPath; Load-Sd }; $dialog.Dispose()
} })
$ui.LoadTarget.Add_Click({ Invoke-UiAction { Load-Sd } })
foreach ($name in @('Install','UpdateRuntime','UpdateFrontend','InstallAddons','CheckAddons','UpdateAddons','CheckSupport')) {
    $ui[$name].Tag = $name
    $ui[$name].Add_Click({ param($sender,$eventArgs) Invoke-UiAction { param($action) Start-SetupJob $action } @([string]$sender.Tag) })
}
$ui.OpenSd.Add_Click({ Invoke-UiAction { Open-SdFolder '' } })
$ui.OpenBackups.Add_Click({ Invoke-UiAction { Open-SdFolder '_nds/dsi-frontend/setup-backups' } })
$ui.OpenThemes.Add_Click({ Invoke-UiAction { Open-SdFolder '_nds/dsi-frontend/themes' } })
$ui.OpenRelease.Add_Click({ Start-Process 'https://github.com/nextcode4u/gloam-shell-ndx/releases' })
$ui.Recommended.Add_Click({ foreach ($item in $ui.AddonList.ItemsSource) { $item.Selected = $item.Id -in $recommended }; $ui.AddonList.Items.Refresh() })
$ui.SelectAll.Add_Click({ foreach ($item in $ui.AddonList.ItemsSource) { $item.Selected = -not $item.Experimental -and (-not $item.Noncommercial -or $ui.AllowNc.IsChecked) }; $ui.AddonList.Items.Refresh() })
$ui.ClearApps.Add_Click({ foreach ($item in $ui.AddonList.ItemsSource) { $item.Selected = $false }; $ui.AddonList.Items.Refresh() })
$ui.Profiles.Add_SelectionChanged({
    $profile = $ui.Profiles.SelectedItem
    if ($null -eq $profile) { return }
    $ui.ProfileId.Text = $profile.Id; $ui.ProfileId.IsEnabled = -not [bool]$profile.Id
    $ui.ProfileName.Text = if ($profile.Id) { $profile.Name } else { '' }
    $ui.Extensions.Text = $profile.Extensions; $ui.Application.Text = $profile.Application
    $ui.ContentFolder.Text = $profile.Folder; $ui.DsApplication.Text = $profile.DsApplication
    $ui.UseBootstrap.IsChecked = $profile.Bootstrap; $ui.UseHbargv.IsChecked = $profile.Hbargv
})
$ui.Tabs.Add_SelectionChanged({
    $tab = $ui.Tabs.SelectedItem; if ($null -eq $tab) { return }
    $ui.TabId.Text = $tab.Id; $ui.TabId.IsEnabled = -not [bool]$tab.Id
    $ui.TabLabel.Text = if ($tab.Id) { $tab.Label } else { '' }; $ui.TabFolder.Text = $tab.Folder
})
$ui.ProfileName.Add_TextChanged({ if ($ui.ProfileId.IsEnabled) { $ui.ProfileId.Text = ($ui.ProfileName.Text.ToLowerInvariant() -replace '[^a-z0-9_-]+','-').Trim('-') } })
$ui.TabLabel.Add_TextChanged({ if ($ui.TabId.IsEnabled) { $ui.TabId.Text = ($ui.TabLabel.Text.ToLowerInvariant() -replace '[^a-z0-9_-]+','-').Trim('-') } })
$ui.PickApplication.Add_Click({ Invoke-UiAction {
    $root = Get-SelectedRoot; $dialog = [Windows.Forms.OpenFileDialog]::new(); $dialog.Filter = 'Nintendo DS homebrew|*.nds;*.srl;*.dsi'; $dialog.InitialDirectory = $root
    if ($dialog.ShowDialog() -eq 'OK') {
        if ($dialog.FileName.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { $ui.Application.Text = 'sd:/' + (ConvertTo-GloamRelative $root $dialog.FileName) }
        else { $ui.Application.Text = Import-GloamApplication $root $dialog.FileName; Write-Activity 'Copied application onto the selected SD card.' }
    }; $dialog.Dispose()
} })
$ui.PickContent.Add_Click({ Invoke-UiAction {
    $root = Get-SelectedRoot; $dialog = [Windows.Forms.FolderBrowserDialog]::new(); $dialog.SelectedPath = $root; $dialog.Description = 'Choose a content folder on the selected SD card'
    if ($dialog.ShowDialog() -eq 'OK') { $ui.ContentFolder.Text = 'sd:/' + (ConvertTo-GloamRelative $root $dialog.SelectedPath) }; $dialog.Dispose()
} })
$ui.CopyContent.Add_Click({ Invoke-UiAction {
    Get-SelectedRoot | Out-Null
    $dialog = [Windows.Forms.OpenFileDialog]::new(); $dialog.Filter = 'Content files|*.*'; $dialog.Multiselect = $true
    if ($dialog.ShowDialog() -eq 'OK') { Start-SetupJob 'CopyContent' $dialog.FileNames }; $dialog.Dispose()
} })
$ui.SaveProfile.Add_Click({ Invoke-UiAction { Save-GloamProfile (Get-SelectedRoot) $ui.ProfileId.Text $ui.ProfileName.Text $ui.Extensions.Text $ui.Application.Text $ui.ContentFolder.Text ([bool]$ui.UseBootstrap.IsChecked) ([bool]$ui.UseHbargv.IsChecked) $ui.DsApplication.Text; Load-Sd; Write-Activity 'File handler saved. Restart Gloam to reload it.' } })
$ui.SaveTab.Add_Click({ Invoke-UiAction { Save-GloamTab (Get-SelectedRoot) $ui.TabId.Text $ui.TabLabel.Text $ui.TabFolder.Text; Load-Sd; Write-Activity 'Folder tab saved. Restart Gloam to reload it.' } })
$ui.SaveRoots.Add_Click({ Invoke-UiAction { Save-GloamRoots (Get-SelectedRoot) $ui.GamesRoot.Text $ui.AppsRoot.Text $ui.MediaRoot.Text; Load-Sd; Write-Activity 'Library folders saved.' } })
$ui.ImportTheme.Add_Click({ Invoke-UiAction {
    $root = Get-SelectedRoot; $dialog = [Windows.Forms.FolderBrowserDialog]::new(); $dialog.Description = 'Choose a theme folder containing theme.ini'
    if ($dialog.ShowDialog() -eq 'OK') { Import-GloamTheme $root $dialog.SelectedPath; Load-Sd; Write-Activity 'Theme imported. Select it in Gloam settings.' }; $dialog.Dispose()
} })
if (-not (Test-Path -LiteralPath $Manifest)) { throw 'Keep Gloam Setup beside downloads.json and the shared installer scripts.' }
$catalog = Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
$apps = if ($catalog.PSObject.Properties.Name -contains 'updates') { $catalog.updates } else { $catalog.applications }
$displayNames = @{
    'gameyob-dsi'='GameYob (DSi)'; 'gbarunner3'='GBARunner3'; 'nesds'='nesDS'; 'dslibris'='DSLibris';
    'dsxp'='DSXP'; 'scummvm'='ScummVM'; 'gimlids'='GimliDS'; 'hachibitto'='Hachibitto';
    'picodrivetwl'='PicoDriveTWL'; 'snemulds'='SNEmulDS'; 'snemulds-shim'='SNEmulDS Shim';
    's8ds'='S8DS'; 'nitrografx'='NitroGrafx'; 'nitroswan'='NitroSwan'; 'ngpds'='NGPDS';
    'fastvideods'='FastVideoDS'; 'tuna-vids'='tuna-viDS'; 'rocketvideoplayer'='Rocket Video Player';
    'nintv-ds'='Nintellivision'; 'jenesisdsplus'='jEnesisDS Plus'; 'a5200ds'='A5200DS';
    'a7800ds'='A7800DS'; 'a8ds'='A8DS'; 'stellads'='StellaDS'; 'colecods'='ColecoDS';
    'dracods'='DracoDS'; 'ds994a'='DS994a'; 'lodjurds'='LodjurDS'; 'microds'='MicroDS';
    'pokemini'='PokeMini'; 'speccyse'='SpeccySE'; 'sugards'='SugarDS'; 'wasabids'='WasabiDS'
}
$ui.AddonList.ItemsSource = @($apps | ForEach-Object {
    $experimental = $_.PSObject.Properties.Name -contains 'experimental' -and $_.experimental
    $name = if ($displayNames.ContainsKey($_.id)) { $displayNames[$_.id] } else { $_.id }
    $details = $name + ' ' + $_.version + "`n" + $_.terms
    if ($_.PSObject.Properties.Name -contains 'bios') { $details += "`nBIOS / support: " + $_.bios }
    if ($_.PSObject.Properties.Name -contains 'notes') { $details += "`n" + $_.notes }
    [pscustomobject]@{ Selected = $_.id -in $recommended; Id = $_.id; Name = $name; Details = $details; Version = $_.version; Noncommercial = $_.noncommercial; Experimental = $experimental;
        Terms = $(if ($experimental) { 'Experimental' } elseif ($_.noncommercial) { 'Noncommercial' } else { $_.terms }) }
})
if ($Target) { $ui.TargetPath.Text = $Target; Load-Sd }
Show-Page 'Setup'
if ($SmokeTest) {
    $window.Add_ContentRendered({
        foreach ($page in @('Addons','Library','Themes','Setup')) { $ui['Nav' + $page].RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent)) }
        Show-Page $ScreenshotPage
        $window.UpdateLayout()
        if ($ScreenshotPath) {
            $bitmap = [Windows.Media.Imaging.RenderTargetBitmap]::new([int]$window.ActualWidth,[int]$window.ActualHeight,96,96,[Windows.Media.PixelFormats]::Pbgra32)
            $bitmap.Render($window); $encoder = [Windows.Media.Imaging.PngBitmapEncoder]::new(); $encoder.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
            $stream = [IO.File]::Create($ScreenshotPath); try { $encoder.Save($stream) } finally { $stream.Dispose() }
        }
        Write-Host ('GUI loaded: ' + $ui.Count + ' controls; ' + @($ui.AddonList.ItemsSource).Count + ' add-ons.')
        if ($SmokeAction) { $ui[$SmokeAction].RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent)) }
        else { $window.Close() }
    })
}
$window.ShowDialog() | Out-Null
