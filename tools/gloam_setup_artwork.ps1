# SPDX-License-Identifier: GPL-3.0-or-later
# Artwork is downloaded on demand, never bundled with Gloam releases.
$GloamArtworkSystems = [ordered]@{
    'Automatic by folder'=''; 'Nintendo DS (GameTDB ID)'=''; 'Nintendo - Nintendo DS'='Nintendo DS'; 'Nintendo - Game Boy'='Game Boy';
    'Nintendo - Game Boy Color'='Game Boy Color'; 'Nintendo - Game Boy Advance'='Game Boy Advance';
    'Nintendo - Nintendo Entertainment System'='NES'; 'Nintendo - Super Nintendo Entertainment System'='SNES';
    'Sega - Mega Drive - Genesis'='Genesis'; 'Sega - Master System - Mark III'='Master System';
    'Sega - Game Gear'='Game Gear'; 'Sega - SG-1000'='SG-1000';
    'NEC - PC Engine - TurboGrafx 16'='PC Engine'; 'Bandai - WonderSwan'='WonderSwan';
    'SNK - Neo Geo Pocket Color'='Neo Geo Pocket'; 'Atari - 2600'='2600';
    'Atari - 5200'='5200'; 'Atari - 7800'='7800'; 'Atari - Lynx'='Lynx';
    'Coleco - ColecoVision'='ColecoVision'; 'Mattel - Intellivision'='Intellivision';
    'Nintendo - Pokemon Mini'='Pokemon Mini'; 'Commodore - 64'='Commodore 64';
    'Amstrad - CPC'='Amstrad CPC'; 'Sinclair - ZX Spectrum'='ZX Spectrum';
    'Microsoft - MSX'='MSX'; 'Microsoft - MSX2'='MSX2';
    'Atari - 8-bit Family'='8-bit'; 'Watara - Supervision'='Watara Supervision'
}

function Read-GloamArtworkRecord([string]$Root, [string]$Content) {
    $identity = 'sd:/' + (ConvertTo-GloamRelative $Root $Content)
    $hash = Get-GloamContentHash $identity
    $relative = '_nds/dsi-frontend/items/' + $hash + '.ini'
    $path = Get-GloamPath $Root $relative
    if (-not (Test-Path -LiteralPath $path) -and (Test-Path -LiteralPath (Get-GloamPath $Root ($relative -replace '\.ini$', '.bak')))) {
        $path = Get-GloamPath $Root ($relative -replace '\.ini$', '.bak')
    }
    $values = [ordered]@{ version='1'; content=$identity; cover=''; application=''; icon='' }
    if (Test-Path -LiteralPath $path) {
        if ((Get-Item -LiteralPath $path).Length -gt 4096) { throw 'Oversized item record; repair it before changing artwork.' }
        $values = [ordered]@{}
        foreach ($line in [IO.File]::ReadAllLines($path)) {
            if (-not $line -or $line.StartsWith(';') -or $line.StartsWith('#')) { continue }
            if ($line -notmatch '^([^=]+)=(.*)$' -or $values.Contains($Matches[1])) { throw 'Invalid item record; original kept.' }
            $values[$Matches[1]] = $Matches[2]
        }
        if ($values.Keys -cnotcontains 'version' -or $values.Keys -cnotcontains 'content' -or $values['version'] -ne '1' -or $values['content'] -cne $identity) { throw 'Item identity mismatch; original kept.' }
        foreach ($field in @('cover','application','icon')) {
            if (-not $values.Contains($field)) { $values[$field] = '' }
            if ($values[$field]) { ConvertTo-GloamRelative $Root $values[$field] | Out-Null }
        }
    }
    return [pscustomobject]@{ Values=$values; Relative=$relative; Hash=$hash }
}

function ConvertTo-GloamArtworkPng([byte[]]$Bytes) {
    if ($Bytes.Length -lt 8 -or $Bytes.Length -gt 8MB) { throw 'Choose a PNG/JPEG smaller than 8 MiB.' }
    $png = [BitConverter]::ToString($Bytes,0,8) -eq '89-50-4E-47-0D-0A-1A-0A'
    if (-not $png -and -not ($Bytes[0] -eq 255 -and $Bytes[1] -eq 216)) { throw 'The response is not a PNG or JPEG image.' }
    Add-Type -AssemblyName PresentationCore
    $stream = [IO.MemoryStream]::new($Bytes,$false)
    try {
        $decoder = [Windows.Media.Imaging.BitmapDecoder]::Create($stream,[Windows.Media.Imaging.BitmapCreateOptions]::PreservePixelFormat,[Windows.Media.Imaging.BitmapCacheOption]::OnDemand)
        $frame = $decoder.Frames[0]
        if ($frame.PixelWidth -gt 8192 -or $frame.PixelHeight -gt 8192 -or ([long]$frame.PixelWidth * $frame.PixelHeight) -gt 24000000) { throw 'Image dimensions are too large.' }
        $scale = [Math]::Min(1.0, 256.0 / [Math]::Max($frame.PixelWidth,$frame.PixelHeight))
        $image = [Windows.Media.Imaging.TransformedBitmap]::new($frame,[Windows.Media.ScaleTransform]::new($scale,$scale))
        $converted = [Windows.Media.Imaging.FormatConvertedBitmap]::new($image,[Windows.Media.PixelFormats]::Bgra32,$null,0)
        $encoder = [Windows.Media.Imaging.PngBitmapEncoder]::new()
        $encoder.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($converted))
        $output = [IO.MemoryStream]::new()
        try {
            $encoder.Save($output)
            if ($output.Length -gt 1MB) { throw 'Prepared image exceeds the launcher limit.' }
            return ,$output.ToArray()
        } finally { $output.Dispose() }
    } finally { $stream.Dispose() }
}

function Resolve-GloamArtworkSystem([string]$Content, [string]$System) {
    if (-not $GloamArtworkSystems.Contains($System)) { throw 'Choose an artwork system.' }
    if ($System -eq 'Automatic by folder') {
        if ([IO.Path]::GetExtension($Content) -in @('.nds','.dsi','.srl')) { $System='Nintendo DS (GameTDB ID)' }
        else {
            $matches = @($GloamArtworkSystems.Keys | Where-Object { $GloamArtworkSystems[$_] -and $GloamArtworkSystems[$_] -in ($Content -split '/') })
            if ($matches.Count -ne 1) { throw 'Choose a download system for this collection, or import an image.' }
            $System=$matches[0]
        }
    }
    return $System
}

function Get-GloamArtworkUrl([string]$Root, [string]$Content, [string]$System, [string]$Name, [string]$Region='US') {
    $System = Resolve-GloamArtworkSystem $Content $System
    if ($System -eq 'Nintendo DS (GameTDB ID)') {
        if ($Region -notin @('US','EN','JA','FR','DE','ES','IT','KO')) { throw 'Unsupported cover language.' }
        if ([IO.Path]::GetExtension($Content) -notin @('.nds','.dsi','.srl')) { throw 'DS title-ID lookup requires an NDS/DSi executable.' }
        $stream = [IO.File]::OpenRead((Get-GloamPath $Root $Content))
        try {
            $header = [byte[]]::new(16)
            if ($stream.Read($header,0,16) -ne 16) { throw 'Truncated DS header.' }
            $id = [Text.Encoding]::ASCII.GetString($header,12,4)
            if ($id -cnotmatch '^[A-Z0-9]{4}$' -or $id -in @('####','0000','PASS')) { throw 'No usable DS game ID.' }
            return "https://art.gametdb.com/ds/coverS/$Region/$id.png"
        } finally { $stream.Dispose() }
    }
    if (-not $Name.Trim()) { throw 'Enter the exact database title, including region when applicable.' }
    $name = $Name -replace '[&*/:`<>?\\|"_]', '_'
    return 'https://thumbnails.libretro.com/' + [Uri]::EscapeDataString($System) + '/Named_Boxarts/' + [Uri]::EscapeDataString($name) + '.png'
}

function Get-GloamArtworkDownload([string]$Url) {
    $uri = [Uri]$Url
    if (-not $uri.IsAbsoluteUri -or $uri.Scheme -ne 'https' -or $uri.UserInfo) { throw 'Use an HTTPS image URL without embedded credentials.' }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $request = [Net.HttpWebRequest]::Create($uri)
    $request.UserAgent = 'Gloam-Setup artwork manager'; $request.Timeout = 12000; $request.ReadWriteTimeout = 12000
    $request.AllowAutoRedirect = $false
    $response = $request.GetResponse()
    try {
        if ([int]$response.StatusCode -ne 200) { throw 'Use the final direct image URL (redirects are not followed).' }
        if ($response.ContentLength -gt 8MB) { throw 'Artwork download exceeds 8 MiB.' }
        $inputStream = $response.GetResponseStream(); $output = [IO.MemoryStream]::new()
        try {
            $buffer = [byte[]]::new(16384)
            $watch = [Diagnostics.Stopwatch]::StartNew()
            while (($count = $inputStream.Read($buffer,0,$buffer.Length)) -gt 0) {
                if ($watch.Elapsed.TotalSeconds -gt 30 -or $output.Length + $count -gt 8MB) { throw 'Artwork download exceeded its size/time limit.' }
                $output.Write($buffer,0,$count)
            }
            return ,(ConvertTo-GloamArtworkPng $output.ToArray())
        } finally { $inputStream.Dispose(); $output.Dispose() }
    } finally { $response.Close() }
}

function Get-GloamEmbeddedArtwork([string]$Path) {
    # Same bounded ID3v2.3/v2.4 APIC subset as the native launcher.
    function Read-ArtworkSize([byte[]]$Data, [int]$Offset, [bool]$SyncSafe) {
        $value = 0L
        for ($i=0; $i -lt 4; $i++) {
            $b = $Data[$Offset+$i]
            if ($SyncSafe -and ($b -band 128)) { return -1L }
            $value = ($value -shl $(if ($SyncSafe) {7} else {8})) -bor $b
        }
        return $value
    }
    $stream = [IO.File]::OpenRead($Path)
    try {
        $header = [byte[]]::new(10)
        if ($stream.Read($header,0,10) -ne 10 -or [Text.Encoding]::ASCII.GetString($header,0,3) -ne 'ID3' -or $header[3] -notin @(3,4) -or ($header[5] -band 128)) { return }
        $size = Read-ArtworkSize $header 6 $true
        if ($size -lt 10 -or $size -gt (1MB+64KB) -or $stream.Length -lt (10+$size)) { return }
        $tag = [byte[]]::new($size); $offset=0
        while ($offset -lt $size) { $read=$stream.Read($tag,$offset,$size-$offset); if (-not $read) { return }; $offset+=$read }
    } finally { $stream.Dispose() }
    $offset=0; $chosen=$null
    if ($header[5] -band 64) {
        $extended = Read-ArtworkSize $tag 0 ($header[3] -eq 4)
        if ($extended -lt 6) { return }
        $offset = $extended + $(if ($header[3] -eq 3) {4} else {0})
    }
    while ($offset+10 -le $tag.Length) {
        $name = [Text.Encoding]::ASCII.GetString($tag,$offset,4)
        $length = Read-ArtworkSize $tag ($offset+4) ($header[3] -eq 4)
        if ($length -le 0 -or $offset+10+$length -gt $tag.Length) { break }
        $end=$offset+10+$length; $start=$offset+10
        $flags=$tag[$offset+9]; $offset=$end
        if ($name -ne 'APIC' -or ($header[3] -eq 3 -and ($flags -band 224)) -or ($header[3] -eq 4 -and ($flags -band 79)) -or $length -lt 5) { continue }
        $encoding=$tag[$start]; $i=$start+1
        if ($encoding -gt 3) { continue }
        while ($i -lt $end -and $tag[$i]) { $i++ }
        if ($i+2 -ge $end) { continue }
        $mime=[Text.Encoding]::ASCII.GetString($tag,$start+1,$i-$start-1)
        if ($mime -notin @('image/png','image/jpeg')) { continue }
        $type=$tag[$i+1]; $i+=2
        if ($encoding -in @(1,2)) {
            while ($i+1 -lt $end -and ($tag[$i] -or $tag[$i+1])) { $i+=2 }; $i+=2
        } else { while ($i -lt $end -and $tag[$i]) { $i++ }; $i++ }
        if ($i -ge $end -or $end-$i -gt 1MB) { continue }
        $bytes=[byte[]]::new($end-$i); [Array]::Copy($tag,$i,$bytes,0,$bytes.Length)
        if ($type -eq 3) { return ,$bytes }
        if (-not $chosen) { $chosen=$bytes }
    }
    if ($chosen) { return ,$chosen }
}

function Get-GloamArtworkProfiles([string]$Root) {
    $folder=Get-GloamPath $Root '_nds/dsi-frontend/plugins'
    if (-not (Test-Path -LiteralPath $folder -PathType Container)) { return }
    foreach ($file in Get-ChildItem -LiteralPath $folder -Filter '*.ini') {
        try {
            $data=Read-GloamIni $file.FullName
            if ($data.Contains('plugin') -and $data['plugin']['cover'] -and $data['plugin']['disabled'] -ne '1') { $data['plugin'] }
        } catch { continue }
    }
}

function Get-GloamArtworkState([string]$Root, [string]$Content, [object[]]$Profiles) {
    $relative = ConvertTo-GloamRelative $Root $Content
    $path = Get-GloamPath $Root $relative
    $item = Get-Item -LiteralPath $path
    $record = Read-GloamArtworkRecord $Root $relative
    $candidates = [Collections.Generic.List[object]]::new()
    if ($record.Values['cover']) { $candidates.Add(@('Assigned', (ConvertTo-GloamRelative $Root $record.Values['cover']))) }
    if (-not $item.PSIsContainer) {
        foreach ($suffix in @('.png','.jpg','.jpeg')) { $candidates.Add(@('Sidecar', ($relative + $suffix))) }
    }
    foreach ($candidate in $candidates) {
        $imagePath = Get-GloamPath $Root $candidate[1]
        if ((Test-Path -LiteralPath $imagePath -PathType Leaf) -and (Get-Item -LiteralPath $imagePath).Length -gt 0) {
            return [pscustomobject]@{ Status='Present'; Kind=$candidate[0]; Image=$imagePath }
        }
    }
    if (-not $item.PSIsContainer -and $item.Extension -eq '.mp3' -and (Get-GloamEmbeddedArtwork $path)) {
        return [pscustomobject]@{ Status='Embedded'; Kind='Embedded album art'; Image=$path }
    }
    $folder = if ($item.PSIsContainer) { $relative } else { $relative.Substring(0,[Math]::Max(0,$relative.LastIndexOf('/'))) }
    foreach ($suffix in @('png','jpg','jpeg')) {
        $imagePath = Get-GloamPath $Root (($folder + '/cover.' + $suffix).TrimStart('/'))
        if (Test-Path -LiteralPath $imagePath -PathType Leaf) { return [pscustomobject]@{ Status='Present'; Kind='Folder cover'; Image=$imagePath } }
    }
    if (-not $item.PSIsContainer) {
        if ($null -eq $Profiles) { $Profiles=@(Get-GloamArtworkProfiles $Root) }
        foreach ($profile in $Profiles) {
            if ($item.Extension.TrimStart('.') -notin ($profile['extensions'] -split ',')) { continue }
            if ($profile['content_root']) {
                $scope=ConvertTo-GloamRelative $Root $profile['content_root']
                if (-not $relative.StartsWith($scope.TrimEnd('/')+'/',[StringComparison]::OrdinalIgnoreCase)) { continue }
            }
            $imagePath=Get-GloamPath $Root (ConvertTo-GloamRelative $Root $profile['cover'])
            if (Test-Path -LiteralPath $imagePath -PathType Leaf) { return [pscustomobject]@{ Status='Default'; Kind='Association default'; Image=$imagePath } }
        }
    }
    return [pscustomobject]@{ Status='Missing'; Kind='No individual/folder cover'; Image='' }
}

function Assert-GloamArtworkActive {
    if ((Test-Path variable:script:GloamCancelFile) -and (Test-Path -LiteralPath $script:GloamCancelFile)) {
        throw [OperationCanceledException]::new('Artwork operation cancelled; no SD artwork was changed.')
    }
}

function Get-GloamArtworkLibrary([string]$Root, [string]$Folder, [bool]$Recursive) {
    $relative = ConvertTo-GloamRelative $Root $Folder
    $pending = [Collections.Generic.Queue[string]]::new(); $pending.Enqueue($relative)
    $count = 0; $profiles = @(Get-GloamArtworkProfiles $Root)
    while ($pending.Count) {
        Assert-GloamArtworkActive
        $current = $pending.Dequeue()
        foreach ($item in Get-ChildItem -LiteralPath (Get-GloamPath $Root $current) | Sort-Object Name) {
            Assert-GloamArtworkActive
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            if ($item.Name -in @('bios','saves','_nds','_gba','data','.git')) { continue }
            if (-not $item.PSIsContainer -and ($item.Extension -in @('.png','.jpg','.jpeg','.sav','.pub','.prv','.bak','.ini','.json','.xml','.log') -or $item.Name -match '^README')) { continue }
            $content = ($current + '/' + $item.Name).TrimStart('/')
            if ($item.PSIsContainer -and $Recursive) { $pending.Enqueue($content) }
            if (++$count -gt 5000) { throw 'More than 5,000 items. Select a smaller folder.' }
            try { $state = Get-GloamArtworkState $Root $content $profiles }
            catch { $state = [pscustomobject]@{ Status='Check record'; Kind=$_.Exception.Message; Image='' } }
            [pscustomobject]@{ Selected=$false; Content=$content; Name=$item.Name; IsFolder=$item.PSIsContainer;
                ArchiveMember=''; IdentityChoice=''; IdentityChoices=@();
                MatchName=$(if ($item.PSIsContainer) { $item.Name } else { [IO.Path]::GetFileNameWithoutExtension($item.Name) });
                Status=$state.Status; Kind=$state.Kind; Image=$state.Image; Candidate=''; CandidateHash=''; Source=''; Identification=''; Result='' }
        }
    }
}

function Set-GloamArtwork([string]$Root, [string]$Content, [string]$Candidate, [string]$ExpectedHash, [string]$Source, [bool]$Replace) {
    $contentPath = Get-GloamPath $Root $Content
    if (-not (Test-Path -LiteralPath $contentPath)) { throw 'Content moved or disappeared; scan again.' }
    $state = Get-GloamArtworkState $Root $Content
    if ($state.Status -ne 'Missing' -and -not $Replace) { throw 'Existing or embedded artwork was preserved. Enable replacement to override it.' }
    if ((Get-Item -LiteralPath $Candidate).Length -gt 1MB -or (Get-GloamHash $Candidate) -cne $ExpectedHash) { throw 'The preview changed. Prepare it again.' }
    $bytes = ConvertTo-GloamArtworkPng ([IO.File]::ReadAllBytes($Candidate))
    $record = Read-GloamArtworkRecord $Root $Content
    $digest = [Security.Cryptography.SHA256]::Create()
    try { $imageHash = ([BitConverter]::ToString($digest.ComputeHash($bytes))).Replace('-','').ToLowerInvariant() } finally { $digest.Dispose() }
    $image = '_nds/dsi-frontend/covers/' + $record.Hash + '-' + $imageHash.Substring(0,12) + '.png'
    # Store immutable prepared images first; a failed record write leaves the old
    # assignment intact. Old images remain available for manual recovery.
    $imagePath = Get-GloamPath $Root $image
    if (Test-Path -LiteralPath $imagePath) {
        if ((Get-GloamHash $imagePath) -ne $imageHash) { throw 'Managed cover filename collision; existing image kept.' }
    } else { Write-GloamFile $Root $image $bytes | Out-Null }
    $record.Values['cover'] = 'sd:/' + $image
    Save-GloamArtworkRecord $Root $record
    if ($Source) { Write-GloamFile $Root ($image + '.source.txt') ([Text.Encoding]::UTF8.GetBytes($Source + "`n")) | Out-Null }
}

function Save-GloamArtworkRecord([string]$Root, $Record) {
    $lines = @($Record.Values.Keys | ForEach-Object { "$_=$($Record.Values[$_])" })
    if (@($Record.Values.Values | Where-Object { $_ -match '[\r\n]' }).Count) { throw 'Invalid multiline item field.' }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($lines -join "`n") + "`n")
    if ($bytes.Length -gt 4096) { throw 'Item record exceeds launcher limit.' }
    Write-GloamFile $Root $Record.Relative $bytes $true | Out-Null
}

function Clear-GloamArtwork([string]$Root, [string]$Content) {
    $record = Read-GloamArtworkRecord $Root $Content
    $record.Values['cover'] = ''
    Save-GloamArtworkRecord $Root $record
    # Sidecars, shared covers, embedded art and managed images are not deleted.
}
