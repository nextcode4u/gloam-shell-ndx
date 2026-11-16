# SPDX-License-Identifier: GPL-3.0-or-later
# Read-only health/diagnostic inspection and explicitly requested asset repair.
function Test-GloamScriptFontBytes([byte[]]$Data, [string]$Id) {
    if ($Id -notin @('japanese','extended')) { throw 'Unsupported shared script font name.' }
    if ($Data.Length -lt 20 -or $Data.Length -gt (20+16384*29) -or [Text.Encoding]::ASCII.GetString($Data,0,8) -cne 'GSFONT01') { throw 'Invalid script font header or size.' }
    $count=[BitConverter]::ToUInt32($Data,8); $depth=[int]$Data[17]
    if (-not $count -or $count -gt 16384 -or $Data.Length -ne (20+$count*29) -or $Data[16] -ne 12 -or $depth -notin @(1,2) -or $Data[18] -or $Data[19]) { throw 'Invalid script font dimensions/count.' }
    [long]$hash=2166136261
    for ($i=20; $i -lt $Data.Length; $i++) { $hash=(($hash -bxor [long]$Data[$i])*16777619) -band 4294967295L }
    if ($hash -ne [BitConverter]::ToUInt32($Data,12)) { throw 'Script font checksum mismatch.' }
    $previous=0L; $first=if ($Id -eq 'japanese') {8192} else {256}; $last=if ($Id -eq 'japanese') {65535} else {1279}
    for ($i=0; $i -lt $count; $i++) {
        $offset=20+$i*29; $cp=[BitConverter]::ToUInt32($Data,$offset); $advance=[int]$Data[$offset+4]
        if ($cp -lt $first -or $cp -gt $last -or $cp -le $previous -or ($cp -ge 55296 -and $cp -le 57343) -or -not $advance -or $advance*$depth -gt 16) { throw 'Invalid script glyph order, scalar or advance.' }
        for ($row=0; $row -lt 12; $row++) {
            if (([int][BitConverter]::ToUInt16($Data,$offset+5+$row*2) -shr ($advance*$depth)) -ne 0) { throw 'Script glyph exceeds its advance.' }
        }
        $previous=$cp
    }
    return [int]$count
}
function Test-GloamLanguageBytes([byte[]]$Data) {
    if (-not $Data.Length -or $Data.Length -gt 32768) { throw 'Language catalog must be 1-32768 bytes.' }
    $utf8=[Text.UTF8Encoding]::new($false,$true); $text=$utf8.GetString($Data)
    if ($text.Contains([char]0)) { throw 'Catalog contains embedded NUL bytes.' }
    if ($text.Length -and $text[0] -eq [char]0xfeff) { $text=$text.Substring(1) }
    $basic=@('home','games','apps','media','system','files','settings','reading_folder','browser_hint','tab_hint','installed_titles','empty_folder')
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $version=$false; $extra=0; $used=0
    foreach ($line in ($text -split "`n")) {
        $line=if ($line.EndsWith("`r")) { $line.Substring(0,$line.Length-1) } else { $line }
        if ($utf8.GetByteCount($line) -ge 511) { throw 'Language line exceeds the native reader limit.' }
        if (-not $line -or $line.StartsWith(';') -or $line.StartsWith('#')) { continue }
        $split=$line.IndexOf('='); if ($split -lt 1) { throw 'Expected catalog key=value.' }
        $key=$line.Substring(0,$split); $value=$line.Substring($split+1)
        if (-not $seen.Add($key)) { throw "Duplicate language key: $key" }
        if ($key -ceq 'version') { if ($value -cne '1') { throw 'Expected catalog version 1.' }; $version=$true; continue }
        $isBasic=$basic -ccontains $key
        if (-not $isBasic -and ($key.Length -ge 48 -or $key -cnotmatch '^(setting|section|dialog|value)\.[a-z0-9_.]+$')) { throw "Unknown language key: $key" }
        $size=$utf8.GetByteCount($value); $cap=if ($key.Contains('.help')) {256} else {96}
        if (-not $size -or $size -ge $cap -or $value -match '[\x00-\x1f\x7f]') { throw "Invalid language text: $key" }
        if ($basic[0..5] -ccontains $key -and $size -ge 64) { throw 'Tab translation exceeds 63 bytes.' }
        if (-not $isBasic) { $extra++; $used+=$size+1 }
    }
    if (-not $version -or $extra -gt 384 -or $used -gt 24576) { throw 'Invalid catalog version or storage limits.' }
    return $seen.Count-1
}
function Get-GloamStartupRows([string]$Root) {
    $file=Get-GloamPath $Root '_nds/dsi-frontend/diagnostics.txt'
    if (-not [IO.File]::Exists($file)) { return [pscustomobject]@{Area='Startup';Item='Timing report';Status='Not measured';Details='Export diagnostics on the DSi, then check setup again.';Action='Export diagnostics on console'} }
    if (([IO.FileInfo]::new($file)).Length -gt 65536) { throw 'Diagnostics report exceeds 64 KiB.' }
    $values=@{}; $frontend=''; $cold=''
    $lines=[IO.File]::ReadAllLines($file)
    if (-not $lines.Length -or $lines[0] -cne 'Gloam Shell NDX diagnostics v1') { throw 'Unrecognized diagnostics report.' }
    foreach ($line in $lines) {
        if ($line -cmatch '^frontend_version=(v[0-9]+\.[0-9]+\.[0-9]+(-(preview|rc)\.[0-9]+)?)$') { $frontend=$Matches[1] }
        if ($line -cmatch '^cold_size_profile=([01])$') { $cold=$Matches[1] }
        if ($line -cmatch '^(boot_[a-z_]+)=(\d+)$') {
            if ($values.ContainsKey($Matches[1])) { throw 'Duplicate startup measurement.' }
            $number=0L
            if (-not [long]::TryParse($Matches[2],[ref]$number) -or $number -gt 120000) { throw 'Invalid startup measurement.' }
            $values[$Matches[1]]=$number
        }
    }
    if ($frontend) { [pscustomobject]@{Area='Startup';Item='Last exported frontend';Status='Observed';Details=$frontend;Action='May predate the currently installed BOOT.NDS'} }
    if ($cold -ne '') { [pscustomobject]@{Area='Startup';Item='Last exported build profile';Status=$(if ($cold -eq '1') {'Experimental'} else {'Observed'});Details=$(if ($cold -eq '1') {'Cold-path size profile'} else {'Baseline speed profile'});Action='Compare equivalent profiles on the console'} }
    if (-not $values.ContainsKey('boot_complete') -or $values.boot_complete -ne 1) { return [pscustomobject]@{Area='Startup';Item='Timing report';Status='Not measured';Details='This report has no complete startup profile.';Action='Export diagnostics after browsing opens'} }
    $labels=[ordered]@{boot_total_ms='Total to completed browser';boot_hardware_ms='Hardware setup';boot_sd_mount_ms='SD mount';boot_configuration_ms='Configuration';boot_associations_ms='File associations';boot_dashboard_ms='Dashboard';boot_theme_ms='Theme preparation';boot_script_fonts_ms='Script fonts';boot_first_folder_ms='First folder';boot_splash_wait_ms='Splash wait';boot_first_audio_ms='Initial audio'}
    foreach ($key in $labels.Keys) { if ($values.ContainsKey($key)) { [pscustomobject]@{Area='Startup';Item=$labels[$key];Status='Observed';Details=([string]$values[$key]+' ms. Nested stages overlap; total excludes the preceding loader.');Action='Reported by the exporting build'} } }
}
function Get-GloamSetupHealth([string]$Root) {
    $root=Get-GloamRoot $Root
    $required=[ordered]@{'BOOT.NDS'='Frontend';'_nds/nds-bootstrap-release.nds'='Retail backend';'_nds/nds-bootstrap-hb-release.nds'='Homebrew backend';'_nds/dsi-frontend/loaders/slot1launch.nds'='Slot-1 helper'}
    foreach ($relative in $required.Keys) {
        $path=Get-GloamPath $root $relative; $present=[IO.File]::Exists($path) -and ([IO.FileInfo]::new($path)).Length -ge 512
        [pscustomobject]@{Area='Loaders';Item=$required[$relative];Status=$(if ($present) {'Present'} else {'Missing / invalid'});Details=$relative;Action=$(if ($present) {'Presence is not a boot/compatibility check'} else {'Update Gloam + backends'})}
    }
    $version=Get-GloamPath $root '_nds/release-bootstrap.ver'
    if ([IO.File]::Exists($version) -and ([IO.FileInfo]::new($version)).Length -le 256) { [pscustomobject]@{Area='Loaders';Item='Backend version file';Status='Observed';Details=([IO.File]::ReadAllText($version).Trim());Action='Version supplied by the installed backend package'} }
    foreach ($id in @('japanese','extended')) {
        $status='Missing'; $detail='Names using this script may display question marks.'
        try {
            $file=Get-GloamPath $root ('_nds/dsi-frontend/fonts/'+$id+'.gsf')
            if ([IO.File]::Exists($file)) {
                if (([IO.FileInfo]::new($file)).Length -gt (20+16384*29)) { throw 'Oversized script font.' }
                $count=Test-GloamScriptFontBytes ([IO.File]::ReadAllBytes($file)) $id
                $status='Ready'; $detail="$count validated glyphs; checksum does not establish authenticity."
            }
        } catch { $status='Invalid'; $detail=$_.Exception.Message }
        [pscustomobject]@{Area='Shared fonts';Item=$id;Status=$status;Details=$detail;Action=$(if ($status -eq 'Ready') {'No action'} else {'Repair shared assets'})}
    }
    try {
        Assert-GloamConfigurationReady $root
        $state=Read-GloamState $root
        [pscustomobject]@{Area='Configuration';Item='Launcher settings';Status=$(if ($state.Issues.Count) {'Warning'} else {'Ready'});Details=$(if ($state.Issues.Count) {'Invalid values use defaults: '+($state.Issues -join ', ')} else {'Readable; existing settings and navigation are retained.'});Action='Settings'}
        $locale=$state.Values.interface_language
        if ($locale -ne 'en') {
            $relative=if ($locale -eq 'auto') {'_nds/dsi-frontend/language.ini'} else {'_nds/dsi-frontend/languages/'+$locale+'.ini'}
            $file=Get-GloamPath $root $relative
            $status='English fallback'; $details='No custom catalog selected/installed.'
            if ([IO.File]::Exists($file)) { if (([IO.FileInfo]::new($file)).Length -gt 32768) { throw 'Oversized language catalog.' }; Test-GloamLanguageBytes ([IO.File]::ReadAllBytes($file)) | Out-Null; $status='Ready'; $details=$relative }
            elseif ($locale -ne 'auto') { $status='Missing'; $details='Selected catalog is absent; Gloam uses English.' }
            [pscustomobject]@{Area='Language';Item=$locale;Status=$status;Details=$details;Action='Import a language pack or select English'}
        }
        if ($state.Values.theme_id) {
            $folder=Get-GloamPath $root ('_nds/dsi-frontend/themes/'+$state.Values.theme_id)
            try { $theme=Test-GloamTheme $folder; $status=if (-not $theme.Valid) {'Invalid'} elseif ($theme.Warnings.Count) {'Warning'} else {'Ready'}; $details=(@($theme.Errors)+@($theme.Warnings)) -join '; ' }
            catch { $status='Missing / invalid'; $details=$_.Exception.Message }
            [pscustomobject]@{Area='Theme';Item=$state.Values.theme_id;Status=$status;Details=$details;Action='Inspect on Themes page'}
        }
    } catch { [pscustomobject]@{Area='Configuration';Item='Settings or selected catalog';Status='Invalid';Details=$_.Exception.Message;Action='Restore or correct configuration'} }
    $folder=Get-GloamPath $root '_nds/dsi-frontend/plugins'
    if ([IO.Directory]::Exists($folder)) {
        $profiles=@(Get-ChildItem -LiteralPath $folder -File -Filter '*.ini' | Sort-Object Name | Select-Object -First 33)
        $missing=0; $invalid=0
        foreach ($profile in $profiles | Select-Object -First 32) {
            try {
                $ini=Read-GloamIni $profile.FullName
                if (-not $ini.Contains('plugin') -or $ini['plugin']['disabled'] -eq '1') { continue }
                foreach ($key in @('application','dsipath','ntrpath')) {
                    if (-not $ini['plugin'][$key]) { continue }
                    $relative=ConvertTo-GloamRelative $root $ini['plugin'][$key]
                    if (-not [IO.File]::Exists((Get-GloamPath $root $relative))) { $missing++ }
                }
            } catch { $invalid++ }
        }
        [pscustomobject]@{Area='File handlers';Item='Optional applications';Status=$(if ($invalid -or $profiles.Count -gt 32) {'Warning'} elseif ($missing) {'Optional'} else {'Ready'});Details=("$missing referenced targets absent; $invalid invalid profiles. Native limit: 32 profiles.");Action='Use Add-ons for installation and BIOS checks'}
    }
    try { Get-GloamStartupRows $root }
    catch { [pscustomobject]@{Area='Startup';Item='Timing report';Status='Invalid';Details=$_.Exception.Message;Action='Export a new diagnostics report'} }
}
function Import-GloamLanguagePack([string]$Root, [string[]]$Sources) {
    $root=Get-GloamRoot $Root; $plan=@(); $seen=@{}
    foreach ($source in $Sources) {
        $info=[IO.FileInfo]::new($source); $id=$info.BaseName
        if (-not $info.Exists -or $info.Length -gt (20+16384*29) -or $id -cnotmatch '^[A-Za-z0-9_-]{1,31}$') { throw 'Invalid language/font input.' }
        $data=[IO.File]::ReadAllBytes($info.FullName)
        if ($info.Extension -ieq '.ini') {
            if ($id -in @('en','auto')) { throw 'en and auto are reserved language IDs.' }
            Test-GloamLanguageBytes $data | Out-Null; $relative='_nds/dsi-frontend/languages/'+$id+'.ini'
        } elseif ($info.Extension -ieq '.gsf') {
            Test-GloamScriptFontBytes $data $id | Out-Null; $relative='_nds/dsi-frontend/fonts/'+$id+'.gsf'
        } else { throw 'Choose .ini catalogs or .gsf shared fonts.' }
        if ($seen.ContainsKey($relative)) { throw 'Duplicate import destination.' }; $seen[$relative]=$true
        $target=Get-GloamPath $root $relative
        if ([IO.Directory]::Exists($target)) { throw 'A folder blocks the import destination.' }
        $plan+=@{Relative=$relative;Data=$data}
    }
    foreach ($item in $plan) { Write-GloamFile $root $item.Relative $item.Data $true | Out-Null; Write-Host ('Installed '+$item.Relative) }
}
function Repair-GloamSharedAssets([string]$Root, [string]$ArchivePath) {
    $root=Get-GloamRoot $Root; $archive=[IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $wanted=@('_nds/dsi-frontend/fonts/japanese.gsf','_nds/dsi-frontend/fonts/extended.gsf','_nds/dsi-frontend/languages/ja.ini','_nds/dsi-frontend/languages/es.ini')
        $seen=@{}; $plan=@(); $total=0L
        foreach ($entry in $archive.Entries) {
            $name=$entry.FullName; $total+=$entry.Length
            if ($name.Contains('\') -or $name.Contains(':') -or $name.StartsWith('/') -or $name -match '(^|/)\.\.?(/|$)' -or ($entry.ExternalAttributes -shr 28 -band 15) -eq 10 -or $seen.ContainsKey($name.ToLowerInvariant())) { throw 'Unsafe or duplicate archive member.' }
            $seen[$name.ToLowerInvariant()]=$true
            if ($seen.Count -gt 30000 -or $entry.Length -gt 64MB -or $total -gt 256MB) { throw 'Oversized repair archive.' }
            if ($name -cnotin $wanted) { continue }
            if ($entry.Length -gt (20+16384*29)) { throw 'Oversized shared asset.' }
            $data=[byte[]]::new([int]$entry.Length); $stream=$entry.Open()
            try { $offset=0; while ($offset -lt $data.Length) { $n=$stream.Read($data,$offset,$data.Length-$offset); if (-not $n) { throw 'Truncated shared asset.' }; $offset+=$n }; if ($stream.ReadByte() -ne -1) { throw 'Shared asset exceeds its declared length.' } }
            finally { $stream.Dispose() }
            $target=Get-GloamPath $root $name
            if ([IO.Directory]::Exists($target)) { throw 'A folder blocks a shared asset.' }
            $id=[IO.Path]::GetFileNameWithoutExtension($name)
            if ($name.EndsWith('.gsf')) { Test-GloamScriptFontBytes $data $id | Out-Null } else { Test-GloamLanguageBytes $data | Out-Null }
            $plan+=@{Relative=$name;Data=$data;Id=$id}
        }
        foreach ($required in $wanted[0..1]) { if ($required -cnotin @($plan | ForEach-Object Relative)) { throw 'This SD package predates shared fonts or is incomplete. Choose the matching newer release.' } }
        foreach ($item in $plan) {
            $file=Get-GloamPath $root $item.Relative; $valid=$false
            if ([IO.File]::Exists($file) -and ([IO.FileInfo]::new($file)).Length -le (20+16384*29)) {
                try {
                    $existing=[IO.File]::ReadAllBytes($file)
                    if ($item.Relative.EndsWith('.gsf')) { Test-GloamScriptFontBytes $existing $item.Id | Out-Null } else { Test-GloamLanguageBytes $existing | Out-Null }
                    $valid=$true
                } catch { $valid=$false }
            }
            if ($valid) { Write-Host ('Kept valid/customized '+$item.Relative); continue }
            Write-GloamFile $root $item.Relative $item.Data $true | Out-Null
            Write-Host ('Repaired '+$item.Relative)
        }
    } finally { $archive.Dispose() }
}
