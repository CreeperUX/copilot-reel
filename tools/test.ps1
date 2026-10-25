param(
    # unit: build Release x64 and run the unit tests. smoke: unit, then the seam preflight and the
    # main-menu and hub groups. regression: unit, every seam group and the still comparison.
    [ValidateSet('unit','smoke','regression')][string]$Tier = 'regression',
    # Also check that the installed package runs the executable this build produced.
    [switch]$Installed
)
# The one entry point for unit, smoke and regression tests. Every step writes its own log under
# artifacts\qa\test-<stamp>\, the run ends with one summary.json, and the exit status is 1 when a
# step failed. Nothing here moves the pointer, sends input, takes the foreground or quits the
# installed reel: the seam steps drive separate unpackaged test instances (docs/testing-plan.md).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$evidence = Join-Path $root "artifacts\qa\test-$stamp"
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$steps = New-Object System.Collections.Generic.List[object]

# Runs one command line through cmd so stdout and stderr land in one log without PowerShell
# turning native stderr into errors; returns the exit status.
function Invoke-Step([string]$Name, [string]$CommandLine) {
    $log = Join-Path $evidence "$Name.log"
    $started = Get-Date
    Push-Location $root
    try { & cmd.exe /d /c "$CommandLine > `"$log`" 2>&1"; $status = $LASTEXITCODE }
    finally { Pop-Location }
    $seconds = [math]::Round(((Get-Date) - $started).TotalSeconds, 1)
    $text = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw -Encoding UTF8 } else { '' }
    # The seam and capture runners end with a JSON line that names their own evidence.
    $report = $null
    foreach ($line in @($text -split "`r?`n" | Where-Object { $_ -match '^\{.*"passed"' })) { $report = $line }
    $step = [ordered]@{ name = $Name; passed = $status -eq 0; exitCode = $status; seconds = $seconds; log = $log }
    if ($report) { $step.report = $report | ConvertFrom-Json }
    $steps.Add([pscustomobject]$step)
    $mark = if ($status -eq 0) { 'passed' } else { 'FAILED' }
    Write-Host ("[{0}] {1} ({2} s)" -f $Name, $mark, $seconds)
    if ($status -ne 0) {
        Write-Host "  log: $log"
        @($text -split "`r?`n" | Where-Object { $_ -match 'FAIL|error|Error|assert' } | Select-Object -Last 8) | ForEach-Object { Write-Host "  $_" }
    }
    return $status -eq 0
}

$python = 'uv run --python 3.14'
$built = Invoke-Step 'unit' 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -Configuration Release -Architecture x64'
if ($built) {
    if ($Tier -eq 'smoke') {
        Invoke-Step 'seam' "$python python tests\run-reel-seam.py --groups a,hub" | Out-Null
    } elseif ($Tier -eq 'regression') {
        Invoke-Step 'seam' "$python python tests\run-reel-seam.py" | Out-Null
        Invoke-Step 'stills' "$python --with pillow python tools\capture-seam.py" | Out-Null
    }
    if ($Installed) {
        $log = Join-Path $evidence 'installed.log'
        $package = Get-AppxPackage -Name 'CopilotReel.Desktop' | Select-Object -First 1
        $lines = @()
        $okay = $false
        if (-not $package) { $lines += 'Copilot Reel is not installed.' }
        else {
            $installedExe = Join-Path $package.InstallLocation 'CopilotReel.exe'
            $installedHash = (Get-FileHash -LiteralPath $installedExe -Algorithm SHA256).Hash
            $builtHash = (Get-FileHash -LiteralPath (Join-Path $root 'build\Release\CopilotReel.exe') -Algorithm SHA256).Hash
            $running = @(Get-Process -Name 'CopilotReel' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $installedExe }).Count -gt 0
            $lines += "package: $($package.PackageFullName)", "installed: $installedHash", "built:     $builtHash", "running: $running"
            $okay = $installedHash -eq $builtHash -and $running
        }
        $lines | Set-Content -LiteralPath $log -Encoding UTF8
        $steps.Add([pscustomobject][ordered]@{ name = 'installed'; passed = $okay; exitCode = [int](-not $okay); seconds = 0; log = $log })
        $mark = if ($okay) { 'passed' } else { 'FAILED' }
        Write-Host "[installed] $mark"
        if (-not $okay) { $lines | ForEach-Object { Write-Host "  $_" } }
    }
}

$passed = @($steps | Where-Object { -not $_.passed }).Count -eq 0
$summary = [ordered]@{ tier = $Tier; installed = [bool]$Installed; passed = $passed; started = $stamp
    commit = (& git -C $root rev-parse --short HEAD); dirty = [bool](& git -C $root status --porcelain); steps = $steps }
$summaryPath = Join-Path $evidence 'summary.json'
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host ("{0} {1}: {2}" -f $Tier, $(if ($passed) { 'passed' } else { 'FAILED' }), $summaryPath)
if (-not $passed) { exit 1 }
