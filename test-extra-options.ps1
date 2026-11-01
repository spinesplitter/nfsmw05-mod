$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$test = Join-Path $root ("dist\exopts-test-" + [guid]::NewGuid().ToString('N'))
$bundle = "$test\extras\ExtraOptions"
function Check($Condition, $Message) { if (!$Condition) { throw $Message } }
& "$root\prepare-extra-options.ps1" -Destination $bundle
Copy-Item -LiteralPath "$root\release\Install-ExtraOptions.ps1" -Destination $test
Set-Content -LiteralPath "$test\speed.exe" -Value 'test fixture, not executable' -Encoding ASCII
& "$test\Install-ExtraOptions.ps1" -GameDirectory $test
Check ((Get-FileHash "$test\scripts\NFSMWExtraOptions.asi").Hash -eq (Get-FileHash "$bundle\scripts\NFSMWExtraOptions.asi").Hash) 'Installed ASI differs'
Check (!(Test-Path "$test\dinput8.dll")) 'Installer replaced ASI loader'
$ini = "$test\scripts\NFSMWExtraOptionsSettings.ini"
$text = Get-Content -LiteralPath $ini -Raw
Check ($text -match '(?m)^Maximum = 15\s*$') 'Opponent maximum missing'
Check ($text -match '(?m)^High = 100\s*$') 'Traffic maximum missing'
$hotkeys = ($text -split '\[Hotkeys\]')[1] -split '\[LapControllers\]'
Check (!($hotkeys[0] -match '(?m)^\w+\s*=\s*[1-9]')) 'Conflicting hotkeys'
Check ($text -match '(?m)^NoCatchUp\s*=\s*0\b') 'Experimental catchup enabled'
Add-Content -LiteralPath $ini -Value '; preserved user setting'
$before = (Get-FileHash $ini).Hash
& "$test\Install-ExtraOptions.ps1" -GameDirectory $test
Check ((Get-FileHash $ini).Hash -eq $before) 'Existing settings overwritten'
Check (@(Get-ChildItem "$test\scripts\NFSMWExtraOptions.asi.before-bundle-*.bak").Count -eq 1) 'ASI backup missing'
Check (Test-Path "$bundle\LICENSE-extra-options.txt") 'License missing'
Check (Test-Path "$bundle\NFSMWExOpts-source-*.zip") 'Corresponding source missing'
Write-Output "PASS: bundle verification, first install, settings preservation, loader preservation and binary backup. Fixtures: $test"