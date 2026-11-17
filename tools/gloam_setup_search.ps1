# SPDX-License-Identifier: GPL-3.0-or-later
# Optional PC-built index. Dot-sourcing does not scan or write the selected SD.
function Initialize-GloamLibraryIndexer {
    if (-not ('GloamLibraryIndex' -as [type])) {
        Add-Type -Path (Join-Path $PSScriptRoot 'gloam-library-index.cs')
    }
}
function Get-GloamSearchRoots([string]$Root) {
    $ini = Read-GloamIni (Get-GloamPath $Root '_nds/dsi-frontend/tabs.ini')
    if (-not $ini.Contains('tabs') -or $ini['tabs']['version'] -cne '1') { $ini = [ordered]@{} }
    $folders = @()
    foreach ($pair in @(@('games','roms'),@('apps','apps'),@('media','media'))) {
        $value = if ($ini.Contains('roots') -and $ini['roots'][$pair[0]]) { $ini['roots'][$pair[0]] } else { $pair[1] }
        $folders += ConvertTo-GloamRelative $Root ($value -replace '^/+', '')
    }
    foreach ($section in $ini.Keys) {
        if ($section -notmatch '^tab\.[A-Za-z0-9_-]{1,31}$' -or $section.Substring(4) -in @('home','games','apps','media','system','files')) { continue }
        $value = if ($ini[$section].Contains('path')) { $ini[$section]['path'] } elseif ($ini[$section].Contains('folder')) { $ini[$section]['folder'] } else { '' }
        if ($value) { $folders += ConvertTo-GloamRelative $Root ($value -replace '^/+', '') }
    }
    $kept = [Collections.Generic.List[string]]::new()
    foreach ($folder in @($folders | Sort-Object Length, { $_.ToLowerInvariant() } -Unique)) {
        Get-GloamPath $Root $folder | Out-Null
        $covered = @($kept | Where-Object { $folder.Equals($_,[StringComparison]::OrdinalIgnoreCase) -or $folder.StartsWith($_+'/',[StringComparison]::OrdinalIgnoreCase) }).Count
        if (-not $covered) { $kept.Add($folder) }
    }
    return $kept.ToArray()
}
function Get-GloamSearchIndexStatus([string]$Root) {
    $path = Get-GloamPath $Root '_nds/dsi-frontend/cache/library.gli'
    if (-not [IO.File]::Exists($path)) { return 'No search index. Build one to enable library search on the console.' }
    $stream = $null
    try {
        Initialize-GloamLibraryIndexer
        $stream = [IO.File]::OpenRead($path)
        $header = [byte[]]::new(32)
        if ($stream.Read($header,0,32) -ne 32 -or $stream.Length -gt 64MB) { throw 'Invalid header size.' }
        $format = [Text.Encoding]::ASCII.GetString($header,0,8)
        $count = [BitConverter]::ToUInt32($header,8)
        $payload = [BitConverter]::ToUInt32($header,12)
        $blocks = [BitConverter]::ToUInt32($header,24)
        if ($format -notin @('GLIDX001','GLIDX002') -or
            (32L+$payload+276L*$blocks) -ne $stream.Length -or
            ($format -ceq 'GLIDX001' -and $blocks -ne 0) -or
            ($format -ceq 'GLIDX002' -and $blocks -ne [Math]::Ceiling($count/64.0)) -or
            $count -gt 1000000 -or $count -gt [Math]::Floor($payload/9) -or
            [GloamLibraryIndex]::Hash($header,28,[uint32]2166136261) -ne [BitConverter]::ToUInt32($header,28)) {
            throw 'Invalid search index header.'
        }
        $date = [DateTime]::new(1970,1,1,0,0,0,[DateTimeKind]::Utc).AddSeconds([BitConverter]::ToUInt64($header,16))
        return "$count indexed files, built $($date.ToString('yyyy-MM-dd HH:mm')) UTC. Refresh after adding or organizing files."
    } catch { return 'Search index is unavailable or invalid; rebuild it. Ordinary browsing still works.' } finally { if ($null -ne $stream) { $stream.Dispose() } }
}
function Build-GloamSearchIndex([string]$Root, [string]$WorkDirectory) {
    Assert-GloamConfigurationReady $Root
    $root = Get-GloamRoot $Root
    $libraries = @(Get-GloamSearchRoots $root)
    foreach ($folder in $libraries) {
        if (-not [IO.Directory]::Exists((Get-GloamPath $root $folder))) { Write-Host "Not indexed (folder missing): $folder" }
    }
    Initialize-GloamLibraryIndexer
    [IO.Directory]::CreateDirectory($WorkDirectory) | Out-Null
    $prepared = Join-Path $WorkDirectory ([Guid]::NewGuid().ToString('N')+'.gli')
    $cancel = if (Get-Variable GloamCancelFile -Scope Script -ErrorAction SilentlyContinue) { $script:GloamCancelFile } else { '' }
    $temporary = ''
    try {
        [GloamLibraryIndex]::Build($root,[string[]]$libraries,$prepared,$cancel) | Out-Null
        if ($cancel -and [IO.File]::Exists($cancel)) { throw [OperationCanceledException]::new('Index cancelled; previous index kept.') }
        $current = Get-GloamPath $root '_nds/dsi-frontend/cache/library.gli'
        $backup = Get-GloamPath $root '_nds/dsi-frontend/cache/library.gli.bak'
        $temporary = Get-GloamPath $root ('_nds/dsi-frontend/cache/library-'+[Guid]::NewGuid().ToString('N')+'.tmp')
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($current)) | Out-Null
        [IO.File]::Copy($prepared,$temporary,$false)
        if ($cancel -and [IO.File]::Exists($cancel)) { throw [OperationCanceledException]::new('Index cancelled; previous index kept.') }
        # Only the derived index has one rotating backup; user settings/content are untouched.
        if ([IO.File]::Exists($current)) {
            if ([IO.File]::Exists($backup)) { [IO.File]::Delete($backup) }
            [IO.File]::Move($current,$backup)
        }
        try { [IO.File]::Move($temporary,$current) }
        catch {
            if (-not [IO.File]::Exists($current) -and [IO.File]::Exists($backup)) { [IO.File]::Move($backup,$current) }
            throw
        }
        Write-Host (Get-GloamSearchIndexStatus $root)
    } catch {
        $cause = $_.Exception
        while ($null -ne $cause.InnerException) { $cause = $cause.InnerException }
        if ($cause -is [OperationCanceledException]) { throw [OperationCanceledException]::new($cause.Message) }
        throw
    } finally {
        if ([IO.File]::Exists($prepared)) { [IO.File]::Delete($prepared) }
        if ($temporary -and [IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
    }
}
