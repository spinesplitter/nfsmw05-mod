$ErrorActionPreference = 'Stop'
$game = 't:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -'
$repo = Join-Path $game 'vr-source'
try {
    if (Get-Process speed -ErrorAction SilentlyContinue) { throw 'Close the game first.' }
    $latest = (Get-ChildItem "$repo\ffb\src","$repo\ffb\tests","$repo\gameplay\src","$repo\gameplay\tests" -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
    foreach ($check in 'ffb-tests','gameplay-tests','ffb-build','gameplay-build','diff') {
        $file = Get-Item "$repo\kerb-$check.exit"
        if ($file.LastWriteTime -lt $latest -or (Get-Content $file.FullName -Raw).Trim() -ne '0') { throw "Stale or failed $check" }
    }
    $pairs = @(
        @{Source="$repo\ffb\build\dinput8.dll"; Target="$game\scripts\nfsmw_ffb.asi"},
        @{Source="$repo\gameplay\build\dsound.dll"; Target="$game\scripts\nfsmw_gameplay.asi"}
    )
    foreach ($pair in $pairs) {
        if ((Get-Item $pair.Source).LastWriteTime -lt $latest) { throw 'Stale binary' }
        if (!(Test-Path $pair.Target)) { throw "Missing installed target $($pair.Target)" }
    }
    $iniHashes = @(Get-ChildItem "$game\scripts\*.ini" | ForEach-Object { @{Path=$_.FullName; Hash=(Get-FileHash $_.FullName).Hash} })
    $backup = Join-Path $game ('mod-update-backups\kerb-jolts-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item $backup -ItemType Directory | Out-Null
    foreach ($pair in $pairs) { Copy-Item $pair.Target $backup }
    Copy-Item "$game\scripts\*.ini" $backup
    $report = @("Backup: $backup", 'All five fresh validation exits: 0')
    foreach ($pair in $pairs) {
        Copy-Item $pair.Source $pair.Target
        $hash = (Get-FileHash $pair.Source).Hash
        if ((Get-FileHash $pair.Target).Hash -ne $hash) { throw 'Binary hash mismatch' }
        $report += "$($pair.Target): $hash"
    }
    foreach ($ini in $iniHashes) { if ((Get-FileHash $ini.Path).Hash -ne $ini.Hash) { throw 'Installed INI changed' } }
    $report += 'Installed INIs unchanged. SurfaceRumble is ignored by code. VR unchanged.'
    $report | Set-Content "$repo\kerb-install.txt"
} catch {
    "Installation/verification failed: $_" | Set-Content "$repo\kerb-install.txt"
    exit 1
}