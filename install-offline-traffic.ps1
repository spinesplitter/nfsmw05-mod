$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$backup = $null
try {
    foreach ($name in @('traffic-v2-tests.exit', 'traffic-v2-build.exit')) {
        if ((Get-Content -LiteralPath (Join-Path $PSScriptRoot $name) -Raw).Trim() -ne '0') {
            throw "Validation failed: $name"
        }
    }
    if (Get-Process speed -ErrorAction SilentlyContinue) { throw 'Close the game before installing.' }
    $source = Join-Path $PSScriptRoot 'gameplay\build\dsound.dll'
    $newestSource = Get-ChildItem (Join-Path $PSScriptRoot 'gameplay\src') -File |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ((Get-Item -LiteralPath $source).LastWriteTimeUtc -lt $newestSource.LastWriteTimeUtc) {
        throw 'Build is older than source; wait for the current build to finish.'
    }
    $target = Join-Path $root 'scripts\nfsmw_gameplay.asi'
    $protected = @('scripts\nfsmw_gameplay.ini', 'scripts\nfsmw_vr.asi',
        'scripts\nfsmw_vr_wheel.ini', 'scripts\NFSMWExtraOptionsSettings.ini', 'speed.exe')
    $hashes = @{}
    foreach ($relative in $protected) {
        $path = Join-Path $root $relative
        if (Test-Path -LiteralPath $path) { $hashes[$relative] = (Get-FileHash -LiteralPath $path).Hash }
    }
    $backup = Join-Path $root ('mod-update-backups\offline-traffic-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Path $backup | Out-Null
    $saved = Join-Path $backup 'nfsmw_gameplay.asi'
    $before = (Get-FileHash -LiteralPath $target).Hash
    Copy-Item -LiteralPath $target -Destination $saved
    if ((Get-FileHash -LiteralPath $saved).Hash -ne $before) { throw 'Backup verification failed.' }
    if (Get-Process speed -ErrorAction SilentlyContinue) { throw 'Game started during installation.' }
    Copy-Item -LiteralPath $source -Destination $target
    $expected = (Get-FileHash -LiteralPath $source).Hash
    if ((Get-FileHash -LiteralPath $target).Hash -ne $expected) { throw 'Installed hash mismatch.' }
    foreach ($relative in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $root $relative)).Hash -ne $hashes[$relative]) {
            throw "Protected file changed: $relative"
        }
    }
    @{ Status = 'Installed'; Backup = $backup; SHA256 = $expected; ProtectedHashes = $hashes } |
        ConvertTo-Json -Depth 4 | Set-Content (Join-Path $PSScriptRoot 'traffic-v2-install.json') -Encoding UTF8
} catch {
    @{ Status = 'Failed'; Backup = $backup; Error = $_.Exception.Message } |
        ConvertTo-Json | Set-Content (Join-Path $PSScriptRoot 'traffic-v2-install.json') -Encoding UTF8
    throw
}