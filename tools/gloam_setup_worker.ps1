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
    . (Join-Path $PSScriptRoot 'gloam-setup-search.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-configuration.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-organizer.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-artwork.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-identification.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-themes.ps1')
    $job = Get-Content -LiteralPath $Request -Raw -Encoding UTF8 | ConvertFrom-Json
    $script:GloamCancelFile = Join-Path ([IO.Path]::GetDirectoryName($Request)) 'cancel'
    $root = Get-GloamRoot $job.Target
    $directory = [string]$job.ReleaseDirectory
    if (-not $job.Offline -and $job.Action -in @('Install','UpdateRuntime','UpdateFrontend','InstallBundled','RepairAssets')) {
        $release = Get-GloamRelease (Join-Path ([IO.Path]::GetDirectoryName($Request)) 'lookup')
        if ($release.tag_name -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+(-(preview|rc)\.[0-9]+)?$') { throw 'Unexpected release version.' }
        $directory = Join-Path $env:LOCALAPPDATA ('Gloam/Setup/releases/' + $release.tag_name)
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        [IO.File]::WriteAllText((Join-Path $directory 'release.json'), ($release | ConvertTo-Json -Depth 30))
        Write-Host "Release: $($release.tag_name)"
    }
    switch ($job.Action) {
        'BuildSearchIndex' { Build-GloamSearchIndex $root ([IO.Path]::GetDirectoryName($Request)) }
        'ThemeCatalog' {
            $catalog = if ($job.Offline) { Read-GloamThemeCatalog $job.Options.Cache } else { Update-GloamThemeCatalog $job.Options.Cache }
            Write-Host ("Theme directory: " + @($catalog.Themes).Count + ' themes at ' + $catalog.Revision)
        }
        'ThemePreview' {
            $path = Get-GloamCatalogThemePreview $job.Options.Cache $job.Options.Id ([bool]$job.Offline)
            $result = @{ Id=$job.Options.Id; Path=$path }
            [IO.File]::WriteAllText((Join-Path ([IO.Path]::GetDirectoryName($Request)) 'theme-preview.json'), ($result | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
            Write-Host 'Preview cached on this PC. No SD files changed.'
        }
        'ThemeInstall' {
            Install-GloamCatalogTheme $root $job.Options.Cache $job.Options.Id $job.Options.InstallId ([bool]$job.Options.Replace) ([bool]$job.Options.AllowNoncommercial) ([bool]$job.Offline)
        }
        'ArtworkDatImport' {
            foreach ($file in $job.Sources) { $count=Import-GloamDat $job.Options.DatDirectory $job.Options.System $file; Write-Host ("Imported $count records from " + [IO.Path]::GetFileName($file)) }
        }
        'ArtworkDatUpdate' {
            if ($job.Offline) { throw 'DAT updates require an internet connection.' }
            $count=Update-GloamDat $job.Options.DatDirectory $job.Options.System
            Write-Host ("Updated $count identification records for " + $job.Options.System)
        }
        'ArtworkScan' {
            $rows = @(Get-GloamArtworkLibrary $root $job.Options.Folder ([bool]$job.Options.Recursive))
            [IO.File]::WriteAllText((Join-Path ([IO.Path]::GetDirectoryName($Request)) 'artwork-results.json'), (ConvertTo-Json -InputObject $rows -Depth 12), [Text.UTF8Encoding]::new($false))
            Write-Host ("Found " + $rows.Count + ' items. No SD files changed.')
        }
        { $_ -in @('ArtworkFetch','ArtworkApply') } {
            $cancelled = $false; $failed = $false; $rows = @($job.Options.Items)
            if ($job.Action -eq 'ArtworkFetch' -and ($rows.Count -gt 50 -or $job.Offline)) { throw 'Downloads require an online batch of at most 50 items.' }
            foreach ($row in $rows) {
                if ($job.Action -eq 'ArtworkFetch' -and (Test-Path -LiteralPath $script:GloamCancelFile)) { $cancelled=$true; break }
                if ($job.Action -eq 'ArtworkFetch') { $row.Candidate=''; $row.CandidateHash=''; $row.Source=''; $row | Add-Member -NotePropertyName Identification -NotePropertyValue '' -Force }
                try {
                    if ($job.Action -eq 'ArtworkFetch') {
                        if ($job.Options.Url) {
                            $url=$job.Options.Url; $bytes=Get-GloamArtworkDownload $url; $method='Direct image URL (review)'
                        } else {
                            $useDat=$job.Options.PSObject.Properties.Name -contains 'UseDat' -and $job.Options.UseDat
                            $cache=if ($job.Options.PSObject.Properties.Name -contains 'DatDirectory') { $job.Options.DatDirectory } else { Join-Path $env:LOCALAPPDATA 'Gloam/Setup/databases' }
                            $member=if ($row.PSObject.Properties.Name -contains 'ArchiveMember') { $row.ArchiveMember } else { '' }
                            $choice=if ($row.PSObject.Properties.Name -contains 'IdentityChoice') { $row.IdentityChoice } else { '' }
                            $match=Get-GloamIdentifiedCover $root $row.Content $job.Options.System $row.MatchName $job.Options.Region $useDat $cache $member $choice
                            $url=$match.Url; $bytes=$match.Bytes; $method=$match.Identification; $row.MatchName=$match.Name
                        }
                        $row | Add-Member -NotePropertyName Identification -NotePropertyValue $method -Force
                        $path = Join-Path ([IO.Path]::GetDirectoryName($Request)) ([Guid]::NewGuid().ToString('N') + '.png')
                        [IO.File]::WriteAllBytes($path,$bytes)
                        $row.Candidate=$path; $row.CandidateHash=Get-GloamHash $path; $row.Source=$url; $row.Result='Preview ready - '+$method
                    } else {
                        Set-GloamArtwork $root $row.Content $row.Candidate $row.CandidateHash $row.Source ([bool]$job.Options.Replace)
                        $state = Get-GloamArtworkState $root $row.Content
                        $row.Status=$state.Status; $row.Kind=$state.Kind; $row.Image=$state.Image; $row.Result='Applied'; $row.Selected=$false
                    }
                } catch {
                    if ($_.Exception.Data.Contains('Choices')) { $row | Add-Member -NotePropertyName IdentityChoices -NotePropertyValue @($_.Exception.Data['Choices']) -Force }
                    $row.Result=$_.Exception.Message
                    if ($_.Exception -is [OperationCanceledException]) { $cancelled=$true; break }
                    $failed=$true
                }
                finally { if ($job.Action -eq 'ArtworkFetch') { Start-Sleep -Milliseconds 500 } }
                Write-Host ($row.Content + ': ' + $row.Result)
            }
            [IO.File]::WriteAllText((Join-Path ([IO.Path]::GetDirectoryName($Request)) 'artwork-results.json'), (ConvertTo-Json -InputObject $rows -Depth 12), [Text.UTF8Encoding]::new($false))
            if ($cancelled) { Write-Host 'Cancelled. Completed previews were kept; no SD artwork changed.'; exit 3 }
            if ($failed) { exit 2 }
        }
        'PreviewOrganization' {
            $plan = New-GloamOrganizerPlan $root $job.Options.Folder $job.Options.Mode $job.Options.Extensions
            Write-GloamOrganizerJson (Join-Path ([IO.Path]::GetDirectoryName($Request)) 'organizer-plan.json') $plan
            Write-Host "Preview: $(@($plan.Moves).Count) file moves; $(@($plan.Skipped).Count) titles skipped. No SD files changed."
        }
        'ApplyOrganization' { Invoke-GloamOrganization $root $job.Options.Plan $job.Options.Hash }
        'UndoOrganization' { Undo-GloamOrganization $root $job.Options.Journal }
        'CheckSetup' { }
        'RepairAssets' { Repair-GloamSharedAssets $root (Get-GloamPackage $directory 'sd.zip' -Offline:$job.Offline) }
        'ImportLanguage' { Import-GloamLanguagePack $root ([string[]]$job.Sources) }
        'ExportConfiguration' { Export-GloamConfiguration $root $job.Options.Path }
        'ImportConfiguration' { Import-GloamConfiguration $root $job.Options.Path $job.Options.Hash }
        'CopyContent' { Copy-GloamContent $root ([string[]]$job.Sources) $job.ContentFolder }
        'Install' {
            Get-GloamPackage $directory 'licenses.zip' -Offline:$job.Offline | Out-Null
            Install-GloamArchive $root (Get-GloamPackage $directory 'sd.zip' -Offline:$job.Offline)
            if ($job.IncludeAddons) { Install-GloamArchive $root (Get-GloamPackage $directory 'addons.zip' -Offline:$job.Offline) }
        }
        'UpdateRuntime' {
            Get-GloamPackage $directory 'licenses.zip' -Offline:$job.Offline | Out-Null
            Install-GloamArchive $root (Get-GloamPackage $directory 'sd.zip' -Offline:$job.Offline) $true
        }
        'UpdateFrontend' {
            Get-GloamPackage $directory 'licenses.zip' -Offline:$job.Offline | Out-Null
            Write-GloamFile $root 'BOOT.NDS' ([IO.File]::ReadAllBytes((Get-GloamPackage $directory 'BOOT.NDS' -Offline:$job.Offline))) $true | Out-Null
            Write-Host 'Updated BOOT.NDS. Existing assets, settings and saves were kept.'
        }
        'InstallBundled' { Get-GloamPackage $directory 'licenses.zip' -Offline:$job.Offline | Out-Null; Install-GloamArchive $root (Get-GloamPackage $directory 'addons.zip' -Offline:$job.Offline) }
        { $_ -in @('InstallAddons','UpdateAddons','CheckAddons','CheckSupport') } {
            if (-not @($job.Selected).Count -and $job.Action -ne 'CheckSupport') { throw 'Select at least one application.' }
            $arguments = @{ Target = $root; Manifest = $job.Manifest; Cache = $job.Cache }
            if (@($job.Selected).Count) { $arguments.Only = [string[]]$job.Selected }
            if ($job.Noncommercial) { $arguments.Noncommercial = $true }
            if ($job.Offline) { $arguments.Offline = $true }
            switch ($job.Action) {
                # Installation refreshes the reviewed catalog in the downloader,
                # just like updates; selected IDs also cover bundled applications.
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
    if ($job.Action -in @('CheckSetup','RepairAssets','ImportLanguage','Install','UpdateRuntime','UpdateFrontend')) {
        $rows=@(Get-GloamSetupHealth $root)
        [IO.File]::WriteAllText((Join-Path ([IO.Path]::GetDirectoryName($Request)) 'setup-health.json'), (ConvertTo-Json -InputObject $rows -Depth 8), [Text.UTF8Encoding]::new($false))
        foreach ($row in $rows) { Write-Host ("$($row.Status): $($row.Item) - $($row.Details)") }
    }
    Write-Host 'Done.'
    exit 0
} catch {
    if ($_.Exception -is [OperationCanceledException]) { Write-Host $_.Exception.Message; exit 3 }
    [Console]::Error.WriteLine('Operation failed: ' + $_.Exception.Message)
    exit 1
}
