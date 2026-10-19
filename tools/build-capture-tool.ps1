param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$compilerRoot = Join-Path $PSScriptRoot 'local\msvc\VC\Tools\MSVC\14.44.35207'
$sdkRoot = Join-Path $PSScriptRoot 'local\sdk\c'
$sdkLibRoot = Join-Path $PSScriptRoot 'local\sdk-x64\c'
$sdkVersion = '10.0.26100.0'
$compiler = Join-Path $compilerRoot 'bin\Hostx64\x64\cl.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Run tools/bootstrap-toolchain.ps1 first.' }
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
try {
    $env:INCLUDE = (@((Join-Path $compilerRoot 'include')) + @('ucrt','shared','um','winrt','cppwinrt') | ForEach-Object {
        if ([IO.Path]::IsPathRooted($_)) { $_ } else { Join-Path $sdkRoot "Include\$sdkVersion\$_" }
    }) -join ';'
    $env:LIB = @((Join-Path $compilerRoot 'lib\x64'),(Join-Path $sdkLibRoot 'um\x64'),(Join-Path $sdkLibRoot 'ucrt\x64')) -join ';'
    $buildRoot = Join-Path $projectRoot 'build\Release'
    New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
    & $compiler /nologo /std:c++20 /utf-8 /EHsc /MT /W4 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x0A00 `
        "$PSScriptRoot\capture-window.cpp" "/Fo$buildRoot\capture-window.obj" "/Fe$buildRoot\capture-window.exe" `
        /link d3d11.lib dxgi.lib windowsapp.lib user32.lib ole32.lib windowscodecs.lib
    if ($LASTEXITCODE -ne 0) { throw 'Window capture helper compilation failed.' }
    Write-Output "Built: $buildRoot\capture-window.exe"
} finally {
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
