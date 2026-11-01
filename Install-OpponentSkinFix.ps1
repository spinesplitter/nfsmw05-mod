param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory
)

$ErrorActionPreference = 'Stop'
$GameDirectory = (Resolve-Path -LiteralPath $GameDirectory).Path
if (!(Test-Path -LiteralPath (Join-Path $GameDirectory 'speed.exe'))) {
    throw 'GameDirectory must contain speed.exe.'
}
if (Get-Process -Name speed -ErrorAction SilentlyContinue) {
    throw 'Close the game before changing Extra Options settings.'
}
$ini = Join-Path $GameDirectory 'scripts\NFSMWExtraOptionsSettings.ini'
# A single-byte round trip preserves the original encoding, BOM and line endings.
$encoding = [System.Text.Encoding]::GetEncoding(28591)
$bytes = [System.IO.File]::ReadAllBytes($ini)
$text = $encoding.GetString($bytes)
$section = [regex]::Match($text, '(?ms)^\[Fixes\][^\r\n]*\r?\n(?<body>.*?)(?=^\[|\z)')
if (!$section.Success) { throw 'Missing [Fixes] section; no settings changed.' }
$pattern = '(?m)^(?<prefix>[ \t]*CarSkinFix[ \t]*=[ \t]*)[01](?<suffix>[ \t]*(?://[^\r\n]*|;[^\r\n]*)?\r?)$'
$matches = [regex]::Matches($section.Value, $pattern)
if ($matches.Count -ne 1) { throw 'Expected exactly one boolean CarSkinFix setting in [Fixes].' }
$updatedSection = [regex]::Replace($section.Value, $pattern, '${prefix}1${suffix}')
$updated = $text.Substring(0, $section.Index) + $updatedSection + $text.Substring($section.Index + $section.Length)
if ($updated -ceq $text) {
    Write-Output 'CarSkinFix is already enabled; no settings changed.'
    return
}
$backup = "$ini.before-opponent-skin-fix-$([guid]::NewGuid().ToString('N')).bak"
[System.IO.File]::WriteAllBytes($backup, $bytes)
if ((Get-FileHash -LiteralPath $backup).Hash -ne (Get-FileHash -LiteralPath $ini).Hash) {
    throw 'Backup verification failed; no settings changed.'
}
if (Get-Process -Name speed -ErrorAction SilentlyContinue) {
    throw 'The game started during backup; no settings changed.'
}
$expected = $encoding.GetBytes($updated)
[System.IO.File]::WriteAllBytes($ini, $expected)
if ([Convert]::ToBase64String([System.IO.File]::ReadAllBytes($ini)) -cne [Convert]::ToBase64String($expected)) {
    throw "Settings verification failed. Restore the backup: $backup"
}
Write-Output "Enabled experimental [Fixes] CarSkinFix=1 in $ini"
Write-Output "Verified backup: $backup"
Write-Output 'Restart and inspect the race starting grid. Visual correction is not yet verified.'