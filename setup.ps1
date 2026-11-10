# SPDX-License-Identifier: GPL-3.0-or-later
# Run from Windows PowerShell: irm https://raw.githubusercontent.com/nextcode4u/gloam-shell-ndx/main/setup.ps1 | iex
[CmdletBinding()]
param([string]$ReleaseDirectory, [switch]$NoLaunch)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (-not $ReleaseDirectory) {
    $releases = Invoke-RestMethod 'https://api.github.com/repos/nextcode4u/gloam-shell-ndx/releases?per_page=100' -UserAgent 'Gloam-Setup' -TimeoutSec 30
    $matchesRelease = @($releases | Where-Object {
        -not $_.draft -and @($_.assets | Where-Object name -eq 'addons.zip').Count -eq 1
    } | Sort-Object published_at -Descending)
    if (-not $matchesRelease.Count) { throw 'No published setup release was found.' }
    $release = $matchesRelease[0]
    if ($release.tag_name -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+(-(preview|rc)\.[0-9]+)?$') { throw 'Unexpected release version.' }
    $ReleaseDirectory = Join-Path $env:LOCALAPPDATA ('Gloam/Setup/releases/' + $release.tag_name)
    [IO.Directory]::CreateDirectory($ReleaseDirectory) | Out-Null
    [IO.File]::WriteAllText((Join-Path $ReleaseDirectory 'release.json'), ($release | ConvertTo-Json -Depth 30))
    foreach ($name in @('SHA256SUMS','addons.zip')) {
        $asset = @($release.assets | Where-Object name -eq $name)
        if ($asset.Count -ne 1 -or $asset[0].size -gt 64MB) { throw "Missing or oversized asset: $name" }
        $url = [string]$asset[0].browser_download_url
        if (-not $url.StartsWith('https://github.com/nextcode4u/gloam-shell-ndx/releases/download/')) { throw 'Unexpected release origin.' }
        $file = Join-Path $ReleaseDirectory $name
        if ($name -eq 'addons.zip' -and (Test-Path -LiteralPath $file)) { continue }
        Write-Host "Downloading $name..."
        $temporary = $file + '.download'
        try {
            Invoke-WebRequest $url -UseBasicParsing -OutFile $temporary -UserAgent 'Gloam-Setup' -TimeoutSec 120
            Move-Item -LiteralPath $temporary -Destination $file -Force
        } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
    }
}
function Get-GloamBootstrapHash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $hash.Dispose(); $stream.Dispose() }
}
$package = Join-Path $ReleaseDirectory 'addons.zip'
$lines = @([IO.File]::ReadAllLines((Join-Path $ReleaseDirectory 'SHA256SUMS')) | Where-Object { $_ -match '^[a-f0-9]{64}  addons\.zip$' })
if ($lines.Count -ne 1 -or (Get-GloamBootstrapHash $package) -ne $lines[0].Substring(0,64)) { throw 'Add-on package checksum mismatch. Remove the cached addons.zip and try again.' }
$allowed = @('gloam-setup.ps1','gloam-setup.xaml','gloam-setup-core.ps1','gloam-setup-worker.ps1','Start Gloam Setup.cmd',
    'download-addons.ps1','download-addons.py','downloads.json','Start Add-on Installer.cmd','README.txt')
$required = @('gloam-setup.ps1','gloam-setup.xaml','gloam-setup-core.ps1','gloam-setup-worker.ps1','download-addons.ps1','downloads.json')
$archive = [IO.Compression.ZipFile]::OpenRead($package)
try {
    $selected = @{}
    foreach ($entry in $archive.Entries) {
        if (-not $entry.FullName.StartsWith('addon-installer/')) { continue }
        $name = $entry.FullName.Substring(16)
        if ($name -notin $allowed) { continue }
        if ($selected.ContainsKey($name) -or $entry.Length -gt 2MB -or ($entry.ExternalAttributes -shr 28 -band 15) -eq 10) { throw 'Invalid setup script entry.' }
        $selected[$name] = $entry
    }
    foreach ($name in $required) { if (-not $selected.ContainsKey($name)) { throw 'This release predates the graphical setup tool. Use a newer Gloam release.' } }
    $folder = Join-Path $ReleaseDirectory 'addon-installer'
    [IO.Directory]::CreateDirectory($folder) | Out-Null
    foreach ($name in $selected.Keys) {
        $stream = $selected[$name].Open(); $file = [IO.File]::Create((Join-Path $folder $name))
        try { $stream.CopyTo($file) } finally { $stream.Dispose(); $file.Dispose() }
    }
} finally { $archive.Dispose() }
$gui = Join-Path $folder 'gloam-setup.ps1'
Write-Host "Verified Gloam Setup: $gui"
if (-not $NoLaunch) {
    $powershell = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    Start-Process $powershell -ArgumentList ('-NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "' + $gui + '" -ReleaseDirectory "' + $ReleaseDirectory + '"')
}
