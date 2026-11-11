# SPDX-License-Identifier: GPL-3.0-or-later
# A separate process keeps downloads and SD operations off the UI thread.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Request)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new()
try {
    . (Join-Path $PSScriptRoot 'gloam-setup-core.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-settings.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-configuration.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-organizer.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-artwork.ps1')
    $job = Get-Content -LiteralPath $Request -Raw -Encoding UTF8 | ConvertFrom-Json
    $root = Get-GloamRoot $job.Target
    $directory = [string]$job.ReleaseDirectory
    if (-not $job.Offline -and $job.Action -in @('Install','UpdateRuntime','UpdateFrontend','InstallBundled')) {
        $release = Get-GloamRelease (Join-Path ([IO.Path]::GetDirectoryName($Request)) 'lookup')
        if ($release.tag_name -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+(-(preview|rc)\.[0-9]+)?$') { throw 'Unexpected release version.' }
        $directory = Join-Path $env:LOCALAPPDATA ('Gloam/Setup/releases/' + $release.tag_name)
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        [IO.File]::WriteAllText((Join-Path $directory 'release.json'), ($release | ConvertTo-Json -Depth 30))
        Write-Host "Release: $($release.tag_name)"
    }
    switch ($job.Action) {
        'ArtworkScan' {
            $rows = @(Get-GloamArtworkLibrary $root $job.Options.Folder ([bool]$job.Options.Recursive))
            [IO.File]::WriteAllText((Join-Path ([IO.Path]::GetDirectoryName($Request)) 'artwork-results.json'), (ConvertTo-Json -InputObject $rows -Depth 12), [Text.UTF8Encoding]::new($false))
            Write-Host ("Found " + $rows.Count + ' items. No SD files changed.')
        }
        { $_ -in @('ArtworkFetch','ArtworkApply') } {
            $failed = $false; $rows = @($job.Options.Items)
            if ($job.Action -eq 'ArtworkFetch' -and ($rows.Count -gt 50 -or $job.Offline)) { throw 'Downloads require an online batch of at most 50 items.' }
            foreach ($row in $rows) {
                if ($job.Action -eq 'ArtworkFetch') { $row.Candidate=''; $row.CandidateHash=''; $row.Source='' }
                try {
                    if ($job.Action -eq 'ArtworkFetch') {
                        $url = if ($job.Options.Url) { $job.Options.Url } else { Get-GloamArtworkUrl $root $row.Content $job.Options.System $row.MatchName $job.Options.Region }
                        $bytes = Get-GloamArtworkDownload $url
                        $path = Join-Path ([IO.Path]::GetDirectoryName($Request)) ([Guid]::NewGuid().ToString('N') + '.png')
                        [IO.File]::WriteAllBytes($path,$bytes)
                        $row.Candidate=$path; $row.CandidateHash=Get-GloamHash $path; $row.Source=$url; $row.Result='Preview ready - review before applying'
                    } else {
                        Set-GloamArtwork $root $row.Content $row.Candidate $row.CandidateHash $row.Source ([bool]$job.Options.Replace)
                        $state = Get-GloamArtworkState $root $row.Content
                        $row.Status=$state.Status; $row.Kind=$state.Kind; $row.Image=$state.Image; $row.Result='Applied'; $row.Selected=$false
                    }
                } catch { $row.Result=$_.Exception.Message; $failed=$true }
                finally { if ($job.Action -eq 'ArtworkFetch') { Start-Sleep -Milliseconds 500 } }
                Write-Host ($row.Content + ': ' + $row.Result)
            }
            [IO.File]::WriteAllText((Join-Path ([IO.Path]::GetDirectoryName($Request)) 'artwork-results.json'), (ConvertTo-Json -InputObject $rows -Depth 12), [Text.UTF8Encoding]::new($false))
            if ($failed) { exit 2 }
        }
        'PreviewOrganization' {
            $plan = New-GloamOrganizerPlan $root $job.Options.Folder $job.Options.Mode $job.Options.Extensions
            Write-GloamOrganizerJson (Join-Path ([IO.Path]::GetDirectoryName($Request)) 'organizer-plan.json') $plan
            Write-Host "Preview: $(@($plan.Moves).Count) file moves; $(@($plan.Skipped).Count) titles skipped. No SD files changed."
        }
        'ApplyOrganization' { Invoke-GloamOrganization $root $job.Options.Plan $job.Options.Hash }
        'UndoOrganization' { Undo-GloamOrganization $root $job.Options.Journal }
        'CheckSetup' { Test-GloamSetup $root }
        'ExportConfiguration' { Export-GloamConfiguration $root $job.Options.Path }
        'ImportConfiguration' { Import-GloamConfiguration $root $job.Options.Path $job.Options.Hash }
        'CopyContent' { Copy-GloamContent $root ([string[]]$job.Sources) $job.ContentFolder }
        'Install' {
            Get-GloamPackage $directory 'licenses.zip' | Out-Null
            Install-GloamArchive $root (Get-GloamPackage $directory 'sd.zip')
            if ($job.IncludeAddons) { Install-GloamArchive $root (Get-GloamPackage $directory 'addons.zip') }
        }
        'UpdateRuntime' {
            Get-GloamPackage $directory 'licenses.zip' | Out-Null
            Install-GloamArchive $root (Get-GloamPackage $directory 'sd.zip') $true
        }
        'UpdateFrontend' {
            Get-GloamPackage $directory 'licenses.zip' | Out-Null
            Write-GloamFile $root 'BOOT.NDS' ([IO.File]::ReadAllBytes((Get-GloamPackage $directory 'BOOT.NDS'))) $true | Out-Null
            Write-Host 'Updated BOOT.NDS. Existing assets, settings and saves were kept.'
        }
        'InstallBundled' { Get-GloamPackage $directory 'licenses.zip' | Out-Null; Install-GloamArchive $root (Get-GloamPackage $directory 'addons.zip') }
        { $_ -in @('InstallAddons','UpdateAddons','CheckAddons','CheckSupport') } {
            if (-not @($job.Selected).Count -and $job.Action -ne 'CheckSupport') { throw 'Select at least one application.' }
            $arguments = @{ Target = $root; Manifest = $job.Manifest; Cache = $job.Cache }
            if (@($job.Selected).Count) { $arguments.Only = [string[]]$job.Selected }
            if ($job.Noncommercial) { $arguments.Noncommercial = $true }
            if ($job.Offline) { $arguments.Offline = $true }
            switch ($job.Action) {
                'InstallAddons' {
                    # The GUI lists the full catalog, including apps bundled in addons.zip.
                    # Freeze those packaged pins just as the standalone install menu does.
                    $catalog = Get-Content -LiteralPath $job.Manifest -Raw -Encoding UTF8 | ConvertFrom-Json
                    if ($catalog.format -ne 1) { throw 'Unsupported download manifest' }
                    $apps = if ($catalog.PSObject.Properties.Name -contains 'updates') { @($catalog.updates) } else { @($catalog.applications) }
                    $snapshot = @{ format=1; applications=$apps; updates=$apps; manual=@() }
                    $path = Join-Path ([IO.Path]::GetDirectoryName($Request)) 'install-catalog.json'
                    [IO.File]::WriteAllText($path, ($snapshot | ConvertTo-Json -Depth 30), [Text.UTF8Encoding]::new($true))
                    $arguments.Manifest = $path
                }
                'UpdateAddons' { $arguments.Update = $true }
                'CheckAddons' { $arguments.CheckUpdates = $true }
                'CheckSupport' { $arguments.CheckSupport = $true }
            }
            $global:LASTEXITCODE = 0
            & (Join-Path $PSScriptRoot 'download-addons.ps1') @arguments
            $succeeded = $?
            if ($LASTEXITCODE -eq 2) { exit 2 }
            if (-not $succeeded) { throw 'The add-on operation failed.' }
        }
        default { throw 'Unsupported setup action.' }
    }
    Write-Host 'Done.'
    exit 0
} catch {
    [Console]::Error.WriteLine('Operation failed: ' + $_.Exception.Message)
    exit 1
}
