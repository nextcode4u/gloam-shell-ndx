# Downloads optional Gloam applications directly from upstream. No Python required.
[CmdletBinding()]
param(
    [switch]$Interactive,
    [string]$Target,
    [string]$Manifest,
    [string]$Cache = (Join-Path ([Environment]::GetFolderPath('UserProfile')) '.gloam-addons-cache'),
    [string[]]$Only,
    [switch]$List,
    [switch]$Noncommercial,
    [switch]$Overwrite,
    [switch]$Offline,
    [switch]$DryRun,
    [switch]$CheckUpdates,
    [switch]$CheckSupport,
    [switch]$Update,
    [string]$Catalog,
    [switch]$Upstream
)
$MenuMode = $Interactive -or $PSBoundParameters.Count -eq 0
if (-not $Manifest) { $Manifest = Join-Path $PSScriptRoot 'downloads.json' }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
$MaxDownload = 64MB
$ReaderFonts = @('LiberationSerif-Regular.ttf', 'LiberationSerif-Bold.ttf',
    'LiberationSerif-Italic.ttf', 'LiberationSerif-BoldItalic.ttf', 'LiberationSans-Regular.ttf') |
    ForEach-Object { 'data/dslibris/font/' + $_ }

$AddonProfiles = @('_nds/dsi-frontend/plugins/dslibris-books.ini',
    '_nds/dsi-frontend/plugins/dslibris-documents.ini',
    '_nds/dsi-frontend/plugins/picodrivetwl.ini')

function Get-Digest([byte[]]$Data) {
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash($Data))).Replace('-', '').ToLowerInvariant() }
    finally { $hash.Dispose() }
}

function Convert-ReviewedCatalog($Data) {
    if ($Data.format -ne 1) { throw 'Unsupported update catalog' }
    if ($Data.PSObject.Properties.Name -contains 'updates') { return $Data.updates }
    foreach ($app in $Data.applications) {
        if ($app.PSObject.Properties.Name -notcontains 'package') { continue }
        $files = @($app.package.files)
        if (-not $files.Count -or @($files | Where-Object { $_.format -notin @('raw', 'zip', 'tar.gz') }).Count) { continue }
        $nc = $app.redistribution -eq 'noncommercial'
        if ($app.PSObject.Properties.Name -contains 'download_noncommercial') { $nc = $nc -or $app.download_noncommercial }
        $defaults = @()
        if ($app.package.PSObject.Properties.Name -contains 'defaults') {
            $defaults = @($app.package.defaults | ForEach-Object {
                [pscustomobject]@{ format = 'inline'; content = $_.content; sha256 = $_.sha256; destination = $_.destination }
            })
        }
        $item = [ordered]@{ id = $app.id; version = $app.version; repository = $app.repository; terms = $app.license; noncommercial = $nc; files = @($files) + @($defaults) }
        $item.bios = if ($app.PSObject.Properties.Name -contains 'bios') { $app.bios } else { '' }
        $item.bios_mode = if ($app.PSObject.Properties.Name -contains 'bios_mode') { $app.bios_mode } else { 'unknown' }
        $item.content_guidance = if ($app.PSObject.Properties.Name -contains 'content_guidance') { $app.content_guidance } else { '' }
        $item.support_requirements = if ($app.PSObject.Properties.Name -contains 'support_requirements') { @($app.support_requirements) } else { @() }
        if ($app.PSObject.Properties.Name -contains 'experimental' -and $app.experimental) {
            $item.experimental = $true
            $item.notes = if ($app.PSObject.Properties.Name -contains 'compatibility_notes') { $app.compatibility_notes } else { 'Experimental build' }
        }
        if ($app.package.PSObject.Properties.Name -contains 'game_configs') { $item.game_configs = $app.package.game_configs }
        if ($app.id -in @('snemulds', 'snemulds-shim')) { $item.requires = @('snemulds', 'snemulds-shim') }
        if ($app.release -and $app.release.Contains('/releases/tag/')) { $item.release_tag = ($app.release -split '/releases/tag/', 2)[1] }
        [pscustomobject]$item
    }
}

function Get-RemoteJson([string]$Url) {
    if (-not $Url.StartsWith('https://')) { throw 'Update catalogs require HTTPS' }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    return Invoke-RestMethod -Uri $Url -UserAgent 'Gloam-addon-installer' -TimeoutSec 30
}

function Get-UpdateApps($Document) {
    if ($Catalog) { return Convert-ReviewedCatalog (Get-Content -Raw -LiteralPath $Catalog | ConvertFrom-Json) }
    if (-not $Offline) {
        $url = 'https://raw.githubusercontent.com/nextcode4u/gloam-shell-ndx/main/config/addon-dependencies.json'
        if ($Document.PSObject.Properties.Name -contains 'catalog_url') { $url = $Document.catalog_url }
        $remote = $null
        try { $remote = Get-RemoteJson $url }
        catch { Write-Host "Remote catalog unavailable ($($_.Exception.Message)); using packaged reviewed pins." }
        if ($null -ne $remote) {
            $apps = @(Convert-ReviewedCatalog $remote)
            if ($Document.PSObject.Properties.Name -contains 'reviewed' -and
                ($remote.PSObject.Properties.Name -notcontains 'reviewed' -or $remote.reviewed -lt $Document.reviewed)) {
                Write-Host 'Remote catalog is older than this package; keeping packaged reviewed pins.'
            } else { return $apps }
        }
    }
    if ($Document.PSObject.Properties.Name -contains 'updates') { return $Document.updates }
    return $Document.applications
}

function Get-SelectedApps($Applications, [string[]]$Names, [bool]$AllowNoncommercial = [bool]$Noncommercial) {
    $apps = @{}
    foreach ($app in $Applications) {
        if ($apps.ContainsKey($app.id)) { throw 'Duplicate application ID' }
        $apps[$app.id] = $app
    }
    $wanted = @()
    if ($Names) { $wanted = @($Names | Select-Object -Unique) }
    else { $wanted = @($Applications | ForEach-Object { $_.id }) }
    for ($index = 0; $index -lt $wanted.Count; $index++) {
        $name = $wanted[$index]
        if (-not $apps.ContainsKey($name)) { throw "No pinned download for: $name; use -List for manual entries" }
        $app = $apps[$name]
        if ($app.PSObject.Properties.Name -contains 'experimental' -and $app.experimental -and -not $Names) { continue }
        if ($app.noncommercial -and -not $AllowNoncommercial) {
            if ($Names) { throw "$name requires -Noncommercial" }
            continue
        }
        $app
        if ($app.PSObject.Properties.Name -contains 'requires') {
            foreach ($required in $app.requires) {
                if (-not $apps.ContainsKey($required)) { throw "Missing paired application: $required" }
                if ($required -notin $wanted) { $wanted += $required }
            }
        }
    }
}

function Read-Exact($Stream, [byte[]]$Buffer, [int]$Count) {
    $offset = 0
    while ($offset -lt $Count) {
        $read = $Stream.Read($Buffer, $offset, $Count - $offset)
        if (-not $read) { throw 'Truncated tar archive' }
        $offset += $read
    }
}

function Read-TarMember([byte[]]$Data, [string]$Member) {
    # Read bounded regular USTAR members directly; never extract to the host.
    $input = [IO.MemoryStream]::new($Data, $false)
    $gzip = [IO.Compression.GZipStream]::new($input, [IO.Compression.CompressionMode]::Decompress)
    $found = $null
    $total = 0L
    try {
        $header = [byte[]]::new(512)
        $buffer = [byte[]]::new(65536)
        while ($true) {
            Read-Exact $gzip $header 512
            if ($header[0] -eq 0) { break }
            $name = [Text.Encoding]::UTF8.GetString($header, 0, 100).TrimEnd([char]0)
            $prefix = [Text.Encoding]::UTF8.GetString($header, 345, 155).TrimEnd([char]0)
            if ($prefix) { $name = "$prefix/$name" }
            $sizeText = [Text.Encoding]::ASCII.GetString($header, 124, 12).Trim([char]0, [char]32)
            $size = [Convert]::ToInt64($sizeText, 8)
            $total += $size + 512
            if ($size -lt 0 -or $total -gt 256MB) { throw 'Oversized tar archive' }
            $matched = $name -ceq $Member
            if ($matched -and ($null -ne $found -or $header[156] -notin @(0, 48) -or $size -gt $MaxDownload)) {
                throw 'Duplicate, non-regular or oversized tar member'
            }
            $output = [IO.MemoryStream]::new()
            try {
                $remaining = $size
                while ($remaining -gt 0) {
                    $count = [int][Math]::Min($remaining, $buffer.Length)
                    Read-Exact $gzip $buffer $count
                    if ($matched) { $output.Write($buffer, 0, $count) }
                    $remaining -= $count
                }
                if ($matched) { $found = $output.ToArray() }
            } finally { $output.Dispose() }
            $padding = [int]((512 - ($size % 512)) % 512)
            if ($padding) { Read-Exact $gzip $buffer $padding }
        }
        if ($null -eq $found) { throw 'Missing tar archive member' }
        return ,$found
    } finally { $gzip.Dispose(); $input.Dispose() }
}

function Get-SdPath([string]$Root, [string]$Name, [switch]$IgnoreCase) {
    if ($Name -match '(^/|\\|:)' -or @($Name.Split('/') | Where-Object { $_ -in @('', '.', '..') }).Count) {
        throw "Unsafe destination: $Name"
    }
    $path = $Root
    foreach ($part in $Name.Split('/')) {
        $candidate = Join-Path $path $part
        if ($IgnoreCase -and -not (Test-Path -LiteralPath $candidate) -and (Test-Path -LiteralPath $path -PathType Container)) {
            $nameMatches = @(Get-ChildItem -LiteralPath $path -Force | Where-Object { $_.Name -ieq $part })
            if ($nameMatches.Count -gt 1) { throw "Ambiguous SD filename: $Name" }
            if ($nameMatches.Count) { $candidate = $nameMatches[0].FullName }
        }
        $path = $candidate
        if (Test-Path -LiteralPath $path) {
            if ((Get-Item -Force -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Destination contains a link: $path"
            }
        }
    }
    return $path
}

function Get-Destination([string]$Root, [string]$Name) {
    if ($Name -cnotmatch '^_nds/dsi-frontend/plugins/apps/.+\.(nds|srl|ini)$' -and
        $Name -cnotmatch '^_gba/configs/[A-Z0-9]{6}\.json$' -and
        $Name -cnotin $ReaderFonts -and $Name -cnotin $AddonProfiles -and
        $Name -cne 'roms/ScummVM/scummvm.nds' -and $Name -cne 'roms/bios/gbc_bios.bin') { throw "Destination outside add-on folders: $Name" }
    return Get-SdPath $Root $Name
}

function Test-Support($Apps, [string]$Folder, [string[]]$Names) {
    if (-not $Names) { $Names = @(Get-MenuInstalledApps $Apps $Folder | ForEach-Object { $_.id }) }
    if (-not $Names.Count) { Write-Output 'No matching applications are installed.'; return }
    foreach ($app in @(Get-SelectedApps $Apps $Names $true)) {
        $note = if ($app.PSObject.Properties.Name -contains 'bios' -and $app.bios) { $app.bios } else { 'No firmware metadata in this catalog.' }
        Write-Output "$($app.id): $note"
        foreach ($item in @($app.files | Where-Object { $_.destination -notmatch '\.(nds|srl)$' })) {
            $path = Get-Destination $Folder $item.destination
            if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -Force -LiteralPath $path).Length -eq 0) {
                Write-Output "  MISSING support file: $($item.destination)"
            }
        }
        $requirements = if ($app.PSObject.Properties.Name -contains 'support_requirements') { @($app.support_requirements) } else { @() }
        foreach ($requirement in $requirements) {
            if (-not @($requirement.any_of).Count) { throw 'Empty firmware requirement' }
            $found = $null
            foreach ($alternative in $requirement.any_of) {
                $paths = @($alternative)
                if (-not $paths.Count) { throw 'Empty firmware requirement' }
                $valid = $true
                foreach ($name in $paths) {
                    $path = Get-SdPath $Folder $name -IgnoreCase
                    $size = $null
                    if ($requirement.PSObject.Properties.Name -contains 'sizes') {
                        $property = $requirement.sizes.PSObject.Properties[[IO.Path]::GetFileName($name)]
                        if ($property) { $size = $property.Value }
                    }
                    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { $valid = $false }
                    else {
                        $length = (Get-Item -Force -LiteralPath $path).Length
                        if (-not $length -or ($size -and $length -ne $size)) { $valid = $false }
                    }
                }
                if ($valid) { $found = $paths; break }
            }
            if ($found) { Write-Output "  FOUND $($requirement.label): $($found -join ', ')" }
            else {
                $scope = if ($requirement.PSObject.Properties.Name -contains 'scope') { $requirement.scope } else { 'required' }
                $locations = @($requirement.any_of | ForEach-Object { @($_) -join ' + ' }) -join ' or '
                Write-Output "  MISSING ($scope) $($requirement.label): $locations"
                if ($requirement.PSObject.Properties.Name -contains 'sizes') {
                    $sizes = @($requirement.sizes.PSObject.Properties | ForEach-Object { "$($_.Name)=$($_.Value) bytes" }) -join ', '
                    Write-Output "    Expected sizes: $sizes"
                }
            }
        }
        Write-Output '  Presence/size checks do not establish firmware authenticity or game compatibility.'
    }
}

function Write-CompleteFile([string]$Path, [byte[]]$Data) {
    $temporary = Join-Path ([IO.Path]::GetDirectoryName($Path)) ([IO.Path]::GetRandomFileName())
    try {
        [IO.File]::WriteAllBytes($temporary, $Data)
        if ([IO.File]::Exists($Path)) { [IO.File]::Replace($temporary, $Path, [NullString]::Value) }
        else { [IO.File]::Move($temporary, $Path) }
    } finally { if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) } }
}

function Get-Download($Item) {
    if ($Item.asset -cnotmatch '^[A-Za-z0-9_.-]+$' -or $Item.asset -in @('.', '..')) { throw 'Unsafe cache filename' }
    if ($Item.sha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Invalid download hash' }
    if (-not $Item.url.StartsWith('https://')) { throw 'Downloads require HTTPS' }
    $path = Join-Path $Cache "$($Item.sha256)-$($Item.asset)"
    $legacy = Join-Path $Cache $Item.asset
    if (-not (Test-Path -LiteralPath $path) -and (Test-Path -LiteralPath $legacy)) {
        if ((Get-Item -Force -LiteralPath $legacy).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Cache entry is a link' }
        if ((Get-Digest ([IO.File]::ReadAllBytes($legacy))) -eq $Item.sha256) { $path = $legacy }
        elseif ($Offline) { throw "Download cache hash mismatch: $($Item.asset)" }
    }
    if (Test-Path -LiteralPath $path) {
        if ((Get-Item -Force -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Cache entry is a link' }
        $bytes = [IO.File]::ReadAllBytes($path)
        if ($bytes.Length -le $MaxDownload -and (Get-Digest $bytes) -eq $Item.sha256) { return ,$bytes }
        if ($Offline) { throw "Download cache hash mismatch: $($Item.asset)" }
        Write-Host "Replacing incomplete or corrupt cache: $($Item.asset)"
    } elseif ($Offline) { throw "Offline cache missing: $($Item.asset)" }
    if (-not $Offline) {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $request = [Net.WebRequest]::Create($Item.url)
        $request.Timeout = 60000
        $request.UserAgent = 'Gloam-addon-installer'
        $response = $request.GetResponse()
        $stream = $response.GetResponseStream()
        $memory = [IO.MemoryStream]::new()
        try {
            $buffer = [byte[]]::new(65536)
            while (($count = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                $memory.Write($buffer, 0, $count)
                if ($memory.Length -gt $MaxDownload) { throw 'Oversized download' }
            }
            $bytes = $memory.ToArray()
        } finally { $memory.Dispose(); $stream.Dispose(); $response.Dispose() }
        if ((Get-Digest $bytes) -ne $Item.sha256) { throw "Download hash mismatch: $($Item.asset)" }
        [IO.Directory]::CreateDirectory($Cache) | Out-Null
        Write-CompleteFile $path $bytes
    }
    if ($bytes.Length -gt $MaxDownload -or (Get-Digest $bytes) -ne $Item.sha256) { throw "Download size/hash mismatch: $($Item.asset)" }
    return ,$bytes
}

function Read-MenuInput([string]$Prompt) {
    if ([Console]::IsInputRedirected) {
        Write-Host -NoNewline ($Prompt + ': ')
        $answer = [Console]::ReadLine()
    } else { $answer = Read-Host $Prompt }
    if ($null -eq $answer) { throw [OperationCanceledException]::new('Input closed') }
    return $answer.Trim()
}

function Read-MenuTarget {
    Write-Host "`nChoose the SD card root or an existing staging folder."
    $drives = @()
    try {
        $drives = @([IO.DriveInfo]::GetDrives() | Where-Object {
            $_.IsReady -and $_.DriveType -eq [IO.DriveType]::Removable
        } | Sort-Object Name)
    } catch { Write-Host 'Enter a folder path to select the target.' }
    for ($i = 0; $i -lt $drives.Count; $i++) {
        Write-Host ('  {0}. {1} ({2:N1} GB free)' -f ($i + 1), $drives[$i].Name, ($drives[$i].AvailableFreeSpace / 1GB))
    }
    while ($true) {
        $answer = (Read-MenuInput 'Drive number or folder path (blank to go back)').Trim('"')
        if (-not $answer) { return $null }
        $number = 0
        if ([int]::TryParse($answer, [ref]$number)) {
            if ($number -lt 1 -or $number -gt $drives.Count) {
                Write-Host 'Choose a listed drive number or enter a folder path.'
                continue
            }
            $answer = $drives[$number - 1].Name
        }
        if ($answer -match '^[A-Za-z]:$') { $answer += '\' }
        if (Test-Path -LiteralPath $answer -PathType Container) {
            return (Resolve-Path -LiteralPath $answer).ProviderPath
        }
        Write-Host 'That folder is unavailable. Choose an existing SD root or staging folder.'
    }
}

function Get-MenuInstalledApps($Applications, [string]$Folder) {
    foreach ($app in $Applications) {
        $present = @($app.files | Where-Object {
            $_.destination -match '\.(nds|srl)$' -and
            (Test-Path -LiteralPath (Get-Destination $Folder $_.destination) -PathType Leaf)
        }).Count -gt 0
        if ($present) { $app }
    }
}

function Show-MenuApps($Applications, [string]$Folder) {
    $installed = @()
    if ($Folder) { $installed = @(Get-MenuInstalledApps $Applications $Folder | ForEach-Object { $_.id }) }
    for ($i = 0; $i -lt $Applications.Count; $i++) {
        $app = $Applications[$i]
        $labels = @()
        if ($app.id -in $installed) { $labels += 'installed' }
        if ($app.noncommercial) { $labels += 'noncommercial' }
        if ($app.PSObject.Properties.Name -contains 'experimental' -and $app.experimental) { $labels += 'experimental' }
        $suffix = if ($labels.Count) { ' [' + ($labels -join ', ') + ']' } else { '' }
        Write-Host ('  {0,2}. {1}  {2}{3}' -f ($i + 1), $app.id, $app.version, $suffix)
    }
}

function Read-MenuApps($Applications, [string]$Folder) {
    Show-MenuApps $Applications $Folder
    Write-Host "Enter numbers or IDs. 'all' selects standard apps, excluding noncommercial/experimental entries."
    while ($true) {
        $answer = Read-MenuInput 'Applications (blank or 0 to go back)'
        if (-not $answer -or $answer -eq '0') { return }
        if ($answer -eq 'all') {
            $Applications | Where-Object {
                -not $_.noncommercial -and
                -not ($_.PSObject.Properties.Name -contains 'experimental' -and $_.experimental)
            } | ForEach-Object { $_.id }
            return
        }
        $names = @()
        $valid = $true
        foreach ($token in ($answer -split '[,\s]+')) {
            $number = 0
            $id = $token
            if ([int]::TryParse($token, [ref]$number) -and $number -ge 1 -and $number -le $Applications.Count) {
                $id = $Applications[$number - 1].id
            }
            if ($id -cnotin @($Applications | ForEach-Object { $_.id })) { $valid = $false; break }
            if ($id -notin $names) { $names += $id }
        }
        if ($valid) { return $names }
        Write-Host 'Choose listed numbers or application IDs, separated by spaces or commas.'
    }
}

function Invoke-InstallerMenu($Document) {
    $folder = $Target
    if ($folder) { $folder = (Resolve-Path -LiteralPath $folder).ProviderPath }
    $scriptPath = $PSCommandPath
    while ($true) {
        Write-Host "`nGloam Add-on Installer`n======================"
        Write-Host ('Target: ' + $(if ($folder) { $folder } else { 'not selected' }))
        Write-Host "1. Install applications`n2. Check installed apps for updates`n3. Update installed apps"
        Write-Host "4. Choose SD card or staging folder`n5. Show applications`n6. Check BIOS and support requirements`n0. Exit"
        $action = Read-MenuInput 'Choose'
        if ($action -eq '0') { return }
        if ($action -eq '4') {
            $chosen = Read-MenuTarget
            if ($chosen) { $folder = $chosen }
            continue
        }
        if ($action -eq '5') {
            $apps = if ($Document.PSObject.Properties.Name -contains 'updates') { @($Document.updates) } else { @($Document.applications) }
            Show-MenuApps @($apps | Sort-Object id) $folder
            $null = Read-MenuInput 'Press Enter to return to the menu...'
            continue
        }
        if ($action -notin @('1', '2', '3', '6')) { Write-Host 'Choose a menu number.'; continue }
        if (-not $folder) {
            $folder = Read-MenuTarget
            if (-not $folder) { continue }
        }
        try {
            Write-Host 'Reading reviewed application versions...'
            $allApps = @(Get-UpdateApps $Document | Sort-Object id)
            $apps = @($allApps)
            if ($action -in @('2', '3', '6')) { $apps = @(Get-MenuInstalledApps $apps $folder) }
            if ($action -eq '6') { Test-Support $allApps $folder @() | Out-Host }
            elseif (-not $apps.Count) { Write-Host 'No matching applications are available.' }
            else {
                $names = @(if ($action -eq '2') { $apps | ForEach-Object { $_.id } } else { Read-MenuApps $apps $folder })
                if ($names.Count) {
                    $allowed = [bool]$Noncommercial
                    if ($action -ne '2') {
                        $selected = @(Get-SelectedApps $allApps $names $true)
                        $names = @($selected | ForEach-Object { $_.id })
                        Write-Host "`nTarget: $folder"
                        foreach ($app in $selected) {
                            Write-Host "  $($app.id) $($app.version): $($app.terms)"
                            if ($app.PSObject.Properties.Name -contains 'experimental' -and $app.experimental) { Write-Host "  Experimental: $($app.notes)" }
                        }
                        if (@($selected | Where-Object { $_.noncommercial }).Count -and -not $allowed) {
                            $allowed = (Read-MenuInput 'Use selected noncommercial apps within their stated terms? [y/N]') -in @('y', 'yes')
                            if (-not $allowed) { Write-Host 'Cancelled.'; $null = Read-MenuInput 'Press Enter to return to the menu...'; continue }
                        }
                        $verb = if ($action -eq '3') { 'Update' } else { 'Install' }
                        if ((Read-MenuInput "$verb these applications? [y/N]") -notin @('y', 'yes')) {
                            Write-Host 'Cancelled.'; $null = Read-MenuInput 'Press Enter to return to the menu...'; continue
                        }
                    } else { $allowed = $true }
                    # Named invocation reuses the unchanged CLI checks and staging behavior.
                    $snapshotPath = [IO.Path]::GetTempFileName()
                    try {
                        # Freeze the catalog shown above, including apps already bundled in the ZIP.
                        # Updates must not refresh to a different version after confirmation.
                        $snapshot = @{ format = 1; applications = @($allApps); updates = @($allApps); manual = @() }
                        [IO.File]::WriteAllText($snapshotPath, ($snapshot | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($true))
                        $operation = @{ Manifest = $snapshotPath; Catalog = $snapshotPath; Target = $folder;
                            Cache = $Cache; Only = [string[]]$names; Offline = [bool]$Offline;
                            Noncommercial = $allowed; DryRun = [bool]$DryRun; Overwrite = [bool]$Overwrite }
                        if ($action -eq '2') { $operation.CheckUpdates = $true; $operation.Upstream = [bool]$Upstream }
                        if ($action -eq '3') { $operation.Update = $true }
                        $global:LASTEXITCODE = 0
                        & $scriptPath @operation
                        if ($LASTEXITCODE -eq 2) { Write-Host 'Completed with failures. See the retry list above.' }
                        elseif ($LASTEXITCODE -ne 0) { Write-Host 'Operation stopped. Review the message above.' }
                    } finally { Remove-Item -LiteralPath $snapshotPath -Force -ErrorAction SilentlyContinue }
                }
            }
        } catch [OperationCanceledException] { return }
        catch { Write-Host "Operation stopped: $($_.Exception.Message)" }
        $null = Read-MenuInput 'Press Enter to return to the menu...'
    }
}


try {
    $document = Get-Content -Raw -LiteralPath $Manifest | ConvertFrom-Json
    if ($document.format -ne 1) { throw 'Unsupported download manifest' }
    if ($MenuMode) { Invoke-InstallerMenu $document; exit 0 }
    if ($List) {
        foreach ($app in $document.applications) {
            $suffix = if ($app.noncommercial) { '; needs -Noncommercial' } else { '' }
            if ($app.PSObject.Properties.Name -contains 'experimental' -and $app.experimental) { $suffix += "; experimental, select with -Only $($app.id)" }
            Write-Output "$($app.id): $($app.version)$suffix; $($app.terms)"
        }
        foreach ($app in $document.manual) { Write-Output "$($app.id): manual setup; $($app.reason); $($app.url)" }
        exit 0
    }
    if (-not $Target -or -not (Test-Path -LiteralPath $Target -PathType Container)) { throw '-Target must name an existing SD root or staging directory' }
    $root = (Resolve-Path -LiteralPath $Target).ProviderPath
    if (([int][bool]$CheckUpdates + [int][bool]$CheckSupport + [int][bool]$Update) -gt 1) { throw 'Choose one of -CheckUpdates, -CheckSupport or -Update' }
    if ($Upstream -and (-not $CheckUpdates -or $Offline)) { throw '-Upstream requires -CheckUpdates and online access' }
    $applications = @(Get-UpdateApps $document)
    if (-not ($CheckUpdates -or $CheckSupport -or $Update -or $Only)) {
        # Refresh versions without expanding the packaged default selection.
        $installIds = @($document.applications | ForEach-Object { $_.id })
        $applications = @($applications | Where-Object { $_.id -in $installIds })
    }
    if ($CheckSupport) { Test-Support $applications $root $Only; exit 0 }
    $selected = @(Get-SelectedApps $applications $Only)
    if ($CheckUpdates) {
        foreach ($app in $selected) {
            $executables = @($app.files | Where-Object { $_.destination -match '\.(nds|srl)$' })
            $present = $false
            $current = $true
            foreach ($item in $executables) {
                $path = Get-Destination $root $item.destination
                if (Test-Path -LiteralPath $path -PathType Leaf) {
                    $present = $true
                    if ((Get-Digest ([IO.File]::ReadAllBytes($path))) -ne $item.sha256) { $current = $false }
                } else { $current = $false }
            }
            $status = if (-not $present) { 'not installed' } elseif ($current) { 'up to date' } else { 'differs from reviewed executable' }
            $missingSupport = @($app.files | Where-Object {
                $_.destination -notmatch '\.(nds|srl)$' -and
                -not (Test-Path -LiteralPath (Get-Destination $root $_.destination) -PathType Leaf)
            }).Count -gt 0
            if ($status -eq 'up to date' -and $missingSupport) { $status = 'missing support files' }
            Write-Output "$($app.id): $status; reviewed $($app.version)"
            if ($Upstream -and $app.PSObject.Properties.Name -contains 'release_tag') {
                $repository = $app.repository.Replace('https://github.com/', '') -replace '\.git$', ''
                if ($repository -cnotmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { throw 'Invalid upstream repository' }
                try {
                    $release = Get-RemoteJson "https://api.github.com/repos/$repository/releases/latest"
                    if ($release.tag_name -ne $app.release_tag) { Write-Output "  Other upstream release: $($release.tag_name) (not selected; review required)." }
                } catch { Write-Output "  Upstream check unavailable: $($_.Exception.Message)" }
            }
        }
        exit 0
    }
    if ($Update -and -not $Only) {
        $installed = @($selected | Where-Object {
            $app = $_
            @($app.files | Where-Object { $_.destination -match '\.(nds|srl)$' -and
                (Test-Path -LiteralPath (Get-Destination $root $_.destination) -PathType Leaf) }).Count -gt 0
        } | ForEach-Object { $_.id })
        $selected = if ($installed.Count) { @(Get-SelectedApps $applications $installed) } else { @() }
    }
    $planned = [Collections.Generic.List[object]]::new()
    $seen = @{}
    foreach ($app in $selected) {
        if ($app.PSObject.Properties.Name -contains 'experimental' -and $app.experimental) { Write-Host "Experimental $($app.id): $($app.notes)" }
        $name = $app.id
        $files = @($app.files)
        if ($app.PSObject.Properties.Name -contains 'game_configs') {
            foreach ($property in $app.game_configs.files.PSObject.Properties) {
                $files += [pscustomobject]@{ download = $app.game_configs.download; format = 'zip'; member = $property.Name; destination = $property.Name; sha256 = $property.Value }
            }
        }
        foreach ($item in $files) {
            $path = Get-Destination $root $item.destination
            if ($seen.ContainsKey($item.destination)) { throw 'Applications have overlapping destinations' }
            $seen[$item.destination] = $true
            $planned.Add([pscustomobject]@{ Id = $name; Item = $item; Path = $path })
        }
    }
    $fetched = @{}
    $installed = 0
    $skipped = 0
    $failed = @{}; $changed = @{}
    foreach ($plan in $planned) {
        if ($failed.ContainsKey($plan.Id)) { continue }
        try {
            $item = $plan.Item
            $replace = $Overwrite -or ($Update -and $item.destination -match '\.(nds|srl)$')
            if ($Update -and (Test-Path -LiteralPath $plan.Path -PathType Leaf) -and
                (Get-Digest ([IO.File]::ReadAllBytes($plan.Path))) -eq $item.sha256) { $skipped++; continue }
            if ((Test-Path -LiteralPath $plan.Path) -and -not $replace) {
                Write-Output "Keep existing: $($item.destination)"
                $skipped++
                continue
            }
            if ($DryRun) { Write-Output "Would install $($plan.Id): $($item.destination)"; continue }
            if ($item.format -eq 'inline') {
                [byte[]]$data = [Text.Encoding]::UTF8.GetBytes([string]$item.content)
                if ($item.destination -cnotin $AddonProfiles -or $data.Length -gt 4096) {
                    throw 'Unsupported inline add-on default'
                }
            } else {
                $key = "$($item.download.asset):$($item.download.sha256)"
                if (-not $fetched.ContainsKey($key)) { $fetched[$key] = Get-Download $item.download }
                [byte[]]$data = $fetched[$key]
            }
            if ($item.format -eq 'zip') {
                $memory = [IO.MemoryStream]::new($data, $false)
                $archive = [IO.Compression.ZipArchive]::new($memory, [IO.Compression.ZipArchiveMode]::Read)
                try {
                    $entries = @($archive.Entries | Where-Object { $_.FullName -ceq $item.member })
                    if ($entries.Count -ne 1 -or $entries[0].Length -gt $MaxDownload) { throw 'Missing, duplicate or oversized archive member' }
                    $stream = $entries[0].Open()
                    $output = [IO.MemoryStream]::new()
                    try { $stream.CopyTo($output); $data = $output.ToArray() }
                    finally { $output.Dispose(); $stream.Dispose() }
                } finally { $archive.Dispose(); $memory.Dispose() }
            } elseif ($item.format -eq 'tar.gz') { $data = Read-TarMember $data $item.member }
            elseif ($item.format -notin @('raw', 'inline')) { throw 'Unsupported download format' }
            if ((Get-Digest $data) -ne $item.sha256) { throw "Member hash mismatch: $($item.destination)" }
            if ($item.destination.EndsWith('.json')) {
                $config = [Text.Encoding]::UTF8.GetString($data) | ConvertFrom-Json
                if ($config -isnot [pscustomobject]) { throw 'Game config must be a JSON object' }
            }
            $path = Get-Destination $root $item.destination
            if ($Update -and (Test-Path -LiteralPath $path -PathType Leaf)) {
                $previous = [IO.File]::ReadAllBytes($path)
                $backup = Join-Path (Join-Path (Join-Path $Cache 'previous-files') (Get-Digest $previous)) ([IO.Path]::GetFileName($path))
                [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup)) | Out-Null
                if (-not [IO.File]::Exists($backup) -or (Get-Digest ([IO.File]::ReadAllBytes($backup))) -ne (Get-Digest $previous)) {
                    Write-CompleteFile $backup $previous
                }
            }
            $folder = [IO.Path]::GetDirectoryName($path)
            [IO.Directory]::CreateDirectory($folder) | Out-Null
            $temporary = Join-Path $folder ([IO.Path]::GetRandomFileName())
            try {
                [IO.File]::WriteAllBytes($temporary, $data)
                if (Test-Path -LiteralPath $path) {
                    if (-not $replace) { $skipped++; continue }
                    [IO.File]::Replace($temporary, $path, [NullString]::Value)
                } else { [IO.File]::Move($temporary, $path) }
                $installed++; $changed[$plan.Id] = $true
            } finally { if (Test-Path -LiteralPath $temporary) { [IO.File]::Delete($temporary) } }
        } catch [OperationCanceledException] { throw }
        catch {
            $failed[$plan.Id] = "$($plan.Item.destination): $($_.Exception.Message)"
            Write-Output "FAILED $($plan.Id): $($failed[$plan.Id]); continuing with other apps."
        }
    }
    # Paired launcher/core files may remain staged, but are not reported complete
    # when a required app failed. Preserve them for the next retry.
    do {
        $added = $false
        foreach ($app in $selected) {
            if ($failed.ContainsKey($app.id) -or $app.PSObject.Properties.Name -notcontains 'requires') { continue }
            foreach ($required in $app.requires) {
                if ($failed.ContainsKey($required)) { $failed[$app.id] = "Required app failed: $required"; $added = $true; break }
            }
        }
    } while ($added)
    foreach ($app in $selected) {
        $status = if ($failed.ContainsKey($app.id)) { 'FAILED: ' + $failed[$app.id] }
            elseif ($DryRun) { 'previewed' } elseif ($changed.ContainsKey($app.id)) { 'installed/updated' } else { 'preserved/current' }
        Write-Output "$($app.id): $status"
    }
    Write-Output "Installed $installed files; preserved $skipped existing files."
    if ($failed.Count) {
        Write-Output ('Completed with failures; retry these apps: ' + (($failed.Keys | Sort-Object) -join ', '))
        exit 2
    }
} catch [OperationCanceledException] { exit 0 }
catch {
    [Console]::Error.WriteLine("Installation stopped: $($_.Exception.Message)")
    if ($MenuMode -and -not $Interactive) { try { $null = Read-MenuInput 'Press Enter to close...' } catch {} }
    exit 1
}
