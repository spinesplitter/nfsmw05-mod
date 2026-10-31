$ErrorActionPreference = 'Stop'
$game = 't:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -'
$repo = Join-Path $game 'vr-source'
try {
    if (Get-Process speed -ErrorAction SilentlyContinue) { throw 'Game is running; close it before installation.' }
    $binary = Get-Item "$repo\gameplay\build\dsound.dll"
    $latest = (Get-ChildItem "$repo\gameplay\src","$repo\gameplay\tests" -File -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
    foreach ($check in 'tests','build','diff') {
        $result = Get-Item "$repo\opponent-damage-$check.exit"
        if ($result.LastWriteTime -lt $latest -or (Get-Content $result.FullName -Raw).Trim() -ne '0') { throw "Missing, stale or failed $check validation" }
    }
    if ($binary.LastWriteTime -lt $latest) { throw 'Stale gameplay binary' }
    $target = "$game\scripts\nfsmw_gameplay.asi"
    if (!(Test-Path -LiteralPath $target)) { throw 'Expected installed gameplay ASI not found' }
    $inis = @(Get-ChildItem "$game\scripts\*.ini" | ForEach-Object { @{Path=$_.FullName; Hash=(Get-FileHash $_.FullName).Hash} })
    $backup = Join-Path $game ('mod-update-backups\opponent-damage-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item $backup -ItemType Directory | Out-Null
    Copy-Item -LiteralPath $target -Destination $backup
    Copy-Item "$game\scripts\*.ini" -Destination $backup
    if (Get-Process speed -ErrorAction SilentlyContinue) { throw 'Game started during preflight; installation stopped.' }
    Copy-Item -LiteralPath $binary.FullName -Destination $target
    $builtHash = (Get-FileHash $binary.FullName).Hash
    if ((Get-FileHash $target).Hash -ne $builtHash) { throw 'Installed hash mismatch' }
    foreach ($ini in $inis) { if ((Get-FileHash $ini.Path).Hash -ne $ini.Hash) { throw "INI changed: $($ini.Path)" } }
    @('SUCCESS: gameplay installed; fresh tests/build/diff all exit 0.', "Target: $target", "Backup: $backup", "SHA256: $builtHash", 'Installed INIs unchanged. VR/FFB binaries unchanged.') | Set-Content "$repo\opponent-damage-install.txt"
} catch {
    "Installation failed or verification incomplete: $_" | Set-Content "$repo\opponent-damage-install.txt"
    exit 1
}