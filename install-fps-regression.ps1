$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
if (Get-Process speed -ErrorAction SilentlyContinue) { throw 'Close the game before installation.' }
& git -C $PSScriptRoot -c core.safecrlf=false diff --check | Out-File "$PSScriptRoot\fps-regression-diff.log" -Encoding utf8
if ($LASTEXITCODE -ne 0) { throw 'Diff check failed; nothing installed' }
foreach ($stage in 'tests', 'build') {
    if ((Get-Content "$PSScriptRoot\fps-regression-$stage.exit" -Raw).Trim() -ne '0') {
        throw "$stage did not pass"
    }
}
$source = "$PSScriptRoot\vr\bin\d3d9.dll"
$target = "$root\scripts\nfsmw_vr.asi"
$backup = "$root\mod-update-backups\fps-mirror-lock-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $target -Destination "$backup\nfsmw_vr.asi"
if ((Get-FileHash $target).Hash -ne (Get-FileHash "$backup\nfsmw_vr.asi").Hash) {
    throw 'Backup verification failed'
}
Copy-Item -LiteralPath $source -Destination $target
if ((Get-FileHash $source).Hash -ne (Get-FileHash $target).Hash) { throw 'Installation hash mismatch' }
@{
    backup = $backup
    installed = $target
    sha256 = (Get-FileHash $target).Hash
    renderScale = @(Select-String -LiteralPath "$root\scripts\nfsmw_vr.ini" -Pattern '^ResolutionScale=' | ForEach-Object { $_.Line })
    installedAt = (Get-Date).ToString('o')
} | ConvertTo-Json | Set-Content "$PSScriptRoot\fps-regression-install.json" -Encoding utf8