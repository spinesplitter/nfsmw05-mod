param([string]$GameDirectory = $PSScriptRoot)
$ErrorActionPreference = 'Stop'
$source = "$PSScriptRoot\extras\ExtraOptions\scripts"
if (!(Test-Path -LiteralPath "$GameDirectory\speed.exe")) { throw 'Select the game directory containing speed.exe with -GameDirectory.' }
if (Get-Process -Name speed -ErrorAction SilentlyContinue) { throw 'Close the game before installing.' }
$scripts = Join-Path $GameDirectory 'scripts'
New-Item -ItemType Directory -Force -Path $scripts | Out-Null
$asi = Join-Path $scripts 'NFSMWExtraOptions.asi'
if (Test-Path -LiteralPath $asi) {
    Copy-Item -LiteralPath $asi -Destination "$asi.before-bundle-$(Get-Date -Format yyyyMMdd-HHmmss).bak"
}
Copy-Item -LiteralPath "$source\NFSMWExtraOptions.asi" -Destination $asi -Force
$ini = Join-Path $scripts 'NFSMWExtraOptionsSettings.ini'
if (!(Test-Path -LiteralPath $ini)) {
    Copy-Item -LiteralPath "$source\NFSMWExtraOptionsSettings.ini" -Destination $ini
} else {
    Write-Warning 'Existing ExOpts settings preserved. Disable FreezeCar=115 (F4) and review other hotkeys before playing.'
}
Write-Output 'Extra Options installed. Our existing ASI loader is retained. Restart the game.'