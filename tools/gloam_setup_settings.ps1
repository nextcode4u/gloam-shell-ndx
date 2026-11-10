# SPDX-License-Identifier: GPL-3.0-or-later
# Launcher state is a flat record stream. Repeated navigation/favourite records
# must survive desktop edits; it is deliberately not parsed as a normal INI.
$GloamSettingRules = [ordered]@{
    settings_mode = @{ Default = 'simple'; Choices = @('simple','advanced') }
    simple_settings_lock = @{ Default = '0'; Choices = @('0','1') }
    browser_style = @{ Default = '0'; Choices = @('0','1','2','3') }
    icon_size = @{ Default = '0'; Choices = @('0','1') }
    sort_mode = @{ Default = '0'; Choices = @('0','1','2','3') }
    scroll_speed = @{ Default = '1'; Choices = @('0','1','2') }
    show_hidden = @{ Default = '0'; Choices = @('0','1') }
    show_system = @{ Default = '0'; Choices = @('0','1') }
    hide_save_folders = @{ Default = '1'; Choices = @('0','1') }
    hide_bios_folders = @{ Default = '1'; Choices = @('0','1') }
    show_extensions = @{ Default = '1'; Choices = @('0','1') }
    filter_recognized = @{ Default = '0'; Choices = @('0','1') }
    show_covers = @{ Default = '1'; Choices = @('0','1') }
    internal_titles = @{ Default = '0'; Choices = @('0','1') }
    title_language = @{ Default = '1'; Choices = @('0','1','2','3','4','5','6','7') }
    reduced_motion = @{ Default = '0'; Choices = @('0','1') }
    clock_12_hour = @{ Default = '0'; Choices = @('0','1') }
    music_path = @{ Default = ''; Pattern = '^(|sd:/[^\r\n\x00]+)$' }
    music_mode = @{ Default = '0'; Choices = @('0','1','2') }
    music_volume = @{ Default = '70'; Minimum = 0; Maximum = 100 }
    effects_volume = @{ Default = '70'; Minimum = 0; Maximum = 100 }
    theme_sounds = @{ Default = '1'; Choices = @('0','1') }
    theme = @{ Default = '0'; Minimum = 0; Maximum = 15 }
    theme_accent_override = @{ Default = '0'; Choices = @('0','1') }
    theme_id = @{ Default = ''; Pattern = '^[a-zA-Z0-9_-]{0,63}$' }
    startup_tab = @{ Default = ''; Pattern = '^[a-zA-Z0-9_-]{0,31}$' }
    auto_launch_delay = @{ Default = '0'; Choices = @('0','3','5','10') }
}

function Test-GloamSetting([string]$Key, [string]$Value) {
    if (-not $GloamSettingRules.Contains($Key)) { return $false }
    $rule = $GloamSettingRules[$Key]
    if ($rule.ContainsKey('Choices')) { return $Value -cin $rule.Choices }
    if ($rule.ContainsKey('Pattern')) { return $Value -cmatch $rule.Pattern }
    $number = 0
    return [int]::TryParse($Value, [ref]$number) -and $number -ge $rule.Minimum -and $number -le $rule.Maximum
}

function Read-GloamState([string]$Root, [string]$Relative = '_nds/dsi-frontend/state.ini') {
    $path = Get-GloamPath $Root $Relative
    $values = [ordered]@{}
    foreach ($key in $GloamSettingRules.Keys) { $values[$key] = $GloamSettingRules[$key].Default }
    $lines = @(); $hash = ''; $issues = @(); $exists = Test-Path -LiteralPath $path -PathType Leaf
    if ($exists) {
        if ((Get-Item -LiteralPath $path).Length -gt 128KB) { throw 'Launcher state exceeds its 128 KiB limit.' }
        $bytes = [IO.File]::ReadAllBytes($path)
        $text = [Text.UTF8Encoding]::new($false, $true).GetString($bytes)
        if ($text.Contains([char]0)) { throw 'Launcher state contains binary data.' }
        $lines = @($text -split "`r?`n")
        if (-not $lines.Count -or $lines[0] -cne 'version=1') { throw 'Unsupported launcher state. Keep the original and restore a valid configuration backup.' }
        $digest = [Security.Cryptography.SHA256]::Create()
        try { $hash = ([BitConverter]::ToString($digest.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
        finally { $digest.Dispose() }
        # Version-1 files without an explicit mode retain the old Advanced UI.
        $values.settings_mode = 'advanced'
        foreach ($line in $lines) {
            if ([Text.Encoding]::UTF8.GetByteCount($line) -ge 1024) { throw 'Launcher state contains an oversized line.' }
            if ($line -notmatch '^([^=]+)=(.*)$') { continue }
            $key = $Matches[1]; $value = $Matches[2]
            if (-not $GloamSettingRules.Contains($key)) { continue }
            if (Test-GloamSetting $key $value) { $values[$key] = $value }
            else { $issues += $key }
        }
    }
    return [pscustomobject]@{ Values = $values; Lines = $lines; Hash = $hash; Issues = $issues }
}

function Save-GloamSettings([string]$Root, $Changes, [string]$ExpectedHash) {
    Assert-GloamConfigurationReady $Root
    $state = Read-GloamState $Root
    if ($state.Hash -cne $ExpectedHash) { throw 'Settings changed on the SD card. Reload settings before saving your changes.' }
    $updates = [ordered]@{}
    foreach ($key in $Changes.Keys) {
        $value = [string]$Changes[$key]
        if (-not (Test-GloamSetting $key $value)) { throw "Unsupported setting value: $key" }
        $updates[$key] = $value
    }
    $locked = if ($updates.Contains('simple_settings_lock')) { $updates['simple_settings_lock'] } else { $state.Values.simple_settings_lock }
    if ($locked -eq '1') { $updates['settings_mode'] = 'simple' }
    if (-not $updates.Count) { Write-Host 'No settings changes.'; return }
    if ($updates.Contains('theme_id') -and $updates['theme_id']) {
        $theme = Get-GloamPath $Root ('_nds/dsi-frontend/themes/' + $updates['theme_id'] + '/theme.ini')
        if (-not (Test-Path -LiteralPath $theme -PathType Leaf)) { throw 'Choose an installed theme or Built-in.' }
    }
    if ($updates.Contains('music_path') -and $updates['music_path']) {
        $relative = ConvertTo-GloamRelative $Root $updates['music_path']
        Test-GloamMusic (Get-GloamPath $Root $relative)
    }
    if ($updates.Contains('music_mode') -and $updates['music_mode'] -eq '2') {
        $music = if ($updates.Contains('music_path')) { $updates['music_path'] } else { $state.Values.music_path }
        if (-not $music) { throw 'Choose a custom WAV file first.' }
        Test-GloamMusic (Get-GloamPath $Root (ConvertTo-GloamRelative $Root $music))
    }
    if ($updates.Contains('startup_tab') -and $updates['startup_tab']) {
        $id = $updates['startup_tab'].ToLowerInvariant()
        $tabs = Read-GloamIni (Get-GloamPath $Root '_nds/dsi-frontend/tabs.ini')
        if ($id -notin @('home','games','apps','media','system','files') -and -not $tabs.Contains('tab.' + $id)) { throw 'Choose an existing startup tab.' }
    }
    $output = [Collections.Generic.List[string]]::new(); $output.Add('version=1'); $written = @{}
    foreach ($line in $state.Lines | Select-Object -Skip 1) {
        if ($line -cmatch '^([^=]+)=') {
            $key = $Matches[1]
            if ($updates.Contains($key)) {
                if (-not $written.ContainsKey($key)) { $output.Add($key + '=' + $updates[$key]); $written[$key] = $true }
                continue
            }
        }
        $output.Add($line)
    }
    foreach ($key in $updates.Keys) { if (-not $written.ContainsKey($key)) { $output.Add($key + '=' + $updates[$key]) } }
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes(($output -join "`n") + "`n")
    if ($bytes.Length -gt 128KB) { throw 'Updated state exceeds its size limit.' }
    Write-GloamFile $Root '_nds/dsi-frontend/state.ini' $bytes $true | Out-Null
    Write-Host 'Saved launcher settings. Restart Gloam to reload them.'
}

function Get-GloamSetupPreset([string]$Name) {
    switch ($Name) {
        'Everyday' { return @{ settings_mode='simple'; simple_settings_lock='0'; browser_style='0'; icon_size='0'; show_covers='1'; reduced_motion='0'; hide_save_folders='1'; hide_bios_folders='1'; show_hidden='0'; show_system='0'; scroll_speed='1'; auto_launch_delay='0' } }
        'Simple locked' { return @{ settings_mode='simple'; simple_settings_lock='1'; show_hidden='0'; show_system='0'; hide_save_folders='1'; hide_bios_folders='1'; filter_recognized='1'; startup_tab='home'; auto_launch_delay='0' } }
        'Quiet' { return @{ music_mode='0'; theme_sounds='0'; reduced_motion='1'; auto_launch_delay='0' } }
        'Large icons' { return @{ browser_style='0'; icon_size='1'; show_covers='1'; scroll_speed='0' } }
        default { throw 'Unknown settings preset.' }
    }
}

function Test-GloamSetup([string]$Root) {
    $root = Get-GloamRoot $Root
    $required = @('BOOT.NDS','_nds/nds-bootstrap-release.nds','_nds/dsi-frontend/loaders/slot1launch.nds')
    foreach ($name in $required) {
        $path = Get-GloamPath $root $name
        if ((Test-Path -LiteralPath $path -PathType Leaf) -and (Get-Item -LiteralPath $path).Length -ge 512) { Write-Host "OK: $name" }
        else { Write-Host "MISSING / INVALID: $name" }
    }
    try {
        Assert-GloamConfigurationReady $root
        $state = Read-GloamState $root
        Write-Host 'OK: launcher settings can be read.'
        foreach ($key in $state.Issues) { Write-Host "Invalid setting uses its default: $key" }
        if ($state.Values.simple_settings_lock -eq '1') { Write-Host 'Simple settings lock: On' }
        $theme = $state.Values.theme_id
        if ($theme -and -not (Test-Path -LiteralPath (Get-GloamPath $root ("_nds/dsi-frontend/themes/$theme/theme.ini")) -PathType Leaf)) { Write-Host "MISSING: selected theme $theme" }
    } catch { Write-Host ('SETTINGS: ' + $_.Exception.Message) }
    $folder = Get-GloamPath $root '_nds/dsi-frontend/plugins'
    if (Test-Path -LiteralPath $folder -PathType Container) {
        $files = @(Get-ChildItem -LiteralPath $folder -Filter '*.ini' -File)
        if ($files.Count -gt 32) { Write-Host 'TOO MANY: only the first 32 profile files can be loaded.' }
        foreach ($file in $files) {
            try {
                $data = Read-GloamIni $file.FullName
                if (-not $data.Contains('plugin')) { continue }
                $plugin = $data['plugin']
                foreach ($key in @('application','dsipath','ntrpath')) {
                    if (-not $plugin[$key]) { continue }
                    $relative = ConvertTo-GloamRelative $root $plugin[$key]
                    if (-not (Test-Path -LiteralPath (Get-GloamPath $root $relative) -PathType Leaf)) { Write-Host "Optional app not installed: $($file.BaseName) ($key)" }
                }
            } catch { Write-Host ("PROFILE $($file.Name): " + $_.Exception.Message) }
        }
    }
    Write-Host 'Check BIOS / support for the installed apps separately. Presence checks do not establish game compatibility.'
}

function Get-GloamThemeSettings([string]$Root, [string]$Id, [bool]$UseLayout) {
    if (-not (Test-GloamSetting 'theme_id' $Id)) { throw 'Unsupported theme ID.' }
    $changes = @{ theme_id = $Id; theme_accent_override = '0' }
    if ($Id) {
        $file = Get-GloamPath $Root ("_nds/dsi-frontend/themes/$Id/theme.ini")
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw 'Theme package is missing.' }
        $data = Read-GloamIni $file
        if (-not $data.Contains('theme') -or $data['theme']['version'] -ne '1') { throw 'Unsupported theme package version.' }
        if ($UseLayout -and $data.Contains('layout')) {
            $views = @('classic','extended','list','grid'); $icons = @('small','large')
            if ($data['layout']['view']) {
                $index = [Array]::IndexOf($views, $data['layout']['view'])
                if ($index -lt 0) { throw 'Unsupported theme browser layout.' }
                $changes['browser_style'] = [string]$index
            }
            if ($data['layout']['icon_size']) {
                $index = [Array]::IndexOf($icons, $data['layout']['icon_size'])
                if ($index -lt 0) { throw 'Unsupported theme icon size.' }
                $changes['icon_size'] = [string]$index
            }
        }
    }
    return $changes
}

function Test-GloamMusic([string]$Path) {
    if ([IO.Path]::GetExtension($Path) -ne '.wav' -or (Get-Item -LiteralPath $Path).Length -gt (1MB + 65536)) { throw 'Use a mono PCM16 WAV with at most 1 MiB of sample data.' }
    $stream = [IO.File]::OpenRead($Path); $reader = [IO.BinaryReader]::new($stream)
    try {
        if ([Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)) -cne 'RIFF') { throw 'Not a RIFF WAV.' }
        $end = [long]$reader.ReadUInt32() + 8
        if ($end -gt $stream.Length -or [Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)) -cne 'WAVE') { throw 'Invalid WAV header.' }
        $format = $false; $samples = $false; $chunks = 0
        while ($stream.Position + 8 -le $end) {
            if (++$chunks -gt 128) { throw 'Too many WAV chunks.' }
            $kind = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)); $size = $reader.ReadUInt32(); $offset = $stream.Position
            if ($size -gt $end - $offset) { throw 'Truncated WAV chunk.' }
            if ($kind -ceq 'fmt ') {
                if ($size -lt 16) { throw 'Invalid WAV format chunk.' }
                $encoding = $reader.ReadUInt16(); $channels = $reader.ReadUInt16(); $rate = $reader.ReadUInt32()
                $speed = $reader.ReadUInt32(); $align = $reader.ReadUInt16(); $bits = $reader.ReadUInt16()
                $format = $encoding -eq 1 -and $channels -eq 1 -and $rate -ge 8000 -and $rate -le 48000 -and $speed -eq $rate * 2 -and $align -eq 2 -and $bits -eq 16
            } elseif ($kind -ceq 'data') { $samples = $size -gt 0 -and $size -le 1MB -and $size % 2 -eq 0 }
            $next = $offset + $size + ($size % 2)
            if ($next -gt $end) { throw 'Missing WAV chunk padding.' }
            $stream.Position = $next
        }
        if (-not $format -or -not $samples) { throw 'Use mono PCM16 WAV, 8000-48000 Hz, at most 1 MiB of samples.' }
    } finally { $reader.Dispose(); $stream.Dispose() }
}

function Restore-GloamStateBackup([string]$Root) {
    Assert-GloamConfigurationReady $Root
    $relative = '_nds/dsi-frontend/state.bak'
    $snapshot = Read-GloamState $Root $relative
    if (-not $snapshot.Hash) { throw 'No previous console settings file was found.' }
    $path = Get-GloamPath $Root $relative
    $bytes = [IO.File]::ReadAllBytes($path)
    if ((Get-GloamHash $path) -cne $snapshot.Hash) { throw 'The console settings backup changed. Try again after closing Gloam.' }
    Write-GloamFile $Root '_nds/dsi-frontend/state.ini' $bytes $true | Out-Null
    Write-Host 'Restored the last console settings. The replaced file is kept in setup-backups.'
}
