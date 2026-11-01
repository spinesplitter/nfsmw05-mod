$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$report = Join-Path $PSScriptRoot 'recovery-install.json'
try {
    if (Get-Process -Name speed -ErrorAction SilentlyContinue) {
        throw 'Game is running; nothing installed. Close the game before retrying.'
    }
    $backup = Join-Path $root ('mod-update-backups\traffic-wheel-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Path $backup | Out-Null
    $pairs = @(
        @{ Source = (Join-Path $PSScriptRoot 'gameplay\build\dsound.dll'); Target = (Join-Path $root 'scripts\nfsmw_gameplay.asi') },
        @{ Source = (Join-Path $PSScriptRoot 'vr\bin\d3d9.dll'); Target = (Join-Path $root 'scripts\nfsmw_vr.asi') }
    )
    $ini = Join-Path $root 'scripts\nfsmw_vr_wheel.ini'
    $iniHash = (Get-FileHash -LiteralPath $ini).Hash
    Copy-Item -LiteralPath $ini -Destination $backup
    $results = @()
    foreach ($pair in $pairs) {
        $before = (Get-FileHash -LiteralPath $pair.Target).Hash
        $saved = Join-Path $backup (Split-Path -Leaf $pair.Target)
        Copy-Item -LiteralPath $pair.Target -Destination $saved
        if ((Get-FileHash -LiteralPath $saved).Hash -ne $before) { throw 'Backup verification failed' }
    }
    foreach ($pair in $pairs) {
        if (Get-Process -Name speed -ErrorAction SilentlyContinue) { throw 'Game started during installation' }
        Copy-Item -LiteralPath $pair.Source -Destination $pair.Target
        $expected = (Get-FileHash -LiteralPath $pair.Source).Hash
        $actual = (Get-FileHash -LiteralPath $pair.Target).Hash
        if ($expected -ne $actual) { throw 'Installed plugin hash mismatch' }
        $results += @{ Path = $pair.Target; SHA256 = $actual }
    }
    if ((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'Wheel INI changed during installation' }
    @{ Status = 'Installed'; Backup = $backup; Plugins = $results; WheelIniUnchanged = $true; WheelIniSHA256 = $iniHash } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $report -Encoding UTF8
} catch {
    @{ Status = 'Failed'; Error = $_.Exception.Message; Backup = $backup } |
        ConvertTo-Json | Set-Content -LiteralPath $report -Encoding UTF8
    throw
}