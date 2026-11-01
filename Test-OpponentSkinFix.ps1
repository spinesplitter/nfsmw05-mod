$ErrorActionPreference = 'Stop'
$test = Join-Path ([IO.Path]::GetTempPath()) ('mw-skin-test-' + [guid]::NewGuid().ToString('N'))
New-Item "$test\scripts" -ItemType Directory -Force | Out-Null
[IO.File]::WriteAllBytes("$test\speed.exe", [byte[]]@())
$ini = "$test\scripts\NFSMWExtraOptionsSettings.ini"
$fixture = "[Fixes]`r`nCarSkinFix = 0 // test`r`nOther = 0`r`n[Misc]`r`nExpandMemoryPools = 0`r`n"
[IO.File]::WriteAllText($ini, $fixture, [Text.Encoding]::ASCII)
& "$PSScriptRoot\Install-OpponentSkinFix.ps1" -GameDirectory $test
if ([IO.File]::ReadAllText($ini) -cne $fixture.Replace('CarSkinFix = 0', 'CarSkinFix = 1')) {
    throw 'Exact one-setting replacement/CRLF preservation failed.'
}
$backups = @(Get-ChildItem "$test\scripts" -Filter '*.bak')
if ($backups.Count -ne 1 -or [IO.File]::ReadAllText($backups[0].FullName) -cne $fixture) {
    throw 'Backup contents mismatch.'
}
& "$PSScriptRoot\Install-OpponentSkinFix.ps1" -GameDirectory $test
if (@(Get-ChildItem "$test\scripts" -Filter '*.bak').Count -ne 1) { throw 'Idempotence failed.' }
foreach ($invalid in @('[Misc]', "[Fixes]`nCarSkinFix = 0`nCarSkinFix = 1`n", "[Fixes]`nCarSkinFix = 2`n")) {
    [IO.File]::WriteAllText($ini, $invalid, [Text.Encoding]::ASCII)
    $rejected = $false
    try { & "$PSScriptRoot\Install-OpponentSkinFix.ps1" -GameDirectory $test } catch { $rejected = $true }
    if (!$rejected -or [IO.File]::ReadAllText($ini) -cne $invalid) { throw 'Invalid input was not safely rejected.' }
}
Write-Output 'PASS: exact change, CRLF preservation, backup, idempotence and invalid-input rejection.'
Write-Output "Test fixtures retained at $test"