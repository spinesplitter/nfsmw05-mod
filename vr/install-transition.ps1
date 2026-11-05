param([ValidateSet('transition','fastshift')][string]$Revision = 'transition')
$ErrorActionPreference = 'Stop'
$r = 'T:\Games\ModWanted\release-candidate-source\vr'
$g = 'T:\Games\ModWanted\driverNeed For Speed Most Wanted Black Edition - Copy'
if (Get-Process speed -ErrorAction SilentlyContinue) { throw 'Game running; installation skipped' }
if (!(Select-String -LiteralPath "$r\$Revision-build.log" -SimpleMatch 'Built bin\d3d9.dll')) { throw 'Build incomplete' }
if (!(Select-String -LiteralPath "$r\$Revision-tests.log" -SimpleMatch 'MARKED ARM REGRESSIONS PASSED')) { throw 'Tests incomplete' }
$b = Join-Path $g ("DRIVERMOD\$Revision-backup-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $b | Out-Null
$dst = "$g\scripts\nfsmw_vr.asi"
Copy-Item -LiteralPath $dst -Destination $b
Copy-Item -LiteralPath "$r\bin\d3d9.dll" -Destination $dst
$h = (Get-FileHash -LiteralPath $dst).Hash
if ($h -ne (Get-FileHash -LiteralPath "$r\bin\d3d9.dll").Hash) { throw 'Hash mismatch' }
@("Backup: $b", "Installed: $dst", "SHA256: $h") | Set-Content -LiteralPath "$r\$Revision-installation.txt"