# SPDX-License-Identifier: GPL-3.0-or-later
# A-Z/# and flatten workflow follows nextcode4u/PowerShell-File-Organizer-Scripts.
# This ROM-aware planner/transaction implementation is original Gloam code.
$GloamRomExtensions = 'nds,dsi,srl,gb,gbc,sgb,gba,nes,fds,sfc,smc,gen,md,sms,gg,sg,pce,ws,wsc,ngp,ngc,lnx,min,a26,a52,a78,atr,xex,col,m5,msx,mx2,mxd,crt,d64,prg,rpk,cas,ccc,c10,k7,int,sv,cpr,dan,dsk,tap,tzx,z80'

function Get-GloamOrganizerFolder([string]$Root, [string]$Folder) {
    $relative = ConvertTo-GloamRelative $Root $Folder
    $tabs = Read-GloamIni (Get-GloamPath $Root '_nds/dsi-frontend/tabs.ini')
    $games = 'roms'
    if ($tabs.Contains('roots') -and $tabs['roots']['games']) { $games = ConvertTo-GloamRelative $Root $tabs['roots']['games'] }
    if ($relative -notlike 'roms/*' -and $relative -ne $games -and -not $relative.StartsWith($games + '/', [StringComparison]::OrdinalIgnoreCase)) { throw 'Choose one system folder inside roms or the configured Games library.' }
    if ($relative -eq $games -or $relative -notmatch '/' -or @($relative -split '/' | Where-Object { $_ -in @('bios','saves','scummvm','titles') }).Count) { throw 'Choose a single-system ROM folder, not the library root, BIOS, saves or multi-file ScummVM data.' }
    # Use actual FAT/Windows directory casing for the launcher's identity hash.
    $cursor = Get-GloamRoot $Root; $parts = @()
    foreach ($part in $relative -split '/') {
        $match = @(Get-ChildItem -LiteralPath $cursor -Directory -Force | Where-Object { $_.Name -eq $part })
        if ($match.Count -ne 1) { throw 'Choose an existing system folder.' }
        $parts += $match[0].Name; $cursor = Get-GloamPath $Root ($parts -join '/')
    }
    return $parts -join '/'
}

# Discover system folders without walking ROM collections or following links.
function Get-GloamOrganizerFolders([string]$Root) {
    $tabs = Read-GloamIni (Get-GloamPath $Root '_nds/dsi-frontend/tabs.ini')
    $libraries = @('roms')
    if ($tabs.Contains('roots') -and $tabs['roots']['games']) {
        $libraries += ConvertTo-GloamRelative $Root $tabs['roots']['games']
    }
    foreach ($library in $libraries | Select-Object -Unique) {
        $path = Get-GloamPath $Root $library
        if (-not (Test-Path -LiteralPath $path -PathType Container)) { continue }
        foreach ($folder in Get-ChildItem -LiteralPath $path -Directory | Sort-Object Name) {
            if ($folder.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            try { $relative = Get-GloamOrganizerFolder $Root ($library + '/' + $folder.Name) }
            catch { continue } # Exclude BIOS, saves and multi-file ScummVM data.
            if ($folder.Name -eq 'Atari') {
                foreach ($system in Get-ChildItem -LiteralPath $folder.FullName -Directory | Sort-Object Name) {
                    if ($system.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
                    try { Get-GloamOrganizerFolder $Root ($relative + '/' + $system.Name) } catch { continue }
                }
            } else { $relative }
        }
    }
}

function Get-GloamOrganizerStamp([string]$Root, [string]$Relative) {
    $file = Get-Item -LiteralPath (Get-GloamPath $Root $Relative)
    if ($file.PSIsContainer) { throw 'A move source is a folder.' }
    return [string]$file.Length + ':' + $file.LastWriteTimeUtc.Ticks
}

function Get-GloamContentHash([string]$Identity) {
    if (-not ('GloamOrganizerIdentity' -as [type])) {
        Add-Type -TypeDefinition @'
public static class GloamOrganizerIdentity {
    public static string Hash(string value) {
        ulong hash = 14695981039346656037UL;
        foreach (byte b in System.Text.Encoding.UTF8.GetBytes(value))
            hash = unchecked((hash ^ b) * 1099511628211UL);
        return hash.ToString("x16");
    }
}
'@
    }
    return [GloamOrganizerIdentity]::Hash($Identity)
}

function Get-GloamOrganizerEdits([string]$Root, $Map) {
    $base = '_nds/dsi-frontend/'; $paths = [Collections.Generic.List[string]]::new()
    foreach ($name in @('state.ini','state.bak','recent.ini','recent.bak')) { $paths.Add($base + $name) }
    foreach ($directory in @('items','games','plugins','plugins/startup')) {
        $folder = Get-GloamPath $Root ($base + $directory)
        if (-not (Test-Path -LiteralPath $folder -PathType Container)) { continue }
        foreach ($file in Get-ChildItem -LiteralPath $folder -File -Force | Where-Object { $_.Extension -in @('.ini','.bak') }) {
            if ($paths.Count -ge 20000) { throw 'Too many frontend records; organize a smaller library first.' }
            $paths.Add($base + $directory + '/' + $file.Name)
        }
    }
    $total = 0L
    foreach ($relative in $paths | Sort-Object) {
        $path = Get-GloamPath $Root $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        if ((Get-Item -LiteralPath $path).Length -gt 128KB) { throw "Oversized frontend record: $relative" }
        $before = [IO.File]::ReadAllBytes($path)
        $text = [Text.UTF8Encoding]::new($false,$true).GetString($before)
        if ($text.Contains([char]0)) { throw "Binary data in frontend record: $relative" }
        $identity = ''; $newIdentity = ''; $changed = $false
        $lines = [Collections.Generic.List[string]]::new()
        foreach ($line in $text -split "`r?`n") {
            $updated = $line
            if ($line -cmatch '^(content|favorite|selected_path|cover|icon|application|dsipath|ntrpath)=(.*)$') {
                $key = $Matches[1]; $value = $Matches[2]
                if ($key -eq 'content') { $identity = $value }
                if ($Map.ContainsKey($value)) {
                    $updated = $key + '=' + $Map[$value]
                    if ($key -eq 'content') { $newIdentity = $Map[$value] }
                }
            } elseif ($line -cmatch '^request=([0-9]+),(.*)$') {
                $timestamp = $Matches[1]; $value = $Matches[2]
                if ($Map.ContainsKey($value)) { $updated = 'request=' + $timestamp + ',' + $Map[$value] }
            }
            if ($updated -cne $line) { $changed = $true }; $lines.Add($updated)
        }
        if (-not $changed) { continue }
        $destination = $relative
        if ($newIdentity -and $relative -cmatch '^_nds/dsi-frontend/(items|games)/([0-9a-f]{16})(\.cheats)?\.(ini|bak)$') {
            $directory = $Matches[1]; $oldHash = $Matches[2]; $extra = $Matches[3]; $suffix = $Matches[4]
            if ((Get-GloamContentHash $identity) -cne $oldHash) { throw 'A per-title record has a mismatched identity. Repair it before organizing.' }
            $destination = $base + $directory + '/' + (Get-GloamContentHash $newIdentity) + $extra + '.' + $suffix
            if ($destination -cne $relative -and (Test-Path -LiteralPath (Get-GloamPath $Root $destination))) { throw 'A new per-title identity already exists. No files were moved.' }
        }
        $after = [Text.UTF8Encoding]::new($false).GetBytes($lines -join "`n")
        $total += $before.Length + $after.Length
        if ($total -gt 8MB) { throw 'Too much affected configuration; choose a smaller ROM folder.' }
        [pscustomobject]@{ Source=$relative; Destination=$destination; Before=[Convert]::ToBase64String($before); After=[Convert]::ToBase64String($after) }
    }
}

function New-GloamOrganizerPlan([string]$Root, [string]$Folder, [string]$Mode, [string]$Extensions = $GloamRomExtensions) {
    Assert-GloamConfigurationReady $Root
    if ($Mode -notin @('Alphabetize','Flatten')) { throw 'Choose A-Z folders or flatten.' }
    $folder = Get-GloamOrganizerFolder $Root $Folder
    $suffixes = @($Extensions.ToLowerInvariant() -split '[,\s]+' | Where-Object { $_ } | ForEach-Object { $_.TrimStart('.') } | Sort-Object -Unique)
    if (-not $suffixes.Count -or $suffixes.Count -gt 96) { throw 'Choose at least one ROM extension.' }
    foreach ($suffix in $suffixes) {
        if ($suffix -notmatch '^[a-z0-9]{1,15}$' -or $suffix -in @('sav','pub','prv','srm','png','jpg','jpeg','ini','bak','ps1','cmd','exe','dll','cue','m3u','opf')) { throw "Not a standalone ROM extension: $suffix" }
    }
    $parents = [Collections.Generic.List[string]]::new()
    if ($Mode -eq 'Alphabetize') { $parents.Add($folder) }
    else {
        foreach ($child in Get-ChildItem -LiteralPath (Get-GloamPath $Root $folder) -Directory -Force) {
            if ($child.Name -cmatch '^[A-Za-z#]$') { $parents.Add($folder + '/' + $child.Name) }
        }
    }
    $groups = [Collections.Generic.List[object]]::new(); $owners = @{}; $targets = @{}
    $companions = @('sav','pub','prv','srm','dsv','eep','fla','yss','state','cht','ips','bps','ups','cc','pal','png','jpg','jpeg','icon.png')
    foreach ($n in 0..9) { foreach ($ext in @('sav','pub','prv','ys','st','ss','state')) { $companions += $ext + $n } }
    foreach ($parent in $parents) {
        $files = @(Get-ChildItem -LiteralPath (Get-GloamPath $Root $parent) -File -Force | Sort-Object Name)
        $adjacent = @{}; $saves = @{}
        foreach ($file in $files) { $adjacent[$file.Name] = $file }
        $saveFolder = Get-GloamPath $Root ($parent + '/saves')
        if (Test-Path -LiteralPath $saveFolder -PathType Container) {
            foreach ($file in Get-ChildItem -LiteralPath $saveFolder -File -Force) { $saves[$file.Name] = $file }
        }
        foreach ($rom in $files) {
            if ($rom.Extension.TrimStart('.').ToLowerInvariant() -notin $suffixes -or $rom.Name.StartsWith('.') -or $rom.Name -match '^(boot|scummvm|bios|disksys|readme|license|licence|credits|changelog)(\.|$)' -or
                $rom.Attributes -band ([IO.FileAttributes]::Hidden -bor [IO.FileAttributes]::System)) { continue }
            if ($groups.Count -ge 50000) { throw 'Too many ROMs for one operation.' }
            $first = $rom.Name.Substring(0,1).ToUpperInvariant()
            $bucket = if ($first -cmatch '^[A-Z]$') { $first } else { '#' }
            $destinationFolder = if ($Mode -eq 'Alphabetize') { $folder + '/' + $bucket } else { $folder }
            # Reuse actual spelling of an existing letter directory for content IDs.
            if ($Mode -eq 'Alphabetize' -and (Test-Path -LiteralPath (Get-GloamPath $Root $destinationFolder) -PathType Container)) {
                $actual = @(Get-ChildItem -LiteralPath (Get-GloamPath $Root $folder) -Directory -Force | Where-Object { $_.Name -eq $bucket })[0]
                $destinationFolder = $folder + '/' + $actual.Name
            }
            $group = [pscustomobject]@{ Title=$parent + '/' + $rom.Name; Reason=''; Moves=[Collections.Generic.List[object]]::new() }
            $members = @{}; $members[$rom.Name] = [pscustomobject]@{ File=$rom; Save=$false }
            foreach ($prefix in @($rom.Name,[IO.Path]::GetFileNameWithoutExtension($rom.Name)) | Select-Object -Unique) {
                foreach ($suffix in $companions) {
                    $name = $prefix + '.' + $suffix
                    if ($adjacent.ContainsKey($name) -and $name -ne $rom.Name) { $members[$name] = [pscustomobject]@{ File=$adjacent[$name]; Save=$false } }
                    if ($saves.ContainsKey($name)) { $members['saves/' + $name] = [pscustomobject]@{ File=$saves[$name]; Save=$true } }
                }
            }
            foreach ($member in $members.Values | Sort-Object { $_.File.FullName }) {
                $source = $parent + '/' + $(if ($member.Save) { 'saves/' } else { '' }) + $member.File.Name
                $destination = $destinationFolder + '/' + $(if ($member.Save) { 'saves/' } else { '' }) + $member.File.Name
                ConvertTo-GloamRelative $Root $destination | Out-Null
                if (Test-Path -LiteralPath (Get-GloamPath $Root $destination)) { $group.Reason = 'Destination exists; title and companions kept together.' }
                $move = [pscustomobject]@{ Source=$source; Destination=$destination; Stamp=(Get-GloamOrganizerStamp $Root $source) }
                $group.Moves.Add($move)
                foreach ($table in @($owners,$targets)) {
                    $key = if ([object]::ReferenceEquals($table,$owners)) { $source } else { $destination }
                    if (-not $table.ContainsKey($key)) { $table[$key] = [Collections.Generic.List[object]]::new() }
                    $table[$key].Add($group)
                }
            }
            $groups.Add($group)
        }
    }
    foreach ($table in @($owners,$targets)) {
        foreach ($key in $table.Keys) { if ($table[$key].Count -gt 1) { foreach ($group in $table[$key]) { $group.Reason = 'Shared companion or duplicate destination; resolve manually.' } } }
    }
    $moves = [Collections.Generic.List[object]]::new(); $skipped = [Collections.Generic.List[object]]::new(); $map = @{}
    foreach ($group in $groups) {
        if ($group.Reason) { $skipped.Add([pscustomobject]@{ Title=$group.Title; Reason=$group.Reason }); continue }
        foreach ($move in $group.Moves) { $moves.Add($move); $map['sd:/' + $move.Source] = 'sd:/' + $move.Destination }
    }
    if ($moves.Count -gt 100000) { throw 'Too many files for one transaction.' }
    $edits = @(); if ($moves.Count) { $edits = @(Get-GloamOrganizerEdits $Root $map) }
    return [pscustomobject]@{ Version=1; Folder=$folder; Mode=$Mode; Extensions=($suffixes -join ','); Moves=@($moves.ToArray()); Edits=$edits; Skipped=@($skipped.ToArray()) }
}

function Write-GloamOrganizerJson([string]$Path, $Value) {
    $temporary = $Path + '.tmp'
    [IO.File]::WriteAllText($temporary, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
    if (Test-Path -LiteralPath $Path) { [IO.File]::Replace($temporary,$Path,[NullString]::Value) }
    else { [IO.File]::Move($temporary,$Path) }
}

function Assert-GloamOrganizerJournal($Plan) {
    if ($Plan.Version -ne 1 -or $Plan.Mode -notin @('Alphabetize','Flatten') -or @($Plan.Moves).Count -eq 0 -or @($Plan.Moves).Count -gt 100000 -or @($Plan.Edits).Count -gt 20000) { throw 'Unsupported organizer journal.' }
    foreach ($move in $Plan.Moves) {
        foreach ($path in @($move.Source,$move.Destination)) {
            if (-not $path.StartsWith($Plan.Folder + '/', [StringComparison]::OrdinalIgnoreCase)) { throw 'Organizer journal leaves its system folder.' }
        }
    }
    foreach ($edit in $Plan.Edits) {
        foreach ($path in @($edit.Source,$edit.Destination)) {
            if ($path -cnotin @('_nds/dsi-frontend/state.ini','_nds/dsi-frontend/state.bak','_nds/dsi-frontend/recent.ini','_nds/dsi-frontend/recent.bak') -and
                $path -cnotmatch '^_nds/dsi-frontend/(items|games)/[0-9a-f]{16}(\.cheats)?\.(ini|bak)$' -and
                $path -cnotmatch '^_nds/dsi-frontend/plugins/(startup/)?[a-zA-Z0-9_-]{1,127}\.(ini|bak)$') { throw 'Unsupported organizer configuration path.' }
        }
        foreach ($text in @($edit.Before,$edit.After)) { if ([Convert]::FromBase64String($text).Length -gt 128KB) { throw 'Oversized organizer configuration.' } }
    }
}

function Undo-GloamOrganization([string]$Root, [string]$Journal) {
    Assert-GloamConfigurationReady $Root
    if ($Journal -notmatch '^[a-f0-9]{32}$') { throw 'Invalid organizer journal ID.' }
    $folder = Get-GloamPath $Root ('_nds/dsi-frontend/organizer/' + $Journal)
    $file = Join-Path $folder 'plan.json'
    if ((Get-Item -LiteralPath $file).Length -gt 64MB) { throw 'Oversized organizer journal.' }
    $plan = Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-GloamOrganizerJournal $plan
    Get-GloamOrganizerFolder $Root $plan.Folder | Out-Null
    # Reconcile original/target paths rather than trusting a completion flag:
    # this also handles process termination between a rename and journal update.
    foreach ($move in $plan.Moves) {
        $source = Get-GloamPath $Root $move.Source; $target = Get-GloamPath $Root $move.Destination
        $hasSource = Test-Path -LiteralPath $source -PathType Leaf; $hasTarget = Test-Path -LiteralPath $target -PathType Leaf
        if ($hasSource -eq $hasTarget) { throw 'Undo cannot identify an unchanged moved file. Nothing was overwritten.' }
        $current = if ($hasTarget) { $move.Destination } else { $move.Source }
        if ((Get-GloamOrganizerStamp $Root $current) -cne $move.Stamp) { throw 'A moved file changed. Preview a fresh flatten operation instead of undoing it.' }
    }
    foreach ($edit in $plan.Edits) {
        $source = Get-GloamPath $Root $edit.Source; $target = Get-GloamPath $Root $edit.Destination
        $value = if (Test-Path -LiteralPath $target -PathType Leaf) { [Convert]::ToBase64String([IO.File]::ReadAllBytes($target)) } else { '' }
        if ($edit.Source -eq $edit.Destination) {
            if ($value -cne $edit.Before -and $value -cne $edit.After) { throw 'Frontend settings changed after organization. Use a fresh flatten preview.' }
        } else {
            $old = if (Test-Path -LiteralPath $source -PathType Leaf) { [Convert]::ToBase64String([IO.File]::ReadAllBytes($source)) } else { '' }
            if ((-not $value -and -not $old) -or ($value -and $value -cne $edit.After) -or ($old -and $old -cne $edit.Before)) { throw 'Per-title records changed after organization.' }
        }
    }
    foreach ($edit in $plan.Edits) {
        $target = Get-GloamPath $Root $edit.Destination
        $current = if (Test-Path -LiteralPath (Get-GloamPath $Root $edit.Source) -PathType Leaf) { [Convert]::ToBase64String([IO.File]::ReadAllBytes((Get-GloamPath $Root $edit.Source))) } else { '' }
        if ($current -cne $edit.Before) { Write-GloamFile $Root $edit.Source ([Convert]::FromBase64String($edit.Before)) $true | Out-Null }
        if ($edit.Source -ne $edit.Destination -and (Test-Path -LiteralPath $target)) { Remove-Item -LiteralPath $target }
    }
    foreach ($move in @($plan.Moves)[(@($plan.Moves).Count - 1)..0]) {
        $target = Get-GloamPath $Root $move.Destination
        if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { continue }
        $source = Get-GloamPath $Root $move.Source
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($source)) | Out-Null
        [IO.File]::Move($target,$source)
    }
    Write-GloamOrganizerJson (Join-Path $folder 'status.json') @{ State='undone'; Folder=$plan.Folder; Mode=$plan.Mode; Time=[DateTime]::UtcNow.ToString('o') }
    Write-Host 'Organization undone. ROM names and file contents were preserved.'
}

function Invoke-GloamOrganization([string]$Root, [string]$PlanPath, [string]$ExpectedHash) {
    Assert-GloamConfigurationReady $Root
    if ((Get-Item -LiteralPath $PlanPath).Length -gt 64MB -or (Get-GloamHash $PlanPath) -cne $ExpectedHash) { throw 'The organization preview changed. Preview again.' }
    $plan = Get-Content -LiteralPath $PlanPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if (-not @($plan.Moves).Count) { throw 'There are no files to move.' }
    Assert-GloamOrganizerJournal $plan
    $fresh = New-GloamOrganizerPlan $Root $plan.Folder $plan.Mode $plan.Extensions
    if (($fresh | ConvertTo-Json -Depth 12 -Compress) -cne ($plan | ConvertTo-Json -Depth 12 -Compress)) { throw 'Files or configuration changed after preview. Preview again before moving anything.' }
    if (-not @($plan.Moves).Count) { throw 'There are no files to move.' }
    $journal = [Guid]::NewGuid().ToString('N')
    $folder = Get-GloamPath $Root ('_nds/dsi-frontend/organizer/' + $journal)
    [IO.Directory]::CreateDirectory($folder) | Out-Null
    Write-GloamOrganizerJson (Join-Path $folder 'plan.json') $plan
    Write-GloamOrganizerJson (Join-Path $folder 'status.json') @{ State='moving'; Folder=$plan.Folder; Mode=$plan.Mode; Time=[DateTime]::UtcNow.ToString('o') }
    Write-Host "Recovery journal: $journal"
    try {
        foreach ($move in $plan.Moves) {
            $source = Get-GloamPath $Root $move.Source; $target = Get-GloamPath $Root $move.Destination
            if ((Get-GloamOrganizerStamp $Root $move.Source) -cne $move.Stamp -or (Test-Path -LiteralPath $target)) { throw 'A move destination/source changed.' }
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
            [IO.File]::Move($source,$target)
            Write-Host "Moved: $($move.Source) -> $($move.Destination)"
        }
        foreach ($edit in $plan.Edits) {
            $source = Get-GloamPath $Root $edit.Source
            if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($source)) -cne $edit.Before) { throw 'Frontend metadata changed during organization.' }
            if (-not (Write-GloamFile $Root $edit.Destination ([Convert]::FromBase64String($edit.After)) ($edit.Source -eq $edit.Destination))) { throw 'A metadata destination appeared during organization.' }
            if ($edit.Source -ne $edit.Destination) { Remove-Item -LiteralPath $source }
        }
        Write-GloamOrganizerJson (Join-Path $folder 'status.json') @{ State='complete'; Folder=$plan.Folder; Mode=$plan.Mode; Time=[DateTime]::UtcNow.ToString('o') }
        Write-Host "Complete: $(@($plan.Moves).Count) file moves, $(@($plan.Edits).Count) frontend records updated."
    } catch {
        $failure = $_
        try { Undo-GloamOrganization $Root $journal }
        catch { Write-Warning "Recovery needed. Keep journal $journal; changed files were not overwritten." }
        throw $failure
    }
}
