[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$BackupRoot)
$ErrorActionPreference='Stop'
$package=Get-AppxPackage -Name 'CopilotReel.Desktop'
if(@($package).Count -ne 1 -or -not $package) {throw 'Exactly one installed Copilot Reel package is required.'}
$backupDirectory=[System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $BackupRoot).Path)
$manifestPath=Join-Path $backupDirectory 'manifest.json'
$metadata=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if($metadata.Family -ne $package.PackageFamilyName) {throw 'Backup package family does not match the installed package.'}
$dataRoots=@(
    (Join-Path $env:LOCALAPPDATA "Packages\$($package.PackageFamilyName)\LocalCache\Local\CopilotReel"),
    (Join-Path $env:LOCALAPPDATA 'CopilotReel')
)
$operations=@()
$seen=@{}
# Validate every record before writing any file. Paths in a backup manifest are
# data, never permission to restore arbitrary locations or execute commands.
foreach($record in $metadata.Files) {
    $leaf=Split-Path -Leaf $record.Backup
    if($leaf -notmatch '^(0|1)-(apps\.ini|parked\.ini|preferences\.ini|behavior\.ini)$') {
        throw "Unsupported backup file: $leaf"
    }
    $rootIndex=[int]$Matches[1]
    $name=$Matches[2]
    $source=[System.IO.Path]::GetFullPath((Join-Path $backupDirectory $leaf))
    $destination=[System.IO.Path]::GetFullPath((Join-Path $dataRoots[$rootIndex] $name))
    if([System.IO.Path]::GetFullPath([string]$record.Backup) -ne $source -or
       [System.IO.Path]::GetFullPath([string]$record.Source) -ne $destination) {
        throw "Backup paths do not match the allowed application data locations: $leaf"
    }
    if($seen.ContainsKey($destination)) {throw "Duplicate restore destination: $name"}
    $seen[$destination]=$true
    if(-not (Test-Path -LiteralPath $source -PathType Leaf)) {throw "Backup file is missing: $leaf"}
    $item=Get-Item -LiteralPath $source
    $hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if($item.Length -ne [long]$record.Length -or $hash -ne $record.Sha256) {
        throw "Backup integrity check failed: $leaf"
    }
    $operations += [pscustomobject]@{Source=$source;Destination=$destination;Sha256=$hash}
}
foreach($operation in $operations) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $operation.Destination) -Force | Out-Null
    Copy-Item -LiteralPath $operation.Source -Destination $operation.Destination -Force
    if((Get-FileHash -LiteralPath $operation.Destination -Algorithm SHA256).Hash -ne $operation.Sha256) {
        throw "Restored file verification failed: $($operation.Destination)"
    }
}
[pscustomobject]@{
    Package=$package.PackageFullName
    RestoredFiles=$operations.Count
    Backup=$backupDirectory
}
