$ErrorActionPreference = 'Stop'
$game = 't:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -'
$repo = Join-Path $game 'vr-source'
try {
    if (Get-Process speed -ErrorAction SilentlyContinue) { throw 'Game is running; close it before installation.' }
    $binary = Get-Item "$repo\gameplay\build\dsound.dll"
    $latest = (Get-ChildItem "$repo\gameplay\src","$repo\gameplay\tests" -File -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
    foreach ($check in 'tests','build','diff') {
        $exit = Get-Item "$repo\slider-$check.exit"
        if ($exit.LastWriteTime -lt $latest -or (Get-Content $exit.FullName -Raw).Trim() -ne '0') { throw "Missing, stale or failed $check validation" }
    }
    if ($binary.LastWriteTime -lt $latest) { throw 'Stale gameplay binary' }
    $inis = @(Get-ChildItem "$game\scripts\*.ini" | ForEach-Object { @{Path=$_.FullName; Hash=(Get-FileHash $_.FullName).Hash} })
    $backup = Join-Path $game ('mod-update-backups\race-sliders-12kmh-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item $backup -ItemType Directory | Out-Null
    Copy-Item "$game\scripts\nfsmw_gameplay.asi" $backup
    Copy-Item "$game\scripts\*.ini" $backup
    Copy-Item $binary.FullName "$game\scripts\nfsmw_gameplay.asi"
    $builtHash = (Get-FileHash $binary.FullName).Hash
    if ((Get-FileHash "$game\scripts\nfsmw_gameplay.asi").Hash -ne $builtHash) { throw 'Installed hash mismatch' }
    foreach ($ini in $inis) { if ((Get-FileHash $ini.Path).Hash -ne $ini.Hash) { throw "INI changed: $($ini.Path)" } }
    @('Installed gameplay; all validation exit codes 0; binary fresh.', "Backup: $backup", "SHA256: $builtHash", 'Installed INIs unchanged. VR/FFB unchanged.') | Set-Content "$repo\slider-install.txt"
} catch {
    "NOT INSTALLED/verification failed: $_" | Set-Content "$repo\slider-install.txt"
    exit 1
}