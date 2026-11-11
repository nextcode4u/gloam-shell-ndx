# SPDX-License-Identifier: GPL-3.0-or-later
$script:GloamDatRecords = @{}
$script:GloamDatAttempts = @{}
function Initialize-GloamDat {
    if (-not ('GloamDat' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'gloam-dat.cs') -ReferencedAssemblies @('System.dll','System.Core.dll','System.Xml.dll','System.IO.Compression.dll') }
}
function Get-GloamDatFolder([string]$Cache, [string]$System) {
    if (-not $GloamArtworkSystems.Contains($System) -or $System -in @('Automatic by folder','Nintendo DS (GameTDB ID)')) { throw 'Choose a specific system for DAT identification.' }
    $hash=[Security.Cryptography.SHA256]::Create()
    try { $key=([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($System)))).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose() }
    return Join-Path $Cache $key
}
function Import-GloamDat([string]$Cache, [string]$System, [string]$Source) {
    Initialize-GloamDat
    $entries=[GloamDat]::Read($Source) # Validate before adding to the active cache.
    $folder=Get-GloamDatFolder $Cache $System
    [IO.Directory]::CreateDirectory($folder) | Out-Null
    $hash=Get-GloamHash $Source
    $target=Join-Path $folder ($hash+'.dat')
    [IO.File]::Copy($Source,$target,$true)
    [IO.File]::WriteAllText(($target+'.source.txt'),[IO.Path]::GetFileName($Source),[Text.UTF8Encoding]::new($false))
    return $entries.Length
}
function Receive-GloamDat([string]$Cache, [string]$System, [ValidateSet('no-intro','tosec')][string]$Collection) {
    Initialize-GloamDat
    $folder=Get-GloamDatFolder $Cache $System
    [IO.Directory]::CreateDirectory($folder) | Out-Null
    $url='https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/'+$Collection+'/'+[Uri]::EscapeDataString($System)+'.dat'
    $request=[Net.HttpWebRequest]::Create($url)
    $request.UserAgent='Gloam-Setup DAT identification'; $request.Timeout=15000; $request.ReadWriteTimeout=15000; $request.AllowAutoRedirect=$false
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    $response=$request.GetResponse()
    $temporary=Join-Path $folder ([Guid]::NewGuid().ToString('N')+'.tmp')
    try {
        if ([int]$response.StatusCode -ne 200 -or $response.ContentLength -gt 32MB) { throw 'Unexpected or oversized DAT response.' }
        $inputStream=$response.GetResponseStream(); $output=[IO.File]::Create($temporary)
        try {
            $buffer=[byte[]]::new(65536); $watch=[Diagnostics.Stopwatch]::StartNew()
            while (($read=$inputStream.Read($buffer,0,$buffer.Length)) -gt 0) {
                if ($output.Length+$read -gt 32MB -or $watch.Elapsed.TotalSeconds -gt 60) { throw 'DAT download exceeded its size/time limit.' }
                $output.Write($buffer,0,$read)
            }
        } finally { $output.Dispose(); $inputStream.Dispose() }
        $count=([GloamDat]::Read($temporary)).Length
        $target=Join-Path $folder 'libretro.dat'
        if (Test-Path -LiteralPath $target) { [IO.File]::Replace($temporary,$target,[NullString]::Value) } else { [IO.File]::Move($temporary,$target) }
        [IO.File]::WriteAllText(($target+'.source.txt'),$url+"`nMetadata source: libretro-database; see upstream notices. Downloaded on demand, not bundled.",[Text.UTF8Encoding]::new($false))
        return $count
    } finally { $response.Close(); if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
}
function Update-GloamDat([string]$Cache, [string]$System) {
    foreach ($collection in @('no-intro','tosec')) {
        try { return Receive-GloamDat $Cache $System $collection }
        catch {
            $cause=$_.Exception
            while ($cause.InnerException) { $cause=$cause.InnerException }
            if ($cause -is [Net.WebException] -and $cause.Response -and [int]$cause.Response.StatusCode -eq 404) {
                $cause.Response.Close()
                continue
            }
            throw # Keep the old cache on timeout, invalid data or other failures.
        }
    }
    throw 'No Libretro No-Intro or TOSEC DAT is available for this system. Import a matching DAT instead.'
}
function Find-GloamDatNames([string]$Root, [string]$Content, [string]$System, [string]$Cache, [string]$ArchiveMember="") {
    Initialize-GloamDat
    $folder=Get-GloamDatFolder $Cache $System
    if (-not (Test-Path -LiteralPath $folder -PathType Container)) { return }
    $files=@(Get-ChildItem -LiteralPath $folder -Filter '*.dat' -File | Sort-Object @{Expression={if ($_.Name -eq 'libretro.dat') {0} else {1}}},Name)
    if (-not $files.Count) { return }
    $fingerprints=[GloamDat]::Identify((Get-GloamPath $Root $Content),$ArchiveMember)
    foreach ($file in $files) {
        $key=$file.FullName+'|'+$file.Length+'|'+$file.LastWriteTimeUtc.Ticks
        if (-not $script:GloamDatRecords.ContainsKey($key)) {
            try { $index=[GloamDatIndex]::new([GloamDat]::Read($file.FullName)) }
            catch { Write-Host ('Skipping invalid DAT '+$file.Name+': '+$_.Exception.Message); continue }
            # Bound retained metadata when a batch crosses many systems/imports.
            if ($script:GloamDatRecords.Count -ge 2) { $script:GloamDatRecords.Clear() }
            $script:GloamDatRecords[$key]=$index
        }
        foreach ($fingerprint in $fingerprints) {
            foreach ($entry in $script:GloamDatRecords[$key].Find($fingerprint)) {
                $method=[GloamDat]::Match($entry,$fingerprint)
                if (-not $method) { continue }
                $source=if ($file.Name -eq 'libretro.dat') { 'Libretro DAT' } else { 'Imported DAT' }
                $note=$file.FullName+'.source.txt'
                if ($file.Name -eq 'libretro.dat' -and (Test-Path -LiteralPath $note) -and (Get-Item -LiteralPath $note).Length -lt 4096 -and [IO.File]::ReadAllText($note).Contains('/tosec/')) { $source='Libretro TOSEC DAT' }
                if ($fingerprint.Description -ne 'Complete file') { $method += "; " + $fingerprint.Description }
                [pscustomobject]@{ Name=$entry.Name; Method=$method; Source=$source }
            }
        }
    }
}

function Get-GloamIdentifiedCover([string]$Root, [string]$Content, [string]$System, [string]$Name, [string]$Region, [bool]$UseDat, [string]$Cache, [string]$ArchiveMember="", [string]$IdentityChoice="") {
    $system=Resolve-GloamArtworkSystem $Content $System
    if ($IdentityChoice) {
        $url=Get-GloamArtworkUrl $Root $Content $system $IdentityChoice $Region
        return [pscustomobject]@{Bytes=(Get-GloamArtworkDownload $url);Url=$url;Identification='User-selected title (review)';Name=$IdentityChoice}
    }
    if ($system -eq 'Nintendo DS (GameTDB ID)') {
        try {
            $url=Get-GloamArtworkUrl $Root $Content $system $Name $Region
            $bytes=Get-GloamArtworkDownload $url
            return [pscustomobject]@{ Bytes=$bytes; Url=$url; Identification='DS header ID'; Name=$Name }
        } catch { $system='Nintendo - Nintendo DS' }
    }
    $candidates=[Collections.Generic.List[object]]::new()
    $note='Filename match (review)'
    if ($UseDat) {
        $folder=Get-GloamDatFolder $Cache $system
        $target=Join-Path $folder 'libretro.dat'
        if (-not (Test-Path -LiteralPath $target) -and -not $script:GloamDatAttempts.ContainsKey($folder)) {
            $script:GloamDatAttempts[$folder]=$true
            try { Update-GloamDat $Cache $system | Out-Null }
            catch { Write-Host ('DAT download unavailable for '+$system+': '+$_.Exception.Message) }
        }
        try {
            $matches=@(Find-GloamDatNames $Root $Content $system $Cache $ArchiveMember)
            # Prefer stronger matches. CRC-only aliases cannot resolve a collision.
            $strong=@($matches | Where-Object { $_.Method -notlike 'CRC32*' })
            if ($strong.Count) { $matches=$strong }
            $names=@($matches | ForEach-Object Name | Sort-Object -Unique)
            if ($names.Count -gt 1) {
                $error=[InvalidOperationException]::new('Multiple DAT title matches. Select a title in the artwork chooser before downloading.')
                $error.Data['Choices']=$names
                throw $error
            }
            foreach ($match in $matches | Select-Object -First 8) {
                $candidates.Add([pscustomobject]@{ Name=$match.Name; Method=($match.Method+' / '+$match.Source) })
            }
            if (-not $matches.Count -and $note -eq 'Filename match (review)') { $note='Filename match; no DAT identification' }
        } catch { if ($_.Exception.Data.Contains('Choices')) { throw }; $note='Filename match; DAT unavailable'; Write-Host $_.Exception.Message }
    }
    $candidates.Add([pscustomobject]@{ Name=$Name; Method=$note })
    $seen=@{}; $last='No candidate names.'
    foreach ($candidate in $candidates) {
        Assert-GloamArtworkActive
        $url=Get-GloamArtworkUrl $Root $Content $system $candidate.Name $Region
        if ($seen.ContainsKey($url)) { continue }; $seen[$url]=$true
        try {
            $bytes=Get-GloamArtworkDownload $url
            return [pscustomobject]@{ Bytes=$bytes; Url=$url; Identification=$candidate.Method; Name=$candidate.Name }
        } catch { $last=$_.Exception.Message }
        finally { Start-Sleep -Milliseconds 500 }
    }
    $identity=if ($candidates.Count -gt 1) { 'DAT identified the file, but no cover was found at the checked names.' } else { 'No cover found; try another DAT, exact title or image URL.' }
    throw ($identity+' '+$last)
}
