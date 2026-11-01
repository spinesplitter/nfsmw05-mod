$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$backup = (Get-Content "$PSScriptRoot\fps-fix-backup-path.txt" -Raw).Trim()
$result = foreach ($name in 'nfsmw_vr.ini', 'nfsmw_gameplay.ini', 'nfsmw_ffb.ini') {
    $old = @(Get-Content "$backup\$name")
    $current = @(Get-Content "$root\scripts\$name")
    $expected = switch ($name) {
        'nfsmw_vr.ini' { $old -replace '^ResolutionScale=.*$', 'ResolutionScale=1.0' }
        'nfsmw_gameplay.ini' { $old -replace '^LogPhysics=1$', 'LogPhysics=0' }
        'nfsmw_ffb.ini' { $old -replace '^LogTelemetry=1$', 'LogTelemetry=0' }
    }
    if (Compare-Object $expected $current -SyncWindow 0) {
        throw "Unexpected changes in $name"
    }
    "$name : only intended setting changed"
}
$result += "Backup: $backup"
$result | Set-Content "$PSScriptRoot\fps-fix-verification.txt" -Encoding utf8