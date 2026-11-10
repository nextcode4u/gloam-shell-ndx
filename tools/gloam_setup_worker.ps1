# SPDX-License-Identifier: GPL-3.0-or-later
# A separate process keeps downloads and SD operations off the UI thread.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Request)
$ErrorActionPreference = 'Stop'
try {
    . (Join-Path $PSScriptRoot 'gloam-setup-core.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-settings.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-configuration.ps1')
    . (Join-Path $PSScriptRoot 'gloam-setup-organizer.ps1')
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
                'UpdateAddons' { $arguments.Update = $true }
                'CheckAddons' { $arguments.CheckUpdates = $true }
                'CheckSupport' { $arguments.CheckSupport = $true }
            }
            & (Join-Path $PSScriptRoot 'download-addons.ps1') @arguments
            if (-not $?) { throw 'The add-on operation failed.' }
        }
        default { throw 'Unsupported setup action.' }
    }
    Write-Host 'Done.'
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
