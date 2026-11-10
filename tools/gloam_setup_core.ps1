# SPDX-License-Identifier: GPL-3.0-or-later
# Shared setup operations. Dot-sourcing this file never opens a window or writes files.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-GloamRoot([string]$Target) {
    if (-not $Target -or -not (Test-Path -LiteralPath $Target -PathType Container)) {
        throw 'Choose an existing SD card or staging folder first.'
    }
    $root = [IO.Path]::GetFullPath($Target)
    if ($env:SystemDrive -and $root.TrimEnd('\') -eq $env:SystemDrive) {
        throw 'Choose the SD card or a staging folder, not the Windows system drive.'
    }
    $node = Get-Item -LiteralPath $root
    while ($null -ne $node) {
        if ($node.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked folders are not supported.' }
        $node = $node.Parent
    }
    return $root.TrimEnd('\') + '\'
}

function Get-GloamPath([string]$Root, [string]$Relative) {
    if (-not $Relative -or $Relative -match '(^[/\\]|:|[\x00-\x1f]|(^|[/\\])\.\.?([/\\]|$))') {
        throw "Invalid SD-relative path: $Relative"
    }
    $root = Get-GloamRoot $Root
    $path = [IO.Path]::GetFullPath((Join-Path $root $Relative))
    if (-not $path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw 'Path leaves the selected SD folder.' }
    $cursor = $path
    while ($cursor.Length -ge $root.TrimEnd('\').Length) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked SD paths are not supported.' }
            if ($cursor -ne $path -and -not $item.PSIsContainer) { throw 'A parent path is a file.' }
        }
        $parent = [IO.Path]::GetDirectoryName($cursor)
        if (-not $parent -or $parent -eq $cursor) { break }
        $cursor = $parent
    }
    return $path
}

function Write-GloamFile([string]$Root, [string]$Relative, [byte[]]$Data, [bool]$Replace = $false) {
    $path = Get-GloamPath $Root $Relative
    if (Test-Path -LiteralPath $path -PathType Container) { throw "A folder occupies $Relative" }
    if ((Test-Path -LiteralPath $path) -and -not $Replace) { return $false }
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
    $temporary = $path + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllBytes($temporary, $Data)
        if (Test-Path -LiteralPath $path) {
            $backupRelative = '_nds/dsi-frontend/setup-backups/' + [Guid]::NewGuid().ToString('N') + '/' + $Relative
            $backup = Get-GloamPath $Root $backupRelative
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup)) | Out-Null
            [IO.File]::Replace($temporary, $path, $backup)
            Write-Output "Backup: $backupRelative" | Write-Host
        } else { [IO.File]::Move($temporary, $path) }
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
    return $true
}

function Get-GloamHash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $hash.Dispose(); $stream.Dispose() }
}

function Get-GloamRelease([string]$Directory) {
    [IO.Directory]::CreateDirectory($Directory) | Out-Null
    $metadata = Join-Path $Directory 'release.json'
    if (Test-Path -LiteralPath $metadata) { return Get-Content -LiteralPath $metadata -Raw | ConvertFrom-Json }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $releases = Invoke-RestMethod 'https://api.github.com/repos/nextcode4u/gloam-shell-ndx/releases?per_page=100' -UserAgent 'Gloam-Setup' -TimeoutSec 30
    $release = @($releases | Where-Object {
        -not $_.draft -and @($_.assets | Where-Object name -eq 'addons.zip').Count -eq 1
    } | Sort-Object published_at -Descending)[0]
    if ($null -eq $release) { throw 'No published setup package was found.' }
    [IO.File]::WriteAllText($metadata, ($release | ConvertTo-Json -Depth 30), [Text.UTF8Encoding]::new($false))
    return $release
}

function Get-GloamPackage([string]$Directory, [string]$Name) {
    if ($Name -notin @('BOOT.NDS', 'sd.zip', 'addons.zip', 'licenses.zip')) { throw 'Unsupported release asset.' }
    $sums = Join-Path $Directory 'SHA256SUMS'
    $release = $null
    if (-not (Test-Path -LiteralPath $sums)) {
        $release = Get-GloamRelease $Directory
        $asset = @($release.assets | Where-Object name -eq 'SHA256SUMS')
        if ($asset.Count -ne 1) { throw 'Missing release checksums.' }
        Invoke-WebRequest $asset[0].browser_download_url -UseBasicParsing -OutFile $sums -UserAgent 'Gloam-Setup' -TimeoutSec 60
    }
    $hashes = @{}
    foreach ($line in [IO.File]::ReadAllLines($sums)) {
        if ($line -notmatch '^([a-f0-9]{64})  ([A-Za-z0-9_.-]+)$' -or $hashes.ContainsKey($Matches[2])) {
            throw 'Invalid release checksum list.'
        }
        $hashes[$Matches[2]] = $Matches[1]
    }
    if (-not $hashes.ContainsKey($Name)) { throw "No checksum for $Name" }
    $file = Join-Path $Directory $Name
    if (-not (Test-Path -LiteralPath $file)) {
        if ($null -eq $release) { $release = Get-GloamRelease $Directory }
        $asset = @($release.assets | Where-Object name -eq $Name)
        if ($asset.Count -ne 1 -or $asset[0].size -gt 128MB) { throw 'Missing or oversized setup asset.' }
        $url = [string]$asset[0].browser_download_url
        if (-not $url.StartsWith('https://github.com/nextcode4u/gloam-shell-ndx/releases/download/')) { throw 'Unexpected release origin.' }
        Write-Host "Downloading $Name..."
        $temporary = $file + '.download'
        try {
            Invoke-WebRequest $url -UseBasicParsing -OutFile $temporary -UserAgent 'Gloam-Setup' -TimeoutSec 120
            if ((Get-GloamHash $temporary) -ne $hashes[$Name]) { throw "Checksum mismatch: $Name" }
            [IO.File]::Move($temporary, $file)
        } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
    }
    if ((Get-GloamHash $file) -ne $hashes[$Name]) { throw "Checksum mismatch: $Name" }
    return $file
}

function Install-GloamArchive([string]$Root, [string]$ArchivePath, [bool]$UpdateRuntime = $false) {
    $root = Get-GloamRoot $Root
    $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $seen = @{}; $total = 0L; $plan = @()
        foreach ($entry in $archive.Entries) {
            $name = $entry.FullName
            if ($name.Contains('\') -or $name.Contains(':') -or $name.StartsWith('/') -or
                $name -match '(^|/)\.\.?(/|$)' -or ($entry.ExternalAttributes -shr 28 -band 15) -eq 10) {
                throw "Unsafe archive path: $name"
            }
            if ($seen.ContainsKey($name.TrimEnd('/'))) { throw 'Duplicate archive path.' }; $seen[$name.TrimEnd('/')] = $name.EndsWith('/')
            $total += $entry.Length
            if ($entry.Length -gt 64MB -or $total -gt 256MB -or $seen.Count -gt 30000) { throw 'Oversized setup archive.' }
            if ($name.StartsWith('addon-installer/')) { continue }
            if ($name -notmatch '^(BOOT\.NDS|(_nds|_gba|roms|apps|media|data)/.*)$') { throw "Unexpected SD package path: $name" }
            $relative = $name.TrimEnd('/')
            $path = Get-GloamPath $root $relative
            $folder = $name.EndsWith('/')
            if (Test-Path -LiteralPath $path) {
                if ((Test-Path -LiteralPath $path -PathType Container) -ne $folder) { throw "File/folder conflict: $relative" }
            }
            $plan += [pscustomobject]@{ Entry = $entry; Relative = $relative; Folder = $folder }
        }
        foreach ($item in $plan) {
            $parent = $item.Relative
            while ($parent.Contains('/')) {
                $parent = $parent.Substring(0, $parent.LastIndexOf('/'))
                if ($seen.ContainsKey($parent) -and -not $seen[$parent]) { throw 'An archive file blocks a parent folder.' }
            }
        }
        # Validate the entire destination plan before creating any SD files.
        foreach ($item in $plan) {
            $path = Get-GloamPath $root $item.Relative
            if ($item.Folder) { [IO.Directory]::CreateDirectory($path) | Out-Null; continue }
            $stream = $item.Entry.Open(); $memory = [IO.MemoryStream]::new()
            try { $stream.CopyTo($memory); [byte[]]$bytes = $memory.ToArray() }
            finally { $stream.Dispose(); $memory.Dispose() }
            $owned = @('BOOT.NDS','_nds/nds-bootstrap-release.nds',
                '_nds/dsi-frontend/loaders/slot1launch.nds','_nds/dsi-frontend/install/gloam-forwarder.dsi',
                '_nds/dsi-frontend/install/gloam-forwarder.tmd')
            $replace = $item.Relative -ceq 'BOOT.NDS' -or ($UpdateRuntime -and $item.Relative -cin $owned)
            if (Write-GloamFile $root $item.Relative $bytes $replace) { Write-Host "Installed: $($item.Relative)" }
            else { Write-Host "Kept existing: $($item.Relative)" }
        }
    } finally { $archive.Dispose() }
}

function Read-GloamIni([string]$Path) {
    $sections = [ordered]@{}; $section = ''
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $sections }
    if ((Get-Item -LiteralPath $Path).Length -gt 64KB) { throw 'Configuration is too large.' }
    foreach ($line in [IO.File]::ReadAllLines($Path)) {
        $text = $line.Trim()
        if (-not $text -or $text.StartsWith(';') -or $text.StartsWith('#')) { continue }
        if ($text -match '^\[([^\]]+)\]$') {
            $section = $Matches[1].ToLowerInvariant()
            if ($sections.Contains($section)) { throw 'Duplicate INI section; edit or repair it before saving.' }
            $sections[$section] = [ordered]@{}
        } elseif ($text -match '^([^=]+)=(.*)$' -and $section) {
            $key = $Matches[1].Trim().ToLowerInvariant()
            if ($sections[$section].Contains($key)) { throw 'Duplicate INI key; edit or repair it before saving.' }
            $sections[$section][$key] = $Matches[2].Trim()
        } else { throw 'Unrecognized configuration line; the original file was kept.' }
    }
    return $sections
}

function ConvertTo-GloamIni($Sections) {
    $lines = @('; Written by Gloam Setup. Previous versions are kept under setup-backups.')
    foreach ($section in $Sections.Keys) {
        $lines += "[$section]"
        foreach ($key in $Sections[$section].Keys) { $lines += "$key=$($Sections[$section][$key])" }
        $lines += ''
    }
    return [Text.Encoding]::UTF8.GetBytes(($lines -join "`n") + "`n")
}

function Get-GloamProfiles([string]$Root) {
    $folder = Get-GloamPath $Root '_nds/dsi-frontend/plugins'
    if (-not (Test-Path -LiteralPath $folder -PathType Container)) { return }
    foreach ($file in Get-ChildItem -LiteralPath $folder -Filter '*.ini' | Sort-Object Name) {
        $data = Read-GloamIni $file.FullName
        if (-not $data.Contains('plugin')) { continue }
        $plugin = $data['plugin']
        [pscustomobject]@{ Id = $file.BaseName; Name = $plugin['name']; Extensions = $plugin['extensions'];
            Application = $(if ($plugin['dsipath']) { $plugin['dsipath'] } else { $plugin['application'] }); DsApplication = $plugin['ntrpath']; Folder = $plugin['content_root'];
            Bootstrap = $plugin['bootstrap'] -eq '1'; Hbargv = $plugin['hbargv'] -eq '1' }
    }
}

function ConvertTo-GloamRelative([string]$Root, [string]$Path) {
    $root = Get-GloamRoot $Root
    if ($Path.StartsWith('sd:/', [StringComparison]::OrdinalIgnoreCase)) { $Path = $Path.Substring(4) }
    elseif ([IO.Path]::IsPathRooted($Path)) {
        $full = [IO.Path]::GetFullPath($Path)
        if (-not $full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw 'Choose a file or folder on the selected SD card.' }
        $Path = $full.Substring($root.Length)
    }
    $relative = $Path.Replace('\', '/').TrimEnd('/')
    Get-GloamPath $root $relative | Out-Null
    if ([Text.Encoding]::UTF8.GetByteCount('sd:/' + $relative) -ge 768) { throw 'SD path is too long for the launcher.' }
    return $relative
}

function Save-GloamProfile([string]$Root, [string]$Id, [string]$Name, [string]$Extensions,
                           [string]$Application, [string]$Folder, [bool]$Bootstrap = $false, [bool]$Hbargv = $false, [string]$DsApplication = '') {
    if ($Id -notmatch '^[a-z0-9_-]{1,63}$') { throw 'Profile ID: use 1-63 lowercase letters, digits, underscores or hyphens.' }
    if (-not $Name.Trim() -or $Name -match '[\r\n=\[\]]' -or [Text.Encoding]::UTF8.GetByteCount($Name) -gt 63) { throw 'Use a short profile name, up to 63 UTF-8 bytes.' }
    $suffixes = @($Extensions.ToLowerInvariant() -split '[,\s]+' | Where-Object { $_ } | ForEach-Object { $_.TrimStart('.') } | Select-Object -Unique)
    if (-not $suffixes.Count -or ($suffixes -join ',').Length -ge 128) { throw 'Enter file extensions, up to 127 characters in total.' }
    foreach ($ext in $suffixes) {
        if ($ext -notmatch '^[a-z0-9_]+(\.[a-z0-9_]+)*$' -or $ext.Length -gt 15 -or $ext -in @('nds','srl','dsi','app','argv','dsi_mode')) { throw "Unsupported extension: $ext" }
    }
    $app = ConvertTo-GloamRelative $Root $Application
    if ($app -notmatch '\.(nds|srl|dsi)$' -or -not (Test-Path -LiteralPath (Get-GloamPath $Root $app) -PathType Leaf)) { throw 'Choose an installed .nds, .srl or .dsi application.' }
    $dsApp = if ($DsApplication) { ConvertTo-GloamRelative $Root $DsApplication } else { $app }
    if ($dsApp -notmatch '\.(nds|srl|dsi)$' -or -not (Test-Path -LiteralPath (Get-GloamPath $Root $dsApp) -PathType Leaf)) { throw 'The DS-mode application is missing or invalid.' }
    $scope = if ($Folder) { 'sd:/' + (ConvertTo-GloamRelative $Root $Folder) } else { '' }
    $relative = "_nds/dsi-frontend/plugins/$Id.ini"
    $target = Get-GloamPath $Root $relative
    $profiles = @(Get-ChildItem -LiteralPath (Get-GloamPath $Root '_nds/dsi-frontend/plugins') -Filter '*.ini' -File)
    if (-not (Test-Path -LiteralPath $target) -and $profiles.Count -ge 32) { throw 'The launcher supports 32 profiles. Edit an existing profile instead.' }
    $data = Read-GloamIni $target
    if (-not $data.Contains('plugin')) { $data['plugin'] = [ordered]@{} }
    $plugin = $data['plugin']; $plugin['version'] = '1'; $plugin['name'] = $Name
    $plugin['extensions'] = $suffixes -join ','; $plugin['application'] = 'sd:/' + $app
    # Mode-specific paths take precedence in the launcher: update them together.
    $plugin['dsipath'] = 'sd:/' + $app; $plugin['ntrpath'] = 'sd:/' + $dsApp
    $plugin['content_root'] = $scope; $plugin['argv'] = '1'; $plugin['bootstrap'] = [int]$Bootstrap
    $plugin['hbargv'] = [int]$Hbargv; $plugin['disabled'] = '0'
    if ($scope) { [IO.Directory]::CreateDirectory((Get-GloamPath $Root $scope.Substring(4))) | Out-Null }
    $bytes = ConvertTo-GloamIni $data
    if ($bytes.Length -gt 4096) { throw 'The resulting association exceeds the launcher''s 4 KiB profile limit.' }
    Write-GloamFile $Root $relative $bytes $true | Out-Null
    Write-Host "Saved association: $Name ($($suffixes -join ', ')). Restart Gloam to reload it."
}

function Save-GloamTab([string]$Root, [string]$Id, [string]$Label, [string]$Folder) {
    if ($Id -notmatch '^[a-zA-Z0-9_-]{1,31}$' -or $Id -in @('home','games','apps','media','system','files')) { throw 'Choose a unique tab ID using 1-31 letters, digits, underscores or hyphens.' }
    if (-not $Label.Trim() -or $Label -match '[\r\n=\[\]]' -or [Text.Encoding]::UTF8.GetByteCount($Label) -gt 63) { throw 'Use a short tab label, up to 63 UTF-8 bytes.' }
    $relative = ConvertTo-GloamRelative $Root $Folder
    $file = '_nds/dsi-frontend/tabs.ini'; $data = Read-GloamIni (Get-GloamPath $Root $file)
    $section = 'tab.' + $Id.ToLowerInvariant()
    if (-not $data.Contains($section) -and @($data.Keys | Where-Object { $_.StartsWith('tab.') }).Count -ge 42) { throw 'The launcher supports 42 custom tabs.' }
    if (-not $data.Contains('tabs')) { $data['tabs'] = [ordered]@{} }
    $data['tabs']['version'] = '1'; $data[$section] = [ordered]@{ label = $Label; path = 'sd:/' + $relative }
    [IO.Directory]::CreateDirectory((Get-GloamPath $Root $relative)) | Out-Null
    Write-GloamFile $Root $file (ConvertTo-GloamIni $data) $true | Out-Null
    Write-Host "Saved folder tab: $Label. Restart Gloam to reload it."
}

function Save-GloamRoots([string]$Root, [string]$Games, [string]$Apps, [string]$Media) {
    $values = [ordered]@{}
    foreach ($item in @(@('games', $Games), @('apps', $Apps), @('media', $Media))) {
        $values[$item[0]] = ConvertTo-GloamRelative $Root $item[1]
    }
    $file = '_nds/dsi-frontend/tabs.ini'; $data = Read-GloamIni (Get-GloamPath $Root $file)
    if (-not $data.Contains('tabs')) { $data['tabs'] = [ordered]@{} }
    if (-not $data.Contains('roots')) { $data['roots'] = [ordered]@{} }
    $data['tabs']['version'] = '1'
    foreach ($key in $values.Keys) {
        [IO.Directory]::CreateDirectory((Get-GloamPath $Root $values[$key])) | Out-Null
        $data['roots'][$key] = 'sd:/' + $values[$key]
    }
    Write-GloamFile $Root $file (ConvertTo-GloamIni $data) $true | Out-Null
    Write-Host 'Saved Games, Apps and Media roots. Restart Gloam to reload them.'
}

function Import-GloamApplication([string]$Root, [string]$Source) {
    $sourceFile = Get-Item -LiteralPath $Source
    if ($sourceFile.PSIsContainer -or $sourceFile.Attributes -band [IO.FileAttributes]::ReparsePoint -or
        $sourceFile.Extension -notin @('.nds','.srl','.dsi') -or $sourceFile.Length -lt 512 -or $sourceFile.Length -gt 64MB) {
        throw 'Choose a homebrew .nds, .srl or .dsi file between 512 bytes and 64 MiB.'
    }
    $relative = '_nds/dsi-frontend/plugins/apps/' + $sourceFile.Name
    $destination = Get-GloamPath $Root $relative
    if ((Test-Path -LiteralPath $destination) -and (Get-GloamHash $destination) -ne (Get-GloamHash $Source)) {
        throw 'An application with that filename already exists. Rename the imported file or select the installed application.'
    }
    Write-GloamFile $Root $relative ([IO.File]::ReadAllBytes($Source)) | Out-Null
    return 'sd:/' + $relative
}

function Import-GloamTheme([string]$Root, [string]$Source) {
    $folder = Get-Item -LiteralPath $Source
    if (-not $folder.PSIsContainer -or -not (Test-Path -LiteralPath (Join-Path $Source 'theme.ini') -PathType Leaf)) { throw 'Choose a theme folder containing theme.ini.' }
    if ($folder.Name -notmatch '^[a-zA-Z0-9_-]{1,63}$') { throw 'Rename the theme folder using letters, digits, underscores or hyphens.' }
    $relative = '_nds/dsi-frontend/themes/' + $folder.Name
    $destination = Get-GloamPath $Root $relative
    if (Test-Path -LiteralPath $destination) { throw 'That theme already exists. Rename the imported folder to keep both.' }
    $files = [Collections.Generic.List[object]]::new(); $queue = [Collections.Generic.Queue[string]]::new(); $queue.Enqueue($folder.FullName); $total = 0L
    while ($queue.Count) {
        $directory = $queue.Dequeue()
        $node = Get-Item -LiteralPath $directory
        if ($node.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked theme folders are not supported.' }
        foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked theme files are not supported.' }
            if ($item.PSIsContainer) { $queue.Enqueue($item.FullName); continue }
            $total += $item.Length
            if ($item.Length -gt 64MB -or $total -gt 128MB -or $files.Count -ge 1024) { throw 'Theme exceeds setup import limits.' }
            $name = $relative + '/' + $item.FullName.Substring($folder.FullName.Length + 1).Replace('\','/')
            Get-GloamPath $Root $name | Out-Null
            $files.Add([pscustomobject]@{ Source = $item.FullName; Relative = $name })
        }
    }
    foreach ($item in $files) { Write-GloamFile $Root $item.Relative ([IO.File]::ReadAllBytes($item.Source)) | Out-Null }
    Write-Host "Imported theme: $($folder.Name). Select it in Gloam's settings."
}

function Copy-GloamContent([string]$Root, [string[]]$Sources, [string]$Folder) {
    $relative = ConvertTo-GloamRelative $Root $Folder
    $plan = @(); $seen = @{}
    foreach ($source in $Sources) {
        $item = Get-Item -LiteralPath $source
        if ($item.PSIsContainer -or $item.Attributes -band [IO.FileAttributes]::ReparsePoint -or $item.Length -gt 4294967295) { throw 'Choose regular files that fit on a FAT SD card.' }
        if ($seen.ContainsKey($item.Name)) { throw 'Two selected files have the same name.' }; $seen[$item.Name] = $true
        $destination = Get-GloamPath $Root ($relative + '/' + $item.Name)
        if (Test-Path -LiteralPath $destination -PathType Container) { throw 'A folder occupies a selected filename.' }
        $plan += [pscustomobject]@{ Source = $item.FullName; Target = $destination; Name = $item.Name }
    }
    foreach ($item in $plan) {
        if (Test-Path -LiteralPath $item.Target) { Write-Host "Kept existing: $($item.Name)"; continue }
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.Target)) | Out-Null
        $temporary = $item.Target + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
        try { [IO.File]::Copy($item.Source, $temporary); [IO.File]::Move($temporary, $item.Target) }
        finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
        Write-Host "Copied: $($item.Name)"
    }
}

# Match the launcher's own pending import/reset markers before desktop edits.
function Assert-GloamConfigurationReady([string]$Root) {
    foreach ($name in @('configuration/pending.dsconfig','configuration/reset-pending')) {
        if (Test-Path -LiteralPath (Get-GloamPath $Root ('_nds/dsi-frontend/' + $name))) {
            throw 'A launcher configuration recovery is pending. Start Gloam to finish recovery before editing settings on the PC.'
        }
    }
}
