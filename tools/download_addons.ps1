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
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
$MaxDownload = 64MB

function Get-Digest([byte[]]$Data) {
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash($Data))).Replace('-', '').ToLowerInvariant() }
    finally { $hash.Dispose() }
}

function Get-Destination([string]$Root, [string]$Name) {
    if ($Name -match '(^/|\\|:)' -or @($Name.Split('/') | Where-Object { $_ -in @('', '.', '..') }).Count) {
        throw "Unsafe destination: $Name"
    }
    if ($Name -cnotmatch '^_nds/dsi-frontend/plugins/apps/.+\.(nds|srl|ini)$' -and
        $Name -cnotmatch '^_gba/configs/[A-Z0-9]{6}\.json$') { throw "Destination outside add-on folders: $Name" }
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
    $path = Join-Path $Cache $Item.asset
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
    $catalog = Get-Content -Raw -LiteralPath $Manifest | ConvertFrom-Json
    if ($catalog.format -ne 1) { throw 'Unsupported download manifest' }
    if ($List) {
        foreach ($app in $catalog.applications) {
            $suffix = if ($app.noncommercial) { '; needs -Noncommercial' } else { '' }
            Write-Output "$($app.id): $($app.version)$suffix; $($app.terms)"
        }
        foreach ($app in $catalog.manual) { Write-Output "$($app.id): manual setup; $($app.reason); $($app.url)" }
        exit 0
    }
    if (-not $Target -or -not (Test-Path -LiteralPath $Target -PathType Container)) { throw '-Target must name an existing SD root or staging directory' }
    $root = (Resolve-Path -LiteralPath $Target).ProviderPath
    $apps = @{}
    foreach ($app in $catalog.applications) {
        if ($apps.ContainsKey($app.id)) { throw 'Duplicate application ID' }
        $apps[$app.id] = $app
    }
    $wanted = if ($Only) { @($Only | Select-Object -Unique) } else { @($catalog.applications | ForEach-Object { $_.id }) }
    $planned = [Collections.Generic.List[object]]::new()
    $seen = @{}
    foreach ($name in $wanted) {
        if (-not $apps.ContainsKey($name)) { throw "No pinned download for: $name; use -List for manual entries" }
        $app = $apps[$name]
        if ($app.noncommercial -and -not $Noncommercial) {
            if ($Only) { throw "$name requires -Noncommercial" }
            continue
        }
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
        if ((Test-Path -LiteralPath $plan.Path) -and -not $Overwrite) {
            Write-Output "Keep existing: $($item.destination)"
            $skipped++
            continue
        }
        if ($DryRun) { Write-Output "Would install $($plan.Id): $($item.destination)"; continue }
        $key = "$($item.download.asset):$($item.download.sha256)"
        if (-not $fetched.ContainsKey($key)) { $fetched[$key] = Get-Download $item.download }
        [byte[]]$data = $fetched[$key]
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
        } elseif ($item.format -ne 'raw') { throw 'Unsupported download format' }
        if ((Get-Digest $data) -ne $item.sha256) { throw "Member hash mismatch: $($item.destination)" }
        if ($item.destination.EndsWith('.json')) {
            $config = [Text.Encoding]::UTF8.GetString($data) | ConvertFrom-Json
            if ($config -isnot [pscustomobject]) { throw 'Game config must be a JSON object' }
        }
        $path = Get-Destination $root $item.destination
        $folder = [IO.Path]::GetDirectoryName($path)
        [IO.Directory]::CreateDirectory($folder) | Out-Null
        $temporary = Join-Path $folder ([IO.Path]::GetRandomFileName())
        try {
            [IO.File]::WriteAllBytes($temporary, $data)
            if (Test-Path -LiteralPath $path) {
                if (-not $Overwrite) { $skipped++; continue }
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
