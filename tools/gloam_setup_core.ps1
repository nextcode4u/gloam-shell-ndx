# SPDX-License-Identifier: GPL-3.0-or-later
# Shared setup operations. Dot-sourcing this file never opens a window or writes files.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-GloamRoot([string]$Target) {
    if (-not $Target -or -not [IO.Directory]::Exists($Target)) {
        throw 'Choose an existing SD card or staging folder first.'
    }
    $root = [IO.Path]::GetFullPath($Target)
    if ($env:SystemDrive -and $root.TrimEnd('\') -eq $env:SystemDrive) {
        throw 'Choose the SD card or a staging folder, not the Windows system drive.'
    }
    $node = [IO.DirectoryInfo]::new($root)
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
    $path = [IO.Path]::GetFullPath([IO.Path]::Combine($root, $Relative))
    if (-not $path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw 'Path leaves the selected SD folder.' }
    $cursor = $path
    while ($cursor.Length -ge $root.TrimEnd('\').Length) {
        # Avoid repeated PowerShell provider invocations for every artwork path.
        # Still inspect every ancestor on every call: no cached link decisions.
        $attributes = $null
        try { $attributes = [IO.File]::GetAttributes($cursor) }
        catch [IO.FileNotFoundException] { }
        catch [IO.DirectoryNotFoundException] { }
        if ($null -ne $attributes) {
            if ($attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked SD paths are not supported.' }
            if ($cursor -ne $path -and -not ($attributes -band [IO.FileAttributes]::Directory)) { throw 'A parent path is a file.' }
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
    $releases = Invoke-RestMethod ('https://api.github.com/repos/nextcode4u/gloam-shell-ndx/releases?per_page=100&setup_refresh=' + [DateTime]::UtcNow.Ticks) -UserAgent 'Gloam-Setup' -TimeoutSec 30
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

# Receipts describe installed originals, never personal theme edits. Unknown or
# malformed receipts cannot authorize replacing files.
function Read-GloamThemeReceipt([string]$Root) {
    $result = @{}
    $path = Get-GloamPath $Root '_nds/dsi-frontend/setup-theme-hashes.json'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $result }
    try {
        if ((Get-Item -LiteralPath $path).Length -gt 1MB) { throw 'Oversized theme receipt.' }
        $data = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        foreach ($entry in $data.PSObject.Properties) {
            if ($entry.Name -notmatch '^_nds/dsi-frontend/themes/[A-Za-z0-9_-]+/.+$' -or
                [string]$entry.Value -notmatch '^[a-f0-9]{64}$') { throw 'Invalid theme receipt.' }
            Get-GloamPath $Root $entry.Name | Out-Null
            $result[$entry.Name] = [string]$entry.Value
        }
    } catch { Write-Host 'Theme receipt unavailable; preserving unrecognized existing themes.'; return @{} }
    return $result
}

function Get-GloamLegacyThemeHashes {
    # Original bundled manifests before icon packs (source revision 154e82a).
    # Exact path/hash matches let existing untouched installs migrate safely.
    return @{
        '_nds/dsi-frontend/themes/copper/theme.ini' = 'd878b4ae94208fca50621f81b258b2228e5afea1230c800bbfa40cc1946d6fc8'
        '_nds/dsi-frontend/themes/eclipse/theme.ini' = '9b966a188ad0aef6fee225a9038e5fdd1658bf8e1c7327826d9739a28441143f'
        '_nds/dsi-frontend/themes/ember/theme.ini' = '60ffeb8e53e1397261e787d0c5b49e72c0fb1290ab4c5fc19b1c84da2a754f16'
        '_nds/dsi-frontend/themes/folio/theme.ini' = 'd29aca1fa86893ea89502051fe5913e7ef4eacabe99044a2cdf56d488b5b8a82'
        '_nds/dsi-frontend/themes/garnet/theme.ini' = '810d13799e38c09e01b60ab584b04455a8a581525ae19b6e5ef32da5b1277b95'
        '_nds/dsi-frontend/themes/glacier/theme.ini' = 'c13b5523ab16cab71f061fa980241ccf42571c852ec7a7ebfb1a6d40f045e053'
        '_nds/dsi-frontend/themes/nocturne/theme.ini' = '4a1cbfcb3fc6f3e345f310a2382aa32427506f69465dd436b3ea1f9d7312a8eb'
        '_nds/dsi-frontend/themes/phosphor/theme.ini' = 'bcd41744e2345afd82ffb9eba037e731ce784a940b6dc146ffe7180984023d13'
        '_nds/dsi-frontend/themes/prism/theme.ini' = '628ebdd46b25b24437eb5f47e757b86c741b966b17c96f8b2eb331896ce823fe'
        '_nds/dsi-frontend/themes/slate/theme.ini' = '8fabc805a75b8321faf5547faa8d0540c05c07a0ad540ed8c385679a3f1ec30a'
        '_nds/dsi-frontend/themes/tide/theme.ini' = '1445ae1909ec665da8d26f01882cc7ed4b694b30e119d25c9d0a0e802f3ea39d'
    }
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
        $receipt = Read-GloamThemeReceipt $root
        $legacy = Get-GloamLegacyThemeHashes
        $blockedThemes = @{}
        $themeHashes = @{}
        $existingHashes = @{}
        # Decide per pack before writing: a customized manifest or asset keeps
        # the entire pack intact, avoiding mixed old/new assets.
        foreach ($item in $plan) {
            if ($item.Folder -or $item.Relative -notmatch '^(_nds/dsi-frontend/themes/[A-Za-z0-9_-]+)/') { continue }
            $pack = $Matches[1]
            $stream = $item.Entry.Open(); $hash = [Security.Cryptography.SHA256]::Create()
            try { $incoming = ([BitConverter]::ToString($hash.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
            finally { $hash.Dispose(); $stream.Dispose() }
            $themeHashes[$item.Relative] = $incoming
            $path = Get-GloamPath $root $item.Relative
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
            $current = Get-GloamHash $path
            $existingHashes[$item.Relative] = $current
            $original = $receipt.ContainsKey($item.Relative) -and $receipt[$item.Relative] -eq $current
            $oldManifest = $legacy.ContainsKey($item.Relative) -and $legacy[$item.Relative] -eq $current
            if ($current -ne $incoming -and (-not $UpdateRuntime -or (-not $original -and -not $oldManifest))) {
                $blockedThemes[$pack] = $true
            }
        }
        foreach ($pack in $blockedThemes.Keys) { Write-Host "Preserved customized/unrecognized theme: $pack" }
        # Validate the entire destination plan before creating any SD files.
        foreach ($item in $plan) {
            $path = Get-GloamPath $root $item.Relative
            $themeFile = $themeHashes.ContainsKey($item.Relative)
            if ($item.Relative -match '^(_nds/dsi-frontend/themes/[A-Za-z0-9_-]+)(/|$)' -and
                $blockedThemes.ContainsKey($Matches[1])) { continue }
            if ($item.Folder) { [IO.Directory]::CreateDirectory($path) | Out-Null; continue }
            if ($themeFile -and $existingHashes.ContainsKey($item.Relative)) {
                if ((Get-GloamHash $path) -ne $existingHashes[$item.Relative]) { throw 'Theme changed during update; retry after closing its editor.' }
                if ($existingHashes[$item.Relative] -eq $themeHashes[$item.Relative]) {
                    $receipt[$item.Relative] = $themeHashes[$item.Relative]
                    continue
                }
            }
            $stream = $item.Entry.Open(); $memory = [IO.MemoryStream]::new()
            try { $stream.CopyTo($memory); [byte[]]$bytes = $memory.ToArray() }
            finally { $stream.Dispose(); $memory.Dispose() }
            $owned = @('BOOT.NDS','_nds/nds-bootstrap-release.nds',
                '_nds/dsi-frontend/loaders/slot1launch.nds','_nds/dsi-frontend/install/gloam-forwarder.dsi',
                '_nds/dsi-frontend/install/gloam-forwarder.tmd')
            $replace = ($themeFile -and $UpdateRuntime) -or $item.Relative -ceq 'BOOT.NDS' -or ($UpdateRuntime -and $item.Relative -cin $owned)
            if (Write-GloamFile $root $item.Relative $bytes $replace) { Write-Host "Installed: $($item.Relative)" }
            else { Write-Host "Kept existing: $($item.Relative)" }
            if ($themeFile) { $receipt[$item.Relative] = $themeHashes[$item.Relative] }
        }
        if ($themeHashes.Count) {
            $json = $receipt | ConvertTo-Json -Depth 3
            Write-GloamFile $root '_nds/dsi-frontend/setup-theme-hashes.json' ([Text.UTF8Encoding]::new($false).GetBytes($json)) $true | Out-Null
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
    $report=Test-GloamTheme $Source
    if (-not $report.Valid) { throw ($report.Errors -join "`n") }
    foreach ($warning in $report.Warnings) { Write-Host $warning }
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

# Read-only details for one selected catalog entry; missing metadata is never
# interpreted as proof that an emulator needs no firmware.
function Get-GloamAddonDetails($App, [string]$Root) {
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('Configured formats and default folders')
    if ($App.PSObject.Properties.Name -contains 'content_guidance' -and $App.content_guidance) { $lines.Add($App.content_guidance) }
    else { $lines.Add('Not recorded in this catalog; consult the application documentation.') }
    $lines.Add('')
    $lines.Add('BIOS / firmware')
    if ($App.PSObject.Properties.Name -contains 'bios' -and $App.bios) { $lines.Add($App.bios) }
    else { $lines.Add('Requirements not recorded; do not assume no BIOS is needed.') }
    $requirements = @(if ($App.PSObject.Properties.Name -contains 'support_requirements') { $App.support_requirements })
    foreach ($requirement in $requirements) {
        $locations = @($requirement.any_of | ForEach-Object { @($_) -join ' + ' }) -join ' OR '
        $status = 'Not checked (choose an SD folder)'
        if ($Root) {
            $status = if (Test-GloamFirmwareRequirement $requirement $Root) { 'Found' } else { 'Missing or wrong size at the listed locations' }
        }
        $lines.Add("$($requirement.label) [$($requirement.scope)]: $status")
        $lines.Add('Locations: ' + $locations)
    }
    if ($App.id -eq 'gameyob-dsi') {
        $status = 'not checked'
        if ($Root) {
            $file = Get-Item -LiteralPath (Get-GloamPath $Root 'roms/bios/gbc_bios.bin') -ErrorAction SilentlyContinue
            $status = if ($file -and -not $file.PSIsContainer -and $file.Length) { 'file present (existing files are preserved)' } else { 'not installed yet' }
        }
        $lines.Add('Optional free SameBoot supplied by installer: ' + $status)
    }
    $lines.Add('Checks cover listed paths and known sizes, not firmware authenticity. BIOS selected inside an emulator may need checking there.')
    if ($App.PSObject.Properties.Name -contains 'notes' -and $App.notes) { $lines.Add(''); $lines.Add($App.notes) }
    return $lines -join "`n"
}

function Test-GloamFirmwareRequirement($requirement, [string]$Root) {
    foreach ($alternative in $requirement.any_of) {
        $valid = @($alternative).Count -gt 0
        foreach ($relative in $alternative) {
            $path = Get-GloamPath $Root $relative
            $file = Get-Item -LiteralPath $path -ErrorAction SilentlyContinue
            $size = $null
            if ($requirement.PSObject.Properties.Name -contains 'sizes') { $size = $requirement.sizes.PSObject.Properties[[IO.Path]::GetFileName($relative)] }
            if (-not $file -or $file.PSIsContainer -or $file.Length -eq 0 -or ($size -and $file.Length -ne $size.Value)) { $valid = $false }
        }
        if ($valid) { return $true }
    }
    return $false
}

function Get-GloamAddonStatus($App, [string]$Root) {
    $installation = 'Not checked'
    if ($Root) {
        $executables = @($App.files | Where-Object { $_.destination -match '\.(nds|srl)$' })
        $present = @($executables | Where-Object {
            $file = Get-Item -LiteralPath (Get-GloamPath $Root $_.destination) -ErrorAction SilentlyContinue
            $file -and -not $file.PSIsContainer -and $file.Length -gt 0
        }).Count
        $installation = if (-not $executables.Count) { 'Unknown' } elseif (-not $present) { 'Not installed' } elseif ($present -lt $executables.Count) { 'Partial install' } else { 'Installed' }
    }
    $mode = if ($App.PSObject.Properties.Name -contains 'bios_mode') { $App.bios_mode } else { 'unknown' }
    $bios = switch ($mode) {
        'none' { 'Not needed' }
        'embedded' { 'Embedded in app' }
        'optional' { 'Optional - check app' }
        'manual' { 'Check in emulator' }
        'required-manual' { 'Required - check app' }
        'saving-manual' { 'Saving needs BIOS' }
        'required' { 'Required (unchecked)' }
        'conditional' { 'Depends on system' }
        default { 'Unknown' }
    }
    $requirements = @(if ($App.PSObject.Properties.Name -contains 'support_requirements') { $App.support_requirements })
    if ($Root -and $requirements.Count) {
        $missing = @($requirements | Where-Object { -not (Test-GloamFirmwareRequirement $_ $Root) })
        if (-not $missing.Count) { $bios = if ($mode -eq 'optional') { 'Optional - present' } else { 'Present' } }
        elseif (@($missing | Where-Object { $_.scope -eq 'required' }).Count) { $bios = 'Required - missing' }
        elseif ($mode -eq 'optional') { $bios = 'Optional - missing' }
        else { $bios = 'System BIOS missing' }
    }
    return [pscustomobject]@{ Installation = $installation; BiosStatus = $bios }
}

# Read-only desktop checks mirror the package's bounded paths and optional PNGs.
function Test-GloamTheme([string]$Source) {
    $root=Get-GloamRoot $Source
    $errors=[Collections.Generic.List[string]]::new(); $warnings=[Collections.Generic.List[string]]::new()
    $icons=[Collections.Generic.List[object]]::new()
    $manifest=Get-GloamPath $root 'theme.ini'
    if (-not [IO.File]::Exists($manifest) -or ([IO.FileInfo]::new($manifest)).Length -gt 4096) { throw 'Theme manifest is missing or exceeds 4 KiB.' }
    $data=Read-GloamIni $manifest
    $allowed=@{theme=@('version','name');colors=@('background','surface','line','text','muted','accent','ink');fonts=@('ui');audio=@('music','cursor','confirm','back');backgrounds=@('top','bottom','strength');layout=@('view','icon_size','row_padding','large_grid_columns');icons=@('directory')}
    foreach ($section in $data.Keys) {
        if (-not $allowed.ContainsKey($section)) { $errors.Add("Unknown section: $section"); continue }
        foreach ($key in $data[$section].Keys) { if ($key -notin $allowed[$section]) { $errors.Add("Unknown key: $section/$key") } }
    }
    if (-not $data.Contains('theme') -or $data['theme']['version'] -ne '1') { $errors.Add('Expected theme version 1.') }
    if ($data.Contains('colors')) { foreach ($key in $data['colors'].Keys) { if ($data['colors'][$key] -notmatch '^#[0-9A-Fa-f]{6}$') { $errors.Add("Invalid colour: $key") } } }
    if ($data.Contains('layout')) {
        foreach ($pair in @(@('view',@('classic','extended','list','grid','shelf','index')),@('icon_size',@('small','large')))) {
            if ($data['layout'].Contains($pair[0]) -and $data['layout'][$pair[0]] -notin $pair[1]) { $errors.Add("Invalid layout: $($pair[0])") }
        }
        foreach ($key in @('row_padding','large_grid_columns')) { if ($data['layout'].Contains($key)) {
            $value=0; $minimum=if ($key -eq 'row_padding') {0} else {2}; $maximum=if ($key -eq 'row_padding') {12} else {4}
            if (-not [int]::TryParse($data['layout'][$key],[ref]$value) -or $value -lt $minimum -or $value -gt $maximum) { $errors.Add("Invalid layout: $key") }
        } }
    }
    foreach ($section in @('fonts','audio','backgrounds')) { if ($data.Contains($section)) {
        foreach ($key in $data[$section].Keys) {
            $value=$data[$section][$key]
            if ($section -eq 'backgrounds' -and $key -eq 'strength') { $strength=0; if (-not [int]::TryParse($value,[ref]$strength) -or $strength -lt 0 -or $strength -gt 35) { $errors.Add('Background strength must be 0-35.') }; continue }
            if ($section -eq 'fonts' -and $value -eq 'builtin') { continue }
            try { $path=Get-GloamPath $root $value; if (-not [IO.File]::Exists($path)) { if ($section -eq 'fonts') { $errors.Add("Missing font: $value") } else { $warnings.Add("Missing $section asset: $value (fallback)") } } }
            catch { $errors.Add("Unsafe asset path: $section/$key") }
        }
    } }
    if ($data.Contains('icons')) {
        try {
            $directory=Get-GloamPath $root $data['icons']['directory']
            Add-Type -AssemblyName PresentationCore
            foreach ($name in @('file','folder','game','application','audio','video','book','text','image','save','config','archive','shortcut','system','missing','settings','camera','sound','pictochat','download-play','system-menu','slot1')) {
                $relative=$data['icons']['directory']+'/'+$name+'.png'
                $path=Get-GloamPath $root $relative
                if (-not [IO.File]::Exists($path)) { $warnings.Add("$name icon missing: built-in fallback"); continue }
                try {
                    if (([IO.FileInfo]::new($path)).Length -gt 65536) { throw 'larger than 64 KiB' }
                    $stream=[IO.File]::OpenRead($path)
                    try { $decoder=[Windows.Media.Imaging.PngBitmapDecoder]::new($stream,[Windows.Media.Imaging.BitmapCreateOptions]::PreservePixelFormat,[Windows.Media.Imaging.BitmapCacheOption]::OnLoad); $image=$decoder.Frames[0]; $image.Freeze() }
                    finally { $stream.Dispose() }
                    if ($image.PixelWidth -ne 32 -or $image.PixelHeight -ne 32) { throw 'must be 32 x 32 pixels' }
                    $icons.Add([pscustomobject]@{Name=$name;Image=$image})
                } catch { $warnings.Add("$name icon invalid: $($_.Exception.Message); built-in fallback") }
            }
        } catch { $errors.Add("Invalid icon directory: $($_.Exception.Message)") }
    }
    return [pscustomobject]@{Valid=($errors.Count -eq 0);Errors=$errors.ToArray();Warnings=$warnings.ToArray();Icons=$icons.ToArray();Source=$root}
}

function Get-GloamCollections([string]$Root) {
    $data=Read-GloamIni (Get-GloamPath $Root '_nds/dsi-frontend/collections.ini')
    foreach ($key in $data.Keys) { if ($key -like 'collection.*') {
        $paths=@($data[$key].Keys | Where-Object { $_ -match '^item[0-9]+$' } | Sort-Object { [int]$_.Substring(4) } | ForEach-Object { $data[$key][$_] })
        [pscustomobject]@{Id=$key.Substring(11);Label=$data[$key]['label'];Paths=($paths -join "`r`n")}
    } }
}
function Save-GloamCollection([string]$Root,[string]$Id,[string]$Label,[string]$Paths) {
    $generatedId=-not $Id
    if (-not $Id -and $Label) { $Id=($Label.ToLowerInvariant() -replace '[^a-z0-9]+','-').Trim('-'); if (-not $Id) { $Id='collection' }; if ($Id.Length -gt 31) { $Id=$Id.Substring(0,31) } }
    if ($Id -cnotmatch '^[a-z0-9_-]{1,31}$' -or -not $Label -or [Text.Encoding]::UTF8.GetByteCount($Label) -ge 64 -or $Label -match '[\x00-\x1f]') { throw 'Use a lowercase ID (31 characters maximum) and a short collection label.' }
    $file='_nds/dsi-frontend/collections.ini'; $data=Read-GloamIni (Get-GloamPath $Root $file)
    if (-not $data.Contains('collections')) { $data['collections']=[ordered]@{version='1'} }
    if ($data['collections']['version'] -ne '1') { throw 'Unsupported collections version.' }
    $entries=[ordered]@{label=$Label}; $seen=@{}; $index=0
    foreach ($line in ($Paths -split "`r?`n")) {
        $value=$line.Trim(); if (-not $value) { continue }
        if ($value -notin @('system:settings','system:camera','system:sound','system:pictochat','system:download-play','system:menu','system:slot1')) {
            $relative=ConvertTo-GloamRelative $Root $value; Get-GloamPath $Root $relative | Out-Null; $value='sd:/'+$relative
        }
        if ([Text.Encoding]::UTF8.GetByteCount($value) -ge 768) { throw 'Collection path is too long.' }
        if ($seen.ContainsKey($value)) { continue }; $seen[$value]=$true
        if ($index -ge 32) { throw 'A collection supports at most 32 items.' }
        $entries['item'+$index]=$value; $index++
    }
    if ($generatedId -and $data.Contains('collection.'+$Id)) { throw 'That collection already exists. Select it to edit, or enter another ID.' }
    $data['collection.'+$Id]=$entries
    $groups=@($data.Keys | Where-Object { $_ -like 'collection.*' })
    if ($groups.Count -gt 8) { throw 'At most eight named collections are supported.' }
    $all=@{}
    foreach ($key in $groups) { foreach ($item in $data[$key].Keys) { if ($item -match '^item[0-9]+$') { $all[$data[$key][$item]]=$true } } }
    if ($all.Count -gt 32) { throw 'Collections share a pool of at most 32 distinct targets.' }
    $bytes=ConvertTo-GloamIni $data
    Write-GloamFile $Root $file $bytes $true | Out-Null
}
function Remove-GloamCollection([string]$Root,[string]$Id) {
    if ($Id -cnotmatch '^[a-z0-9_-]{1,31}$') { throw 'Choose an existing collection.' }
    $file='_nds/dsi-frontend/collections.ini'; $data=Read-GloamIni (Get-GloamPath $Root $file)
    if (-not $data.Contains('collection.'+$Id)) { throw 'Collection no longer exists. Reload the SD folder.' }
    $data.Remove('collection.'+$Id)
    Write-GloamFile $Root $file (ConvertTo-GloamIni $data) $true | Out-Null
}
