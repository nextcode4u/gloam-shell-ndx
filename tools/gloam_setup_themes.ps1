# SPDX-License-Identifier: GPL-3.0-or-later
# Catalog and runtime downloads share one immutable revision; browsing writes only to the PC cache.
function Get-GloamThemeBlobHash([byte[]]$Bytes) {
    $hash = [Security.Cryptography.SHA1]::Create()
    try {
        $prefix = [Text.Encoding]::UTF8.GetBytes("blob $($Bytes.Length)`0")
        $null = $hash.TransformBlock($prefix, 0, $prefix.Length, $prefix, 0)
        $null = $hash.TransformFinalBlock($Bytes, 0, $Bytes.Length)
        return ([BitConverter]::ToString($hash.Hash)).Replace('-', '').ToLowerInvariant()
    } finally { $hash.Dispose() }
}
function Assert-GloamThemeRelative([string]$Path) {
    if (-not $Path -or $Path.Length -gt 240 -or $Path -match '(^/|\\|:|[\x00-\x1f]|(^|/)\.|//|[<>"|?*])' -or
        @($Path.Split('/') | Where-Object { $_ -match '[. ]$|^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)' }).Count) {
        throw 'Unsafe theme catalog path.'
    }
}
function Assert-GloamThemeNotCancelled {
    $cancel = Get-Variable GloamCancelFile -Scope Script -ErrorAction SilentlyContinue
    if ($cancel -and $cancel.Value -and [IO.File]::Exists($cancel.Value)) { throw [OperationCanceledException]::new('Theme operation cancelled; installed themes were kept.') }
}
function Get-GloamThemeNetwork([string]$Url, [long]$Limit) {
    $uri = [Uri]$Url
    if ($uri.Scheme -ne 'https' -or $uri.UserInfo -or
        ($uri.Host -ne 'api.github.com' -and $uri.Host -ne 'raw.githubusercontent.com')) { throw 'Unexpected theme download origin.' }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $request = [Net.HttpWebRequest]::Create($uri)
    $request.UserAgent = 'Gloam-SD-Setup theme catalog'; $request.AllowAutoRedirect = $false
    $request.Timeout = 20000; $request.ReadWriteTimeout = 20000
    $response = $request.GetResponse()
    try {
        if ([int]$response.StatusCode -ne 200 -or $response.ContentLength -gt $Limit) { throw 'Theme download status or size rejected.' }
        $inputStream = $response.GetResponseStream(); $output = [IO.MemoryStream]::new()
        try {
            $buffer = [byte[]]::new(32768); $watch = [Diagnostics.Stopwatch]::StartNew()
            while (($count = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                Assert-GloamThemeNotCancelled
                if ($output.Length + $count -gt $Limit -or $watch.Elapsed.TotalSeconds -gt 60) { throw 'Theme download exceeded its size/time limit.' }
                $output.Write($buffer, 0, $count)
            }
            return ,$output.ToArray()
        } finally { $inputStream.Dispose(); $output.Dispose() }
    } finally { $response.Close() }
}
function Get-GloamThemeRawUrl([string]$Revision, [string]$Path) {
    if ($Revision -cnotmatch '^[a-f0-9]{40}$') { throw 'Invalid theme source revision.' }
    Assert-GloamThemeRelative $Path
    $encoded = ($Path.Split('/') | ForEach-Object { [Uri]::EscapeDataString($_) }) -join '/'
    return "https://raw.githubusercontent.com/nextcode4u/gloam-shell-theme-directory/$Revision/$encoded"
}
function Assert-GloamThemeCatalog($Catalog) {
    if ($Catalog.Format -ne 1 -or $Catalog.Revision -cnotmatch '^[a-f0-9]{40}$' -or
        @($Catalog.Themes).Count -gt 256 -or -not @($Catalog.Themes).Count) { throw 'Invalid theme catalog.' }
    $ids = @{}
    foreach ($theme in $Catalog.Themes) {
        if ($theme.Id -cnotmatch '^[a-zA-Z0-9_-]{1,63}$' -or $ids.ContainsKey($theme.Id) -or
            [string]::IsNullOrWhiteSpace($theme.Name) -or $theme.Name.Length -gt 128 -or
            $theme.Author.Length -gt 1024 -or $theme.Description.Length -gt 4096 -or $theme.License.Length -gt 4096 -or $theme.Requirements.Length -gt 4096) { throw 'Invalid or duplicate theme catalog entry.' }
        $ids[$theme.Id] = $true; $paths = @{}; $total = 0L
        if (-not @($theme.Files).Count -or @($theme.Files).Count -gt 1024) { throw 'Theme catalog file count exceeds its limit.' }
        foreach ($file in @($theme.Files) + @($theme.Preview)) {
            if ($null -eq $file) { continue }
            Assert-GloamThemeRelative $file.Path
            if ($file.Sha -cnotmatch '^[a-f0-9]{40}$' -or $file.Bytes -le 0 -or $file.Bytes -gt 16MB) { throw 'Invalid theme file identity/size.' }
            if ($file -eq $theme.Preview) {
                if ($file.Path -cne "themes/$($theme.Id)/preview.png" -or $file.Bytes -gt 2MB) { throw 'Invalid theme preview.' }
                continue
            }
            Assert-GloamThemeRelative $file.Relative
            $isNotice = ($file.Path -ceq 'LICENSE' -and $file.Relative -ceq 'licenses/theme-directory-GPL-3.0.txt') -or
                ($file.Path -ceq 'LICENSES.md' -and $file.Relative -ceq 'licenses/theme-directory-credits.md')
            if (-not $isNotice -and $file.Path -cne "themes/$($theme.Id)/$($file.Relative)") { throw 'Theme file leaves its catalog folder.' }
            $extension = [IO.Path]::GetExtension($file.Relative).ToLowerInvariant()
            if ($extension -notin @('.ini','.png','.jpg','.jpeg','.wav','.dsfont','.gtm','.gsp','.gfx','.md','.txt','.json','')) { throw 'Unsupported file in theme package.' }
            if ($paths.ContainsKey($file.Relative)) { throw 'Duplicate/case-colliding theme file.' }
            $paths[$file.Relative] = $true; $total += $file.Bytes
        }
        if (-not $paths.ContainsKey('theme.ini') -or -not $paths.ContainsKey('CREDITS.md') -or -not $paths.ContainsKey('licenses/theme-directory-GPL-3.0.txt') -or -not $paths.ContainsKey('licenses/theme-directory-credits.md') -or $total -gt 64MB) { throw 'Theme is incomplete or exceeds 64 MiB.' }
    }
}
function Get-GloamThemeCache([string]$Cache) {
    [IO.Directory]::CreateDirectory($Cache) | Out-Null
    return Get-GloamRoot $Cache
}
function Read-GloamThemeCatalog([string]$Cache) {
    $folder = Get-GloamThemeCache $Cache; $path = Get-GloamPath $folder 'catalog.json'
    if (-not [IO.File]::Exists($path) -or ([IO.FileInfo]::new($path)).Length -gt 8MB) { throw 'No cached theme catalog. Refresh the directory while online first.' }
    $catalog = [IO.File]::ReadAllText($path) | ConvertFrom-Json
    Assert-GloamThemeCatalog $catalog
    return $catalog
}
function Update-GloamThemeCatalog([string]$Cache) {
    $repo = 'https://api.github.com/repos/nextcode4u/gloam-shell-theme-directory'
    $commit = [Text.Encoding]::UTF8.GetString((Get-GloamThemeNetwork "$repo/commits/main" 2MB)) | ConvertFrom-Json
    $revision = [string]$commit.sha
    if ($revision -cnotmatch '^[a-f0-9]{40}$') { throw 'Unexpected theme revision.' }
    $tree = [Text.Encoding]::UTF8.GetString((Get-GloamThemeNetwork "$repo/git/trees/$revision`?recursive=1" 8MB)) | ConvertFrom-Json
    if ($tree.truncated -or @($tree.tree).Count -gt 20000) { throw 'Theme source tree is incomplete or too large.' }
    $blobs = @{}
    foreach ($entry in $tree.tree) { if ($entry.type -eq 'blob') { if ($blobs.ContainsKey($entry.path)) { throw 'Duplicate source-tree path.' }; $blobs[$entry.path] = $entry } }
    if (-not $blobs.ContainsKey('catalog.json')) { throw 'Theme directory has no catalog.' }
    $bytes = Get-GloamThemeNetwork (Get-GloamThemeRawUrl $revision 'catalog.json') 1MB
    if ((Get-GloamThemeBlobHash $bytes) -cne $blobs['catalog.json'].sha) { throw 'Theme catalog hash mismatch.' }
    $source = [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
    if ($source.format_version -ne 1) { throw 'Unsupported theme directory format.' }
    $themes = @()
    foreach ($item in $source.themes) {
        if ($item.id -cnotmatch '^[a-zA-Z0-9_-]{1,63}$' -or $item.path -cne "themes/$($item.id)") { throw 'Unsafe theme directory entry.' }
        $files = @(); $preview = $null
        foreach ($path in @($blobs.Keys | Where-Object { $_.StartsWith($item.path + '/', [StringComparison]::Ordinal) } | Sort-Object)) {
            $entry = $blobs[$path]; $relative = $path.Substring($item.path.Length + 1)
            if ($entry.mode -notin @('100644','100755')) { throw 'Linked theme entries are not supported.' }
            if ($relative -ceq 'preview.png') { $preview = [pscustomobject]@{ Path=$path; Sha=$entry.sha; Bytes=$entry.size }; continue }
            if ($relative -in @('README.md','settings-preview.png','home-preview.png')) { continue }
            $files += [pscustomobject]@{ Path=$path; Relative=$relative; Sha=$entry.sha; Bytes=$entry.size }
        }
        foreach ($notice in @(@('LICENSE','licenses/theme-directory-GPL-3.0.txt'),@('LICENSES.md','licenses/theme-directory-credits.md'))) {
            if (-not $blobs.ContainsKey($notice[0])) { throw 'Theme directory license notices are missing.' }
            $entry = $blobs[$notice[0]]
            $files += [pscustomobject]@{ Path=$notice[0]; Relative=$notice[1]; Sha=$entry.sha; Bytes=$entry.size }
        }
        $themes += [pscustomobject]@{ Id=[string]$item.id; Name=[string]$item.name; Author=[string]$item.author; Description=[string]$item.description;
            License=([string]$item.configuration_license + '; ' + [string]$item.asset_license); Requirements=[string]$item.requires_gloam; Preview=$preview; Files=$files }
    }
    $catalog = [pscustomobject]@{ Format=1; Revision=$revision; Themes=$themes }
    Assert-GloamThemeCatalog $catalog
    $folder = Get-GloamThemeCache $Cache
    Write-GloamFile $folder 'catalog.json' ([Text.UTF8Encoding]::new($false).GetBytes(($catalog | ConvertTo-Json -Depth 12))) $true | Out-Null
    return $catalog
}
function Get-GloamThemeCatalogEntry($Catalog, [string]$Id) {
    Assert-GloamThemeCatalog $Catalog
    $entry = @($Catalog.Themes | Where-Object Id -CEQ $Id)
    if ($entry.Count -ne 1) { throw 'Theme is not in the cached directory.' }
    return $entry[0]
}
function Get-GloamThemeCachedBlob([string]$Cache, [string]$Revision, $File, [bool]$Offline) {
    $folder = Get-GloamThemeCache $Cache
    $path = Get-GloamPath $folder ('blobs/' + $File.Sha)
    if ([IO.File]::Exists($path) -and ([IO.FileInfo]::new($path)).Length -eq $File.Bytes -and
        (Get-GloamThemeBlobHash ([IO.File]::ReadAllBytes($path))) -ceq $File.Sha) { return $path }
    if ($Offline) { throw 'This theme file is not cached. Connect online to download it first.' }
    Assert-GloamThemeNotCancelled
    $data = Get-GloamThemeNetwork (Get-GloamThemeRawUrl $Revision $File.Path) ([long]$File.Bytes)
    if ($data.Length -ne $File.Bytes -or (Get-GloamThemeBlobHash $data) -cne $File.Sha) { throw ('Theme file hash/size mismatch: ' + $File.Path) }
    Write-GloamFile $folder ('blobs/' + $File.Sha) $data $true | Out-Null
    return $path
}
function Get-GloamCatalogThemePreview([string]$Cache, [string]$Id, [bool]$Offline) {
    $catalog = Read-GloamThemeCatalog $Cache; $theme = Get-GloamThemeCatalogEntry $catalog $Id
    if ($null -eq $theme.Preview) { throw 'This contributor has not provided preview.png. The theme can still be installed.' }
    $path = Get-GloamThemeCachedBlob $Cache $catalog.Revision $theme.Preview $Offline
    $header = [byte[]]::new(24); $stream = [IO.File]::OpenRead($path)
    try { $read = $stream.Read($header, 0, 24) } finally { $stream.Dispose() }
    if ($read -ne 24 -or [BitConverter]::ToString($header, 0, 8) -ne '89-50-4E-47-0D-0A-1A-0A') { throw 'Theme preview is not PNG.' }
    $width = ([long]$header[16] * 16777216) + ([long]$header[17] * 65536) + ([long]$header[18] * 256) + $header[19]
    $height = ([long]$header[20] * 16777216) + ([long]$header[21] * 65536) + ([long]$header[22] * 256) + $header[23]
    if ($width -lt 1 -or $height -lt 1 -or $width -gt 1024 -or $height -gt 1536) { throw 'Theme preview dimensions rejected.' }
    Add-Type -AssemblyName System.Drawing
    $stream = [IO.File]::OpenRead($path)
    try {
        $image = [Drawing.Image]::FromStream($stream)
        try { if ($image.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid -or $image.Width -gt 1024 -or $image.Height -gt 1536) { throw 'Theme preview dimensions/format rejected.' } }
        finally { $image.Dispose() }
    } finally { $stream.Dispose() }
    return $path
}
function Install-GloamCatalogTheme([string]$Root, [string]$Cache, [string]$Id, [string]$InstallId, [bool]$Replace, [bool]$AllowNoncommercial, [bool]$Offline) {
    $catalog = Read-GloamThemeCatalog $Cache; $theme = Get-GloamThemeCatalogEntry $catalog $Id
    if ($InstallId -cnotmatch '^[a-zA-Z0-9_-]{1,63}$') { throw 'Choose a theme folder ID using up to 63 letters, digits, hyphens or underscores.' }
    if ($theme.License -match '(?i)(non.?commercial|BY-NC|NC-SA)' -and -not $AllowNoncommercial) { throw 'Enable acceptance of noncommercial theme terms before installing this theme.' }
    $relative = '_nds/dsi-frontend/themes/' + $InstallId; $destination = Get-GloamPath $Root $relative
    if ([IO.Directory]::Exists($destination) -and -not $Replace) { throw 'That theme already exists. Choose another folder ID, or explicitly enable replacement with backup.' }
    if ([IO.File]::Exists($destination)) { throw 'A file occupies the theme destination.' }
    $parent = Get-GloamPath $Root '_nds/dsi-frontend/themes'
    if (-not [IO.Directory]::Exists($destination) -and [IO.Directory]::Exists($parent) -and @(Get-ChildItem -LiteralPath $parent -Directory | Where-Object { $_.Name -notlike '.*' -and [IO.File]::Exists((Join-Path $_.FullName 'theme.ini')) }).Count -ge 64) { throw 'Gloam supports up to 64 installed themes. Remove one before installing another.' }
    # Complete every network read and identity check before touching the SD.
    $cached = @()
    foreach ($file in $theme.Files) {
        Write-Host ('Preparing: ' + $file.Relative)
        $cached += [pscustomobject]@{ File=$file; Source=(Get-GloamThemeCachedBlob $Cache $catalog.Revision $file $Offline) }
    }
    Assert-GloamThemeNotCancelled
    $stageRelative = '_nds/dsi-frontend/.theme-install-' + [Guid]::NewGuid().ToString('N')
    $stage = Get-GloamPath $Root $stageRelative; $backup = ''
    try {
        [IO.Directory]::CreateDirectory($stage) | Out-Null
        foreach ($item in $cached) {
            Assert-GloamThemeNotCancelled
            $path = Get-GloamPath $Root ($stageRelative + '/' + $item.File.Relative)
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
            [IO.File]::Copy($item.Source, $path)
            if ((Get-GloamThemeBlobHash ([IO.File]::ReadAllBytes($path))) -cne $item.File.Sha) { throw 'Staged theme changed or could not be copied correctly.' }
        }
        $report = Test-GloamTheme $stage
        if (-not $report.Valid) { throw ($report.Errors -join "`n") }
        foreach ($warning in $report.Warnings) { Write-Host $warning }
        Assert-GloamThemeNotCancelled
        $destination = Get-GloamPath $Root $relative
        if ([IO.Directory]::Exists($destination)) {
            if (-not $Replace) { throw 'A theme appeared at the destination; nothing was replaced.' }
            $backup = Get-GloamPath $Root ('_nds/dsi-frontend/setup-backups/themes/' + [Guid]::NewGuid().ToString('N') + '/' + $InstallId)
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup)) | Out-Null
            [IO.Directory]::Move($destination, $backup)
        }
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        try { [IO.Directory]::Move($stage, $destination) }
        catch { if ($backup -and -not [IO.Directory]::Exists($destination)) { [IO.Directory]::Move($backup, $destination) }; throw }
        Write-Host ("Installed $($theme.Name) as $InstallId from revision $($catalog.Revision).")
        if ($backup) { Write-Host ('Previous theme backup: ' + $backup) }
    } finally { if ([IO.Directory]::Exists($stage)) { [IO.Directory]::Delete($stage, $true) } }
}
