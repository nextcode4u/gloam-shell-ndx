# SPDX-License-Identifier: GPL-3.0-or-later
$script:OrganizerPlan = ''; $script:OrganizerPlanHash = ''; $script:OrganizerRoot = ''
$ui.OrganizerMode.ItemsSource = @('A-Z / # folders','Back to system folder'); $ui.OrganizerMode.SelectedIndex = 0
$ui.OrganizerExtensions.Text = $GloamRomExtensions

function Reset-GloamOrganizerPreview {
    $script:OrganizerPlan = ''; $script:OrganizerPlanHash = ''
    $ui.ApplyOrganization.IsEnabled = $false; $ui.OrganizerPreview.ItemsSource = @()
    $ui.OrganizerSummary.Text = 'Preview first. No files are changed until you apply.'
}
function Load-GloamOrganizerFolders {
    $previous = if ($script:OrganizerRoot -eq $script:Root) { $ui.OrganizerFolder.Text } else { '' }
    $script:OrganizerRoot = $script:Root
    $folders = @(Get-GloamOrganizerFolders $script:Root)
    if ($previous -and $previous -notin $folders) {
        try { $previous = Get-GloamOrganizerFolder $script:Root $previous; $folders += $previous }
        catch { $previous = '' }
    }
    $ui.OrganizerFolder.ItemsSource = $folders
    if ($previous -in $folders) { $ui.OrganizerFolder.SelectedItem = $previous }
    elseif ($folders.Count) { $ui.OrganizerFolder.SelectedIndex = 0 }
    else { $ui.OrganizerFolder.Text = ''; $ui.OrganizerSummary.Text = 'No system folders found under roms or the configured Games library. Install the SD layout or add your ROM folders first.' }
}
function Load-GloamOrganizerJournals {
    $items = @(); $base = '_nds/dsi-frontend/organizer'
    $path = Get-GloamPath $script:Root $base
    if (Test-Path -LiteralPath $path -PathType Container) {
        foreach ($folder in Get-ChildItem -LiteralPath $path -Directory | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 20) {
            if ($folder.Name -notmatch '^[a-f0-9]{32}$') { continue }
            $file = Get-GloamPath $script:Root ($base + '/' + $folder.Name + '/status.json')
            if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -gt 4096) { continue }
            try {
                $state = Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json
                if ($state.State -eq 'undone') { continue }
                $items += [pscustomobject]@{ Id=$folder.Name; Label=$state.Folder + ' - ' + $state.Mode + ' (' + $state.State + ')' }
            } catch { Write-Activity ('Invalid organizer journal: ' + $folder.Name) }
        }
    }
    $ui.OrganizerJournals.ItemsSource = $items
    if ($items.Count) { $ui.OrganizerJournals.SelectedIndex = 0 }
}
function Complete-GloamOrganizerJob($Job) {
    if ($Job.Action -ne 'PreviewOrganization') { return }
    $path = Join-Path $Job.Folder 'organizer-plan.json'
    $plan = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($move in $plan.Moves) { $rows.Add([pscustomobject]@{ Action='Move'; Source=$move.Source; Destination=$move.Destination }) }
    foreach ($edit in $plan.Edits) { $rows.Add([pscustomobject]@{ Action='Relink'; Source=$edit.Source; Destination=$edit.Destination }) }
    foreach ($skip in $plan.Skipped) { $rows.Add([pscustomobject]@{ Action='Skip'; Source=$skip.Title; Destination=$skip.Reason }) }
    $ui.OrganizerPreview.ItemsSource = $rows.ToArray()
    $ui.OrganizerSummary.Text = "$(@($plan.Moves).Count) files, $(@($plan.Edits).Count) frontend records, $(@($plan.Skipped).Count) skipped titles. Review before applying."
    $script:OrganizerPlan = $path; $script:OrganizerPlanHash = Get-GloamHash $path
    $ui.ApplyOrganization.IsEnabled = @($plan.Moves).Count -gt 0
}
$ui.OrganizerExtensions.Add_TextChanged({ Reset-GloamOrganizerPreview })
$ui.OrganizerFolder.AddHandler([Windows.Controls.Primitives.TextBoxBase]::TextChangedEvent, [Windows.Controls.TextChangedEventHandler]{ Reset-GloamOrganizerPreview })
$ui.OrganizerMode.Add_SelectionChanged({ Reset-GloamOrganizerPreview })
$ui.OpenOrganizerRoms.Add_Click({ Invoke-UiAction { Open-SdFolder 'roms' } })
$ui.PickOrganizerFolder.Add_Click({ Invoke-UiAction {
    $root = Get-SelectedRoot; $dialog = [Windows.Forms.FolderBrowserDialog]::new(); $dialog.SelectedPath = if ($ui.OrganizerFolder.Text) { Get-GloamPath $root $ui.OrganizerFolder.Text } else { Get-GloamPath $root 'roms' }; $dialog.Description = 'Choose one system ROM folder on the SD card'
    if ($dialog.ShowDialog() -eq 'OK') { $ui.OrganizerFolder.Text = Get-GloamOrganizerFolder $root $dialog.SelectedPath }; $dialog.Dispose()
} })
$ui.PreviewOrganization.Add_Click({ Invoke-UiAction {
    $mode = if ($ui.OrganizerMode.SelectedIndex -eq 0) { 'Alphabetize' } else { 'Flatten' }
    Start-SetupJob 'PreviewOrganization' @() @{ Folder=$ui.OrganizerFolder.Text; Mode=$mode; Extensions=$ui.OrganizerExtensions.Text }
} })
$ui.ApplyOrganization.Add_Click({ Invoke-UiAction {
    Get-SelectedRoot | Out-Null
    if ($script:SettingsDirty) { throw 'Save or reload pending launcher settings before organizing ROMs.' }
    if (-not $script:OrganizerPlan) { throw 'Preview the moves first.' }
    $message = 'Apply the reviewed file moves and Gloam reference updates? Close the launcher/emulator first. A recovery journal is kept; existing destinations are never overwritten.'
    if ([Windows.MessageBox]::Show($window,$message,'Organize ROM folder','YesNo','Question') -eq 'Yes') {
        Start-SetupJob 'ApplyOrganization' @() @{ Plan=$script:OrganizerPlan; Hash=$script:OrganizerPlanHash }
    }
} })
$ui.UndoOrganization.Add_Click({ Invoke-UiAction {
    Get-SelectedRoot | Out-Null
    if ($script:SettingsDirty) { throw 'Save or reload pending settings first.' }
    if (-not $ui.OrganizerJournals.SelectedValue) { throw 'Choose an organization journal first.' }
    if ([Windows.MessageBox]::Show($window,'Undo this operation only if its files and frontend records are unchanged?','Undo organization','YesNo','Question') -eq 'Yes') {
        Start-SetupJob 'UndoOrganization' @() @{ Journal=[string]$ui.OrganizerJournals.SelectedValue }
    }
} })
