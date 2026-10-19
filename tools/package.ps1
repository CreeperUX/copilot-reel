param(
    [ValidateSet('Release','Debug')][string]$Configuration='Release',
    [string]$CertificateThumbprint
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$binary=Join-Path $projectRoot "build\$Configuration\CopilotReel.exe"
if(-not (Test-Path -LiteralPath $binary)) {throw 'Build the application with tools/build.ps1 first.'}
$sdkBin=Join-Path $PSScriptRoot 'local\sdk\c\bin\10.0.26100.0\x64'
$makeappx=Join-Path $sdkBin 'makeappx.exe'
$signtool=Join-Path $sdkBin 'signtool.exe'
if(-not (Test-Path -LiteralPath $makeappx)) {throw 'Windows SDK packaging tool not found.'}
$stage=Join-Path $projectRoot 'artifacts\package'
# Recreate only this exact staging directory, so removed assets cannot survive
# from an earlier package. Resolve and check the destination before deletion.
$stage=[System.IO.Path]::GetFullPath($stage)
$expectedStage=[System.IO.Path]::GetFullPath((Join-Path $projectRoot 'artifacts\package'))
if($stage -ne $expectedStage -or (Split-Path -Parent $stage) -ne [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'artifacts'))) {
    throw 'The package staging directory is outside the expected workspace location.'
}
if(Test-Path -LiteralPath $stage) {Remove-Item -LiteralPath $stage -Recurse -Force}
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $stage 'CopilotReel.exe')
Copy-Item -LiteralPath (Join-Path $projectRoot 'packaging\AppxManifest.xml') -Destination (Join-Path $stage 'AppxManifest.xml')
Copy-Item -LiteralPath (Join-Path $projectRoot 'packaging\assets') -Destination $stage -Recurse -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\brands') -Destination (Join-Path $stage 'assets') -Recurse -Force
[xml]$manifest=Get-Content -LiteralPath (Join-Path $stage 'AppxManifest.xml')
$version=$manifest.Package.Identity.Version
$package=Join-Path $projectRoot "artifacts\CopilotReel_${version}_x64.msix"
& $makeappx pack /d $stage /p $package /o
if($LASTEXITCODE -ne 0) {throw 'MSIX validation/packing failed.'}
if($CertificateThumbprint) {
    & $signtool sign /fd SHA256 /sha1 $CertificateThumbprint $package
    if($LASTEXITCODE -ne 0) {throw 'MSIX signing failed. Certificate publisher must match the manifest Identity.'}
    & $signtool verify /pa $package
    if($LASTEXITCODE -ne 0) {throw 'Package signature chain verification failed.'}
} else {
    Write-Output 'Package is unsigned. Windows Copilot picker eligibility requires a signed, installed provider.'
}
Write-Output "Package: $package"
Write-Output "Layout for development registration: $stage\AppxManifest.xml"
