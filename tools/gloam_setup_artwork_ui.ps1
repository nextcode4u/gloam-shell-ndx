# SPDX-License-Identifier: GPL-3.0-or-later
$script:ArtworkRoot = ''
$script:ArtworkSelectionLoading = $false
$ui.ArtworkSystem.ItemsSource = @($GloamArtworkSystems.Keys)
$ui.ArtworkSystem.SelectedIndex = 0
$ui.ArtworkRegion.ItemsSource = @('US','EN','JA','FR','DE','ES','IT','KO')
$ui.ArtworkRegion.SelectedIndex = 0

function Load-GloamArtworkUi {
    if ($script:ArtworkRoot -ne $script:Root) {
        $script:ArtworkRoot = $script:Root
        $ui.ArtworkList.ItemsSource = @()
        $ui.ArtworkFolder.Text = 'roms'
        $ui.ArtworkSummary.Text = 'Choose a collection and scan. Downloaded/imported images are previews until you apply them.'
        $ui.ArtworkCurrent.Source = $null; $ui.ArtworkCandidate.Source = $null
    }
}
function Show-GloamArtworkImage($Control, [string]$Path) {
    $Control.Source = $null
    if (-not $Path) { return }
    if ([IO.Path]::GetExtension($Path) -ne '.mp3' -and (Get-Item -LiteralPath $Path).Length -gt 8MB) { throw 'Image exceeds preview limit.' }
    $raw = if ([IO.Path]::GetExtension($Path) -eq '.mp3') { Get-GloamEmbeddedArtwork $Path } else { [IO.File]::ReadAllBytes($Path) }
    $bytes = ConvertTo-GloamArtworkPng $raw
    $stream = [IO.MemoryStream]::new($bytes,$false)
    try {
        $image = [Windows.Media.Imaging.BitmapImage]::new(); $image.BeginInit()
        $image.CacheOption = [Windows.Media.Imaging.BitmapCacheOption]::OnLoad
        $image.StreamSource = $stream; $image.EndInit(); $image.Freeze(); $Control.Source = $image
    } finally { $stream.Dispose() }
}
function Show-GloamArtworkSelection {
    $item = $ui.ArtworkList.SelectedItem
    $ui.ArtworkCurrent.Source = $null; $ui.ArtworkCandidate.Source = $null
    if (-not $item) { return }
    $script:ArtworkSelectionLoading = $true
    $ui.ArtworkArchiveMember.ItemsSource=@('')
    if ([IO.Path]::GetExtension($item.Content) -eq '.zip') {
        try { Initialize-GloamDat; $ui.ArtworkArchiveMember.ItemsSource=@('')+@([GloamDat]::ArchiveMembers((Get-GloamPath (Get-SelectedRoot) $item.Content))) }
        catch { Write-Activity $_.Exception.Message }
    }
    $ui.ArtworkArchiveMember.SelectedItem=$item.ArchiveMember
    $ui.ArtworkIdentityChoice.ItemsSource=@('')+@($item.IdentityChoices)
    $ui.ArtworkIdentityChoice.SelectedItem=$item.IdentityChoice
    $script:ArtworkSelectionLoading = $false
    $ui.ArtworkMatch.Text = $item.MatchName
    $ui.ArtworkInfo.Text = $item.Content + "`n" + $item.Kind + "`n" + $item.Identification + "`n" + $item.Result + "`n" + $item.Source
    try { Show-GloamArtworkImage $ui.ArtworkCurrent $item.Image; Show-GloamArtworkImage $ui.ArtworkCandidate $item.Candidate }
    catch { $ui.ArtworkInfo.Text += "`nPreview unavailable: " + $_.Exception.Message }
}
function Complete-GloamArtworkJob($Job) {
    if ($Job.Action -notlike 'Artwork*') { return }
    if ($Job.Action -in @('ArtworkDatImport','ArtworkDatUpdate')) { $ui.ArtworkSummary.Text='DAT metadata is ready on this PC. Download previews to identify checked files; see the activity log for imported record counts.'; return }
    $file = Join-Path $Job.Folder 'artwork-results.json'
    if (-not (Test-Path -LiteralPath $file)) { return }
    $results = @(Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json | ForEach-Object { $_ })
    if ($Job.Action -eq 'ArtworkScan') { $ui.ArtworkList.ItemsSource = $results }
    else {
        foreach ($result in $results) {
            $item = @($ui.ArtworkList.ItemsSource | Where-Object Content -CEQ $result.Content)
            if ($item.Count -ne 1) { continue }
            foreach ($field in @('Status','Kind','Image','Candidate','CandidateHash','Source','Result','Selected','Identification','MatchName','IdentityChoices')) { if ($result.PSObject.Properties.Name -contains $field) { $item[0] | Add-Member -NotePropertyName $field -NotePropertyValue $result.$field -Force } }
        }
        $ui.ArtworkList.Items.Refresh()
    }
    $ui.ArtworkSummary.Text = 'Items: ' + @($ui.ArtworkList.ItemsSource).Count + '. Select a row to preview. Check boxes choose items for download/apply; existing art requires replacement enabled.'
    if ($ui.ArtworkList.SelectedIndex -lt 0 -and @($ui.ArtworkList.ItemsSource).Count) { $ui.ArtworkList.SelectedIndex = 0 }
    Show-GloamArtworkSelection
}
function Clear-GloamArtworkCandidate {
    $item=$ui.ArtworkList.SelectedItem
    $item.Candidate=''; $item.CandidateHash=''; $item.Source=''; $item.Identification=''
    $item.Result='Choice changed; download a new preview before applying.'
    $ui.ArtworkCandidate.Source=$null; $ui.ArtworkList.Items.Refresh()
}
$ui.ArtworkArchiveMember.Add_SelectionChanged({
    if (-not $script:ArtworkSelectionLoading -and $ui.ArtworkList.SelectedItem -and $null -ne $ui.ArtworkArchiveMember.SelectedItem) {
        $ui.ArtworkList.SelectedItem.ArchiveMember=[string]$ui.ArtworkArchiveMember.SelectedItem
        $ui.ArtworkList.SelectedItem.IdentityChoice=''; $ui.ArtworkList.SelectedItem.IdentityChoices=@()
        $ui.ArtworkIdentityChoice.ItemsSource=@(''); Clear-GloamArtworkCandidate
    }
})
$ui.ArtworkIdentityChoice.Add_SelectionChanged({
    if (-not $script:ArtworkSelectionLoading -and $ui.ArtworkList.SelectedItem -and $null -ne $ui.ArtworkIdentityChoice.SelectedItem) { $ui.ArtworkList.SelectedItem.IdentityChoice=[string]$ui.ArtworkIdentityChoice.SelectedItem; Clear-GloamArtworkCandidate }
})
$ui.ArtworkList.Add_SelectionChanged({ Show-GloamArtworkSelection })
$ui.PickArtworkFolder.Add_Click({ Invoke-UiAction {
    $root = Get-SelectedRoot; $dialog = [Windows.Forms.FolderBrowserDialog]::new()
    try {
        $dialog.SelectedPath = $root; $dialog.Description = 'Choose a game, music, book or other collection on the SD card'
        if ($dialog.ShowDialog() -eq 'OK') { $ui.ArtworkFolder.Text = ConvertTo-GloamRelative $root $dialog.SelectedPath }
    } finally { $dialog.Dispose() }
} })
$ui.ArtworkScan.Add_Click({ Invoke-UiAction {
    Start-SetupJob 'ArtworkScan' @() @{ Folder=$ui.ArtworkFolder.Text; Recursive=[bool]$ui.ArtworkRecursive.IsChecked }
} })
$ui.ArtworkMissing.Add_Click({
    foreach ($item in $ui.ArtworkList.ItemsSource) { $item.Selected = $item.Status -eq 'Missing' -and -not $item.IsFolder }
    $ui.ArtworkList.Items.Refresh()
})
$ui.ArtworkClearChecks.Add_Click({ foreach ($item in $ui.ArtworkList.ItemsSource) { $item.Selected=$false }; $ui.ArtworkList.Items.Refresh() })
$ui.ArtworkFetch.Add_Click({ Invoke-UiAction {
    $ui.ArtworkList.CommitEdit() | Out-Null
    $items = @($ui.ArtworkList.ItemsSource | Where-Object Selected)
    if (-not $items.Count -or $items.Count -gt 50) { throw 'Check 1-50 items per download batch.' }
    if ($ui.ArtworkUrl.Text -and $items.Count -ne 1) { throw 'A direct image URL applies to one checked item at a time.' }
    if ($items.Count -eq 1 -and $ui.ArtworkList.SelectedItem -eq $items[0]) { $items[0].MatchName = $ui.ArtworkMatch.Text }
    Start-SetupJob 'ArtworkFetch' @() @{ Items=$items; System=[string]$ui.ArtworkSystem.SelectedItem; Region=[string]$ui.ArtworkRegion.SelectedItem; Url=$ui.ArtworkUrl.Text; UseDat=[bool]$ui.ArtworkUseDat.IsChecked; DatDirectory=(Join-Path $env:LOCALAPPDATA 'Gloam/Setup/databases') }
} })
$ui.ArtworkImport.Add_Click({ Invoke-UiAction {
    Get-SelectedRoot | Out-Null
    $item = $ui.ArtworkList.SelectedItem
    if (-not $item) { throw 'Select a file or folder first.' }
    $dialog = [Windows.Forms.OpenFileDialog]::new(); $dialog.Filter = 'Cover images|*.png;*.jpg;*.jpeg'
    try {
        if ($dialog.ShowDialog() -ne 'OK') { return }
        if ((Get-Item -LiteralPath $dialog.FileName).Length -gt 8MB) { throw 'Image exceeds 8 MiB.' }
        $bytes = ConvertTo-GloamArtworkPng ([IO.File]::ReadAllBytes($dialog.FileName))
        $folder = Join-Path $JobDirectory 'artwork-imports'; [IO.Directory]::CreateDirectory($folder) | Out-Null
        $candidate = Join-Path $folder ([Guid]::NewGuid().ToString('N') + '.png')
        [IO.File]::WriteAllBytes($candidate,$bytes)
        $item.Candidate=$candidate; $item.CandidateHash=Get-GloamHash $candidate; $item.Source='Local import'; $item.Result='Preview ready'; $item.Selected=$true
        $ui.ArtworkList.Items.Refresh(); Show-GloamArtworkSelection
    } finally { $dialog.Dispose() }
} })
$ui.ArtworkApply.Add_Click({ Invoke-UiAction {
    $ui.ArtworkList.CommitEdit() | Out-Null
    $items = @($ui.ArtworkList.ItemsSource | Where-Object { $_.Selected -and $_.Candidate })
    if (-not $items.Count) { throw 'Check at least one prepared preview first.' }
    Start-SetupJob 'ArtworkApply' @() @{ Items=$items; Replace=[bool]$ui.ArtworkReplace.IsChecked }
} })
$ui.ArtworkRemove.Add_Click({ Invoke-UiAction {
    $root = Get-SelectedRoot; $item = $ui.ArtworkList.SelectedItem
    if (-not $item) { throw 'Select an item first.' }
    Clear-GloamArtwork $root $item.Content
    $state = Get-GloamArtworkState $root $item.Content
    $item.Status=$state.Status; $item.Kind=$state.Kind; $item.Image=$state.Image
    $item.Result='Assignment cleared; original images and automatic covers kept.'
    $ui.ArtworkList.Items.Refresh(); Show-GloamArtworkSelection
} })

function Get-GloamSelectedDatSystem {
    $item=$ui.ArtworkList.SelectedItem
    $content=if ($item) { $item.Content } else { $ui.ArtworkFolder.Text.TrimEnd('/')+'/.dat-selection' }
    $system=Resolve-GloamArtworkSystem $content ([string]$ui.ArtworkSystem.SelectedItem)
    if ($system -eq 'Nintendo DS (GameTDB ID)') { $system='Nintendo - Nintendo DS' }
    return $system
}
$ui.ArtworkImportDat.Add_Click({ Invoke-UiAction {
    Get-SelectedRoot | Out-Null
    $system=Get-GloamSelectedDatSystem
    $dialog=[Windows.Forms.OpenFileDialog]::new(); $dialog.Filter='Extracted TOSEC / No-Intro / Redump DATs|*.dat;*.xml'; $dialog.Multiselect=$true
    try {
        if ($dialog.ShowDialog() -eq 'OK') {
            Start-SetupJob 'ArtworkDatImport' $dialog.FileNames @{ System=$system; DatDirectory=(Join-Path $env:LOCALAPPDATA 'Gloam/Setup/databases') }
        }
    } finally { $dialog.Dispose() }
} })
$ui.ArtworkUpdateDat.Add_Click({ Invoke-UiAction {
    $system=Get-GloamSelectedDatSystem
    Start-SetupJob 'ArtworkDatUpdate' @() @{ System=$system; DatDirectory=(Join-Path $env:LOCALAPPDATA 'Gloam/Setup/databases') }
} })

function Open-GloamTitleEditor([string]$Root, [string]$Content) {
    $record = Read-GloamTitleMetadata $Root $Content
    $dialog = [Windows.Window]::new()
    $dialog.Owner = $window; $dialog.Title = 'Title details'; $dialog.Width = 560; $dialog.Height = 560
    $dialog.MinWidth = 420; $dialog.MinHeight = 440
    $dialog.WindowStartupLocation = 'CenterOwner'; $dialog.Background = $window.Background
    $dialog.Foreground = $window.Foreground; $dialog.Resources = $window.Resources
    $panel = [Windows.Controls.StackPanel]::new(); $panel.Margin = '18'
    $scroll = [Windows.Controls.ScrollViewer]::new(); $scroll.VerticalScrollBarVisibility = 'Auto'
    $scroll.Content = $panel; $dialog.Content = $scroll
    $heading = [Windows.Controls.TextBlock]::new(); $heading.Text = $Content
    $heading.TextWrapping = 'Wrap'; $panel.Children.Add($heading) | Out-Null
    $note = [Windows.Controls.TextBlock]::new()
    $note.Text = 'Optional details for the top-screen Context view. These do not rename the file or affect its cover, handler or launch options.'
    $note.TextWrapping = 'Wrap'; $note.Margin = '0,8,0,12'; $panel.Children.Add($note) | Out-Null
    $fields = [ordered]@{}
    foreach ($key in $GloamTitleFields.Keys) {
        $label = [Windows.Controls.TextBlock]::new()
        $label.Text = if ($key -eq 'year') { 'Year (four digits, or leave empty)' } else { $key.Substring(0,1).ToUpperInvariant() + $key.Substring(1) + ' (up to ' + $GloamTitleFields[$key] + ' UTF-8 bytes)' }
        $panel.Children.Add($label) | Out-Null
        $inputBox = [Windows.Controls.TextBox]::new(); $inputBox.Text = $record.Values[$key]
        $inputBox.Margin = '0,3,0,8'; $fields[$key] = $inputBox; $panel.Children.Add($inputBox) | Out-Null
    }
    $errorText = [Windows.Controls.TextBlock]::new(); $errorText.TextWrapping = 'Wrap'
    $panel.Children.Add($errorText) | Out-Null
    $buttons = [Windows.Controls.WrapPanel]::new(); $panel.Children.Add($buttons) | Out-Null
    $save = [Windows.Controls.Button]::new(); $save.Content = 'Save details'; $save.IsDefault = $true
    $cancel = [Windows.Controls.Button]::new(); $cancel.Content = 'Cancel'; $cancel.IsCancel = $true
    $import = [Windows.Controls.Button]::new(); $import.Content = 'Import JSON...'
    foreach ($button in @($save,$import,$cancel)) { $buttons.Children.Add($button) | Out-Null }
    # Keep editor state on the buttons. A closure module hides script-local setup
    # functions; dynamic dialog variables can also be shadowed by a caller's timer.
    $editor = [pscustomobject]@{ Window=$dialog; Fields=$fields; Record=$record; Root=$Root; Content=$Content; Error=$errorText }
    $save.Tag = $editor; $import.Tag = $editor
    $save.Add_Click({
        param($sender, $eventArgs)
        $editor = $sender.Tag
        try {
            $values = [ordered]@{}; foreach ($key in $editor.Fields.Keys) { $values[$key] = $editor.Fields[$key].Text }
            $null = Save-GloamTitleMetadata $editor.Root $editor.Content $values $editor.Record.Hash
            $editor.Window.DialogResult = $true
        } catch { $editor.Error.Text = $_.Exception.Message }
    })
    $import.Add_Click({
        param($sender, $eventArgs)
        $editor = $sender.Tag
        $picker = [Windows.Forms.OpenFileDialog]::new(); $picker.Filter = 'Title details JSON|*.json'
        try {
            if ($picker.ShowDialog() -ne 'OK') { return }
            if ((Get-Item -LiteralPath $picker.FileName).Length -gt 8192) { throw 'Import exceeds 8 KiB.' }
            $data = [IO.File]::ReadAllText($picker.FileName, [Text.UTF8Encoding]::new($false,$true)) | ConvertFrom-Json
            if (-not $data -or $data -is [array] -or $data -is [string]) { throw 'Expected one JSON object with title-detail fields.' }
            foreach ($property in $data.PSObject.Properties) { Test-GloamTitleField $property.Name ([string]$property.Value) }
            foreach ($property in $data.PSObject.Properties) { $editor.Fields[$property.Name].Text = [string]$property.Value }
            $editor.Error.Text = 'Imported for review. Save details applies the changes.'
        } catch { $editor.Error.Text = $_.Exception.Message }
        finally { $picker.Dispose() }
    })
    if ($dialog.ShowDialog()) { Write-Activity ('Saved title details: ' + $Content) }
}
$ui.ArtworkMetadata.Add_Click({ Invoke-UiAction {
    if ($null -ne $script:Job) { throw 'Wait for the current SD operation to finish.' }
    $item = $ui.ArtworkList.SelectedItem
    if (-not $item) { throw 'Scan a collection and select a file or folder first.' }
    Open-GloamTitleEditor (Get-SelectedRoot) $item.Content
} })
