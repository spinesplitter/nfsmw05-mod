param([string]$Destination = "$PSScriptRoot\dist\extra-options")
$ErrorActionPreference = 'Stop'
$commit = '8a094fe29d28195bdcf69122dbfe945f16315f9a'
$cache = "$PSScriptRoot\dist\exopts-cache"
New-Item -ItemType Directory -Force -Path $cache | Out-Null
function Download-Checked($Url, $Path, $Hash) {
    if (!(Test-Path -LiteralPath $Path)) {
        Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Path
    }
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash) {
        throw "SHA256 mismatch: $Path (remove this cached file and retry)"
    }
}
Download-Checked 'https://github.com/ExOptsTeam/NFSMWExOpts/releases/download/v10.0.1.1337/NFSMW.ExOpts.v10.0.1.1337.zip' "$cache\release.zip" '0B128F1B8C03AEA349DDFB4C9BE1373AD1B35853881ECDA24B453F3B465A5433'
Download-Checked "https://api.github.com/repos/ExOptsTeam/NFSMWExOpts/zipball/$commit" "$cache\source.zip" '1A77110F8B513DA3C902294B18A34AF41EDB500300FA6179841581FCCB01E879'
Expand-Archive -LiteralPath "$cache\release.zip" -DestinationPath "$cache\release" -Force
Expand-Archive -LiteralPath "$cache\source.zip" -DestinationPath "$cache\source" -Force
$sourceRoot = @(Get-ChildItem -LiteralPath "$cache\source" -Directory)
if ($sourceRoot.Count -ne 1) { throw 'Unexpected source archive layout' }
$asi = "$cache\release\scripts\NFSMWExtraOptions.asi"
if ((Get-FileHash -LiteralPath $asi).Hash -ne '32BC3AE6C3F36218AE507B666D8FB816C2263FDB7819498286BCA11537EC8FF2') {
    throw 'Unexpected Extra Options binary'
}
New-Item -ItemType Directory -Force -Path "$Destination\scripts" | Out-Null
Copy-Item -LiteralPath $asi -Destination "$Destination\scripts\NFSMWExtraOptions.asi" -Force
Copy-Item -LiteralPath "$PSScriptRoot\release\NFSMWExtraOptionsSettings.ini" -Destination "$Destination\scripts\NFSMWExtraOptionsSettings.ini" -Force
Copy-Item -LiteralPath "$cache\release\Read Me.txt" -Destination "$Destination\README-upstream.txt" -Force
Copy-Item -LiteralPath "$cache\source.zip" -Destination "$Destination\NFSMWExOpts-source-$commit.zip" -Force
Copy-Item -LiteralPath "$($sourceRoot[0].FullName)\LICENSE" -Destination "$Destination\LICENSE-extra-options.txt" -Force
Copy-Item -LiteralPath "$PSScriptRoot\release\EXTRA-OPTIONS.md" -Destination $Destination -Force
Write-Output "Prepared verified Extra Options bundle: $Destination"