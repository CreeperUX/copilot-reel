[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackagePath)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$packagePath=(Resolve-Path -LiteralPath $PackagePath).Path
$signtool=Join-Path $PSScriptRoot 'local\sdk\c\bin\10.0.26100.0\x64\signtool.exe'
& $signtool verify /pa $packagePath
if($LASTEXITCODE -ne 0) {throw 'The package must have a trusted signature before uninstalling anything.'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[System.IO.Compression.ZipFile]::OpenRead($packagePath)
$builtHash=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'build\Release\CopilotReel.exe') -Algorithm SHA256).Hash
try {
    $entry=$archive.GetEntry('AppxManifest.xml')
    if(-not $entry) {throw 'Package manifest is missing.'}
    $reader=[System.IO.StreamReader]::new($entry.Open())
    try {[xml]$manifest=$reader.ReadToEnd()} finally {$reader.Dispose()}
    $executable=$archive.GetEntry('CopilotReel.exe')
    if(-not $executable) {throw 'Packaged executable is missing.'}
    $stream=$executable.Open();$hasher=[System.Security.Cryptography.SHA256]::Create()
    try {$packageHash=[BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-','')}
    finally {$stream.Dispose();$hasher.Dispose()}
    if($packageHash -ne $builtHash) {throw 'Package does not contain the tested Release executable.'}
} finally {$archive.Dispose()}
$identity=$manifest.Package.Identity
if($identity.Name -ne 'CopilotReel.Desktop' -or $identity.Publisher -ne 'CN=CopilotReel Development' -or $identity.ProcessorArchitecture -ne 'x64') {
    throw "Refusing to install architecture '$($identity.ProcessorArchitecture)'. This host installs only the x64 package; an ARM64 package must not replace the installed x64 app."
}
$installed=@(Get-AppxPackage -Name 'CopilotReel.Desktop')
if($installed.Count -gt 1) {throw 'Unexpected multiple installed Copilot Reel packages.'}
$backup=$null
if($installed.Count -eq 1) {
    $oldBinary=Join-Path $installed[0].InstallLocation 'CopilotReel.exe'
    $request=Start-Process -FilePath $oldBinary -ArgumentList '--quit' -WindowStyle Hidden -PassThru
    if(-not $request.WaitForExit(10000)) {throw "The app did not accept shutdown. Backup: $backup"}
    $deadline=(Get-Date).AddSeconds(5)
    do {
        $running=@(Get-Process -Name CopilotReel -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $oldBinary})
        if($running.Count -eq 0) {break}
        Start-Sleep -Milliseconds 100
    } while((Get-Date) -lt $deadline)
    if($running.Count -ne 0) {throw "Close the unsaved app editor before installation. Backup: $backup"}
    $backup=& (Join-Path $PSScriptRoot 'backup-user-data.ps1')
    Remove-AppxPackage -Package $installed[0].PackageFullName
    if(Get-AppxPackage -Name 'CopilotReel.Desktop') {throw 'The previous package is still installed.'}
}
try {
    Add-AppxPackage -Path $packagePath
    if($backup) {& (Join-Path $PSScriptRoot 'restore-user-data.ps1') -BackupRoot $backup}
} catch {throw "Installation or restoration failed. User data backup: $backup. $($_.Exception.Message)"}
$current=Get-AppxPackage -Name 'CopilotReel.Desktop'
if(-not $current -or $current.Version.ToString() -ne $identity.Version) {throw 'Installed version does not match the requested package.'}
$installedHash=(Get-FileHash -LiteralPath (Join-Path $current.InstallLocation 'CopilotReel.exe') -Algorithm SHA256).Hash
if($builtHash -ne $installedHash) {throw 'Installed executable differs from the tested Release build.'}
$assetCount=0
foreach($asset in Get-ChildItem -LiteralPath (Join-Path $projectRoot 'assets\brands') -File -Recurse) {
    $relative=$asset.FullName.Substring((Join-Path $projectRoot 'assets\brands').Length+1)
    $destination=Join-Path $current.InstallLocation ('assets\brands\'+$relative)
    if(-not (Test-Path -LiteralPath $destination) -or (Get-FileHash -LiteralPath $asset.FullName).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) {
        throw "Installed artwork differs: $relative"
    }
    $assetCount++
}
$registration=& (Join-Path $PSScriptRoot 'verify-registration.ps1') | ConvertFrom-Json
$receipt=[pscustomobject]@{
    Version=$current.Version.ToString();Package=$current.PackageFullName;Backup=$backup
    ExecutableSha256=$installedHash;VerifiedAssetFiles=$assetCount
    ProviderDeclarationValid=$registration.ProviderDeclarationValid
    SelectedByUser=$registration.SelectedByUser;CleanInstall=$true
}
$receipt | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $projectRoot 'artifacts\installation.json') -Encoding utf8
$receipt
