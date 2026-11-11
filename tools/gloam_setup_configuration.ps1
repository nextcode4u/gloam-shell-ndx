# SPDX-License-Identifier: GPL-3.0-or-later
# The same DSCFG001 record layout and whitelist used by the launcher and Python tool.
function Test-GloamConfigurationName([string]$Name) {
    if ($Name -cin @('state.ini','tabs.ini','dashboard.ini','recent.ini', 'language.ini', 'collections.ini','appearance-1.ini','appearance-2.ini','appearance-3.ini')) { return $true }
    return $Name -cmatch '^(games|items)/[0-9a-f]{16}(\.cheats)?\.ini$' -or $Name -cmatch '^plugins/user/[a-z0-9_-]{1,15}\.ini$'
}

function Get-GloamCrc32([byte[]]$Data) {
    if (-not ('GloamConfigurationCrc32' -as [type])) {
        Add-Type -TypeDefinition @'
public static class GloamConfigurationCrc32 {
    private static readonly uint[] Table = BuildTable();
    private static uint[] BuildTable() {
        uint[] table = new uint[256];
        for (uint n = 0; n < 256; n++) {
            uint value = n;
            for (int bit = 0; bit < 8; bit++)
                value = (value & 1) != 0 ? 0xedb88320U ^ (value >> 1) : value >> 1;
            table[n] = value;
        }
        return table;
    }
    public static uint Compute(byte[] data) {
        if (data == null) return 0;
        uint value = 0xffffffffU;
        foreach (byte b in data) value = Table[(value ^ b) & 255] ^ (value >> 8);
        return ~value;
    }
}
'@
    }
    return [GloamConfigurationCrc32]::Compute($Data)
}

function Read-GloamConfigurationArchive([string]$Path) {
    if ((Get-Item -LiteralPath $Path).Length -gt (4MB + 1024 * 108)) { throw 'Configuration archive exceeds its size limit.' }
    $stream = [IO.File]::OpenRead($Path); $reader = [IO.BinaryReader]::new($stream)
    try {
        if ([Text.Encoding]::ASCII.GetString($reader.ReadBytes(8)) -cne 'DSCFG001') { throw 'Choose a native .dsconfig archive.' }
        $count = $reader.ReadUInt32()
        if ($count -gt 1024) { throw 'Too many configuration records.' }
        $records = [ordered]@{}; $total = 0L
        for ($i = 0; $i -lt $count; $i++) {
            $length = $reader.ReadUInt16(); $flags = $reader.ReadUInt16()
            $size = $reader.ReadUInt32(); $crc = $reader.ReadUInt32()
            if ($flags -ne 0 -or $length -eq 0 -or $length -ge 96 -or $size -gt 65536) { throw 'Invalid configuration record bounds.' }
            $nameBytes = $reader.ReadBytes($length)
            if ($nameBytes.Length -ne $length -or @($nameBytes | Where-Object { $_ -gt 127 }).Count) { throw 'Invalid configuration path encoding.' }
            $name = [Text.Encoding]::ASCII.GetString($nameBytes)
            if (-not (Test-GloamConfigurationName $name) -or $records.Contains($name)) { throw 'Unsupported or duplicate configuration path.' }
            $data = $reader.ReadBytes([int]$size); $total += $size
            if ($data.Length -ne $size -or $data -contains 0 -or $total -gt 4MB -or (Get-GloamCrc32 $data) -ne $crc) { throw 'Truncated, corrupt or oversized configuration record.' }
            $records[$name] = $data
        }
        if ($stream.Position -ne $stream.Length) { throw 'Unexpected trailing configuration data.' }
        return $records
    } finally { $reader.Dispose(); $stream.Dispose() }
}

function Export-GloamConfiguration([string]$Root, [string]$Path) {
    $root = Get-GloamRoot $Root
    if ([IO.Path]::GetExtension($Path) -ne '.dsconfig') { throw 'Save configuration as .dsconfig.' }
    $base = '_nds/dsi-frontend/'
    $names = @('state.ini','tabs.ini','dashboard.ini','recent.ini', 'language.ini', 'collections.ini','appearance-1.ini','appearance-2.ini','appearance-3.ini')
    foreach ($folder in @('games','items','plugins/user')) {
        $directory = Get-GloamPath $root ($base + $folder)
        if (Test-Path -LiteralPath $directory -PathType Container) {
            foreach ($file in Get-ChildItem -LiteralPath $directory -Filter '*.ini' -File) {
                $name = $folder + '/' + $file.Name
                if (Test-GloamConfigurationName $name) { $names += $name }
            }
        }
    }
    $records = [ordered]@{}; $total = 0L
    foreach ($name in $names | Sort-Object -Unique) {
        $source = Get-GloamPath $root ($base + $name)
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { continue }
        if ((Get-Item -LiteralPath $source).Length -gt 65536) { throw 'A configuration record exceeds 64 KiB.' }
        $data = [IO.File]::ReadAllBytes($source); $total += $data.Length
        if ($data -contains 0 -or $total -gt 4MB -or $records.Count -ge 1024) { throw 'Configuration exceeds its portable limits.' }
        $records[$name] = $data
    }
    $bytes = ConvertTo-GloamConfigurationBytes $records
    $temporary = $Path + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllBytes($temporary, $bytes)
        if (Test-Path -LiteralPath $Path) { [IO.File]::Replace($temporary, $Path, $Path + '.bak') }
        else { [IO.File]::Move($temporary, $Path) }
    } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
    Write-Host "Exported $($records.Count) configuration records to $Path"
}

function Import-GloamConfiguration([string]$Root, [string]$Path, [string]$ExpectedHash) {
    $root = Get-GloamRoot $Root
    Assert-GloamConfigurationReady $root
    if ((Get-GloamHash $Path) -cne $ExpectedHash) { throw 'Configuration archive changed after its preview. Select it again.' }
    $records = Read-GloamConfigurationArchive $Path
    $plan = @()
    foreach ($name in $records.Keys) {
        $relative = '_nds/dsi-frontend/' + $name; $target = Get-GloamPath $root $relative
        if (Test-Path -LiteralPath $target -PathType Container) { throw 'A configuration destination is a folder.' }
        $exists = Test-Path -LiteralPath $target -PathType Leaf
        if ($exists -and (Get-Item -LiteralPath $target).Length -gt 65536) { throw 'Existing configuration is too large to preserve safely.' }
        $previous = if ($exists) { ,[IO.File]::ReadAllBytes($target) } else { [byte[]]@() }
        $plan += [pscustomobject]@{ Relative = $relative; Target = $target; Exists = $exists; Previous = $previous; Data = $records[$name]; Applied = $false }
    }
    # Use the same recovery journal as the native importer. A terminated PC
    # import can be rolled back by Gloam before it loads preferences.
    $previousRecords = [ordered]@{}
    foreach ($item in $plan) {
        if ($item.Exists) { $previousRecords[$item.Relative.Substring('_nds/dsi-frontend/'.Length)] = $item.Previous }
    }
    $rollback = '_nds/dsi-frontend/configuration/rollback.dsconfig'
    $pending = '_nds/dsi-frontend/configuration/pending.dsconfig'
    Write-GloamFile $root $rollback (ConvertTo-GloamConfigurationBytes $previousRecords) $true | Out-Null
    if (-not (Write-GloamFile $root $pending (ConvertTo-GloamConfigurationBytes $records))) { throw 'Another configuration import has started.' }
    # All records, CRCs and destinations are checked before replacing anything.
    try {
        foreach ($item in $plan) {
            $exists = Test-Path -LiteralPath $item.Target -PathType Leaf
            if ($exists -ne $item.Exists) { throw 'Configuration destination changed during import.' }
            if ($exists) {
                if ((Get-Item -LiteralPath $item.Target).Length -gt 128KB -or
                    [Convert]::ToBase64String([IO.File]::ReadAllBytes($item.Target)) -cne [Convert]::ToBase64String($item.Previous)) { throw 'Configuration was edited during import.' }
            }
            Write-GloamFile $root $item.Relative $item.Data $true | Out-Null
            $item.Applied = $true
        }
        Remove-Item -LiteralPath (Get-GloamPath $root $pending)
    } catch {
        $failure = $_
        $restored = $true
        foreach ($item in @($plan | Where-Object Applied)) {
            try {
                if (-not (Test-Path -LiteralPath $item.Target -PathType Leaf) -or
                    (Get-Item -LiteralPath $item.Target).Length -gt 65536 -or
                    [Convert]::ToBase64String([IO.File]::ReadAllBytes($item.Target)) -cne [Convert]::ToBase64String($item.Data)) { throw 'External edit preserved during rollback.' }
                if ($item.Exists) { Write-GloamFile $root $item.Relative $item.Previous $true | Out-Null }
                else { Remove-Item -LiteralPath $item.Target }
            } catch { $restored = $false; Write-Warning "Restore from setup-backups if needed: $($item.Relative)" }
        }
        if ($restored) { Remove-Item -LiteralPath (Get-GloamPath $root $pending) }
        throw $failure
    }
    Write-Host "Imported $($records.Count) configuration records. Restart Gloam to reload them."
}

function ConvertTo-GloamConfigurationBytes($Records) {
    if ($Records.Count -gt 1024) { throw 'Too many configuration records.' }
    $memory = [IO.MemoryStream]::new(); $writer = [IO.BinaryWriter]::new($memory)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('DSCFG001')); $writer.Write([uint32]$Records.Count)
        $total = 0L
        foreach ($name in $Records.Keys) {
            [byte[]]$data = $Records[$name]
            if ($null -eq $data) { $data = [byte[]]::new(0) }
            $encoded = [Text.Encoding]::ASCII.GetBytes($name)
            $total += $data.Length
            if (-not (Test-GloamConfigurationName $name) -or $encoded.Length -ge 96 -or
                $data.Length -gt 65536 -or $total -gt 4MB -or $data -contains 0) { throw 'Unsupported configuration record.' }
            $writer.Write([uint16]$encoded.Length); $writer.Write([uint16]0); $writer.Write([uint32]$data.Length)
            $writer.Write([uint32](Get-GloamCrc32 $data)); $writer.Write($encoded); $writer.Write($data)
        }
        $writer.Flush()
        return ,$memory.ToArray()
    } finally { $writer.Dispose(); $memory.Dispose() }
}
