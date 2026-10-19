[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$package=Get-AppxPackage -Name 'CopilotReel.Desktop'
if(-not $package) {throw 'Copilot Reel is not installed for the current user.'}
$backupRoot=Join-Path $projectRoot ('artifacts\backups\'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
$dataRoots=@(
    (Join-Path $env:LOCALAPPDATA "Packages\$($package.PackageFamilyName)\LocalCache\Local\CopilotReel"),
    (Join-Path $env:LOCALAPPDATA 'CopilotReel')
)
$records=@()
for($index=0;$index -lt $dataRoots.Count;$index++) {
    foreach($name in @('apps.ini','preferences.ini')) {
        $source=Join-Path $dataRoots[$index] $name
        if(Test-Path -LiteralPath $source) {
            $destination=Join-Path $backupRoot "$index-$name"
            Copy-Item -LiteralPath $source -Destination $destination
            $records += [pscustomobject]@{
                Source=$source
                Backup=$destination
                Sha256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
                Length=(Get-Item -LiteralPath $destination).Length
            }
        }
    }
}
$selection=Get-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Windows\Shell\BrandedKey' -ErrorAction SilentlyContinue
$metadata=[pscustomobject]@{
    Package=$package.PackageFullName
    Family=$package.PackageFamilyName
    Files=$records
    KeyChoice=($selection | Select-Object BrandedKeyChoiceType,AppAumid)
}
$metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $backupRoot 'manifest.json') -Encoding utf8
Write-Output $backupRoot
