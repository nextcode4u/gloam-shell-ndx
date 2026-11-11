# SPDX-License-Identifier: GPL-3.0-or-later
$script:ArtworkRoot = ''
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
    $ui.ArtworkMatch.Text = $item.MatchName
    $ui.ArtworkInfo.Text = $item.Content + "`n" + $item.Kind + "`n" + $item.Result + "`n" + $item.Source
    try { Show-GloamArtworkImage $ui.ArtworkCurrent $item.Image; Show-GloamArtworkImage $ui.ArtworkCandidate $item.Candidate }
    catch { $ui.ArtworkInfo.Text += "`nPreview unavailable: " + $_.Exception.Message }
}
function Complete-GloamArtworkJob($Job) {
    if ($Job.Action -notlike 'Artwork*') { return }
    $file = Join-Path $Job.Folder 'artwork-results.json'
    if (-not (Test-Path -LiteralPath $file)) { return }
    $results = @(Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json)
    if ($Job.Action -eq 'ArtworkScan') { $ui.ArtworkList.ItemsSource = $results }
    else {
        foreach ($result in $results) {
            $item = @($ui.ArtworkList.ItemsSource | Where-Object Content -CEQ $result.Content)
            if ($item.Count -ne 1) { continue }
            foreach ($field in @('Status','Kind','Image','Candidate','CandidateHash','Source','Result','Selected')) { $item[0].$field = $result.$field }
        }
        $ui.ArtworkList.Items.Refresh()
    }
    $ui.ArtworkSummary.Text = 'Items: ' + @($ui.ArtworkList.ItemsSource).Count + '. Select a row to preview. Check boxes choose items for download/apply; existing art requires replacement enabled.'
    if ($ui.ArtworkList.SelectedIndex -lt 0 -and @($ui.ArtworkList.ItemsSource).Count) { $ui.ArtworkList.SelectedIndex = 0 }
    Show-GloamArtworkSelection
}
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
    Start-SetupJob 'ArtworkFetch' @() @{ Items=$items; System=[string]$ui.ArtworkSystem.SelectedItem; Region=[string]$ui.ArtworkRegion.SelectedItem; Url=$ui.ArtworkUrl.Text }
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
