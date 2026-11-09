# Downloads optional Gloam applications directly from upstream. No Python required.
[CmdletBinding()]
param(
    [string]$Target,
    [string]$Manifest = (Join-Path $PSScriptRoot 'downloads.json'),
    [string]$Cache = (Join-Path ([Environment]::GetFolderPath('UserProfile')) '.gloam-addons-cache'),
    [string[]]$Only,
    [switch]$List,
    [switch]$Noncommercial,
    [switch]$Overwrite,
    [switch]$Offline,
    [switch]$DryRun,
    [switch]$CheckUpdates,
    [switch]$Update,
    [string]$Catalog,
    [switch]$Upstream
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
$MaxDownload = 64MB
$ReaderFonts = @('LiberationSerif-Regular.ttf', 'LiberationSerif-Bold.ttf',
    'LiberationSerif-Italic.ttf', 'LiberationSerif-BoldItalic.ttf', 'LiberationSans-Regular.ttf') |
    ForEach-Object { 'data/dslibris/font/' + $_ }

$ReaderProfiles = @('_nds/dsi-frontend/plugins/dslibris-books.ini',
    '_nds/dsi-frontend/plugins/dslibris-documents.ini')

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

function Get-SelectedApps($Applications, [string[]]$Names) {
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
        if ($app.noncommercial -and -not $Noncommercial) {
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

function Get-Destination([string]$Root, [string]$Name) {
    if ($Name -match '(^/|\\|:)' -or @($Name.Split('/') | Where-Object { $_ -in @('', '.', '..') }).Count) {
        throw "Unsafe destination: $Name"
    }
    if ($Name -cnotmatch '^_nds/dsi-frontend/plugins/apps/.+\.(nds|srl|ini)$' -and
        $Name -cnotmatch '^_gba/configs/[A-Z0-9]{6}\.json$' -and
        $Name -cnotin $ReaderFonts -and $Name -cnotin $ReaderProfiles) { throw "Destination outside add-on folders: $Name" }
    $path = $Root
    foreach ($part in $Name.Split('/')) {
        $path = Join-Path $path $part
        if (Test-Path -LiteralPath $path) {
            if ((Get-Item -Force -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Destination contains a link: $path"
            }
        }
    }
    return $path
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
    } else {
        if ($Offline) { throw "Offline cache missing: $($Item.asset)" }
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
        [IO.File]::WriteAllBytes($path, $bytes)
    }
    if ($bytes.Length -gt $MaxDownload -or (Get-Digest $bytes) -ne $Item.sha256) { throw "Download size/hash mismatch: $($Item.asset)" }
    return ,$bytes
}

try {
    $document = Get-Content -Raw -LiteralPath $Manifest | ConvertFrom-Json
    if ($document.format -ne 1) { throw 'Unsupported download manifest' }
    if ($List) {
        foreach ($app in $document.applications) {
            $suffix = if ($app.noncommercial) { '; needs -Noncommercial' } else { '' }
            Write-Output "$($app.id): $($app.version)$suffix; $($app.terms)"
        }
        foreach ($app in $document.manual) { Write-Output "$($app.id): manual setup; $($app.reason); $($app.url)" }
        exit 0
    }
    if (-not $Target -or -not (Test-Path -LiteralPath $Target -PathType Container)) { throw '-Target must name an existing SD root or staging directory' }
    $root = (Resolve-Path -LiteralPath $Target).ProviderPath
    if ($CheckUpdates -and $Update) { throw 'Choose -CheckUpdates or -Update, not both' }
    if ($Upstream -and (-not $CheckUpdates -or $Offline)) { throw '-Upstream requires -CheckUpdates and online access' }
    $applications = @($document.applications)
    if ($CheckUpdates -or $Update) { $applications = @(Get-UpdateApps $document) }
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
    foreach ($plan in $planned) {
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
            if ($item.destination -cnotin $ReaderProfiles -or $data.Length -gt 4096) {
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
            if (-not (Test-Path -LiteralPath $backup)) { [IO.File]::WriteAllBytes($backup, $previous) }
        }
        $folder = [IO.Path]::GetDirectoryName($path)
        [IO.Directory]::CreateDirectory($folder) | Out-Null
        $temporary = Join-Path $folder ([IO.Path]::GetRandomFileName())
        try {
            [IO.File]::WriteAllBytes($temporary, $data)
            if (Test-Path -LiteralPath $path) {
                if (-not $replace) { $skipped++; continue }
                [IO.File]::Delete($path)
            }
            [IO.File]::Move($temporary, $path)
            $installed++
        } finally { if (Test-Path -LiteralPath $temporary) { [IO.File]::Delete($temporary) } }
    }
    Write-Output "Installed $installed files; preserved $skipped existing files."
} catch {
    [Console]::Error.WriteLine("Installation stopped: $($_.Exception.Message)")
    exit 1
}
