param([ValidateSet('Release','Debug')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$localRoot = Join-Path $PSScriptRoot 'local'
$compilerRoot = Join-Path $localRoot 'msvc\VC\Tools\MSVC\14.44.35207'
$sdkRoot = Join-Path $localRoot 'sdk\c'
$sdkLibRoot = Join-Path $localRoot 'sdk-x64\c'
$sdkVersion = '10.0.26100.0'
$compiler = Join-Path $compilerRoot 'bin\Hostx64\x64\cl.exe'
$resourceCompiler = Join-Path $sdkRoot "bin\$sdkVersion\x64\rc.exe"
if (-not (Test-Path -LiteralPath $compiler) -or -not (Test-Path -LiteralPath $resourceCompiler)) {
    throw 'Local compiler/SDK not found. Run tools/bootstrap-toolchain.ps1, or build CMakeLists.txt from a Visual Studio C++ developer prompt.'
}
$buildRoot = Join-Path $projectRoot "build\$Configuration"
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$savedInclude=$env:INCLUDE; $savedLib=$env:LIB; $savedPath=$env:PATH
try {
    $env:INCLUDE = (@((Join-Path $compilerRoot 'include')) + @('ucrt','shared','um','winrt','cppwinrt') | ForEach-Object {
        if ([System.IO.Path]::IsPathRooted($_)) { $_ } else { Join-Path $sdkRoot "Include\$sdkVersion\$_" }
    }) -join ';'
    $env:LIB = @((Join-Path $compilerRoot 'lib\x64'),(Join-Path $sdkLibRoot 'um\x64'),(Join-Path $sdkLibRoot 'ucrt\x64')) -join ';'
    $env:PATH = "$(Split-Path $compiler);$(Split-Path $resourceCompiler);$savedPath"
    Push-Location (Join-Path $projectRoot 'packaging')
    try { & $resourceCompiler /nologo "/fo$buildRoot\app.res" app.rc; if($LASTEXITCODE -ne 0) {throw 'Resource compilation failed.'} }
    finally { Pop-Location }
    $flags=@('/nologo','/std:c++20','/utf-8','/EHsc','/MT','/W4','/DUNICODE','/D_UNICODE','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/D_WIN32_WINNT=0x0A00',"/I$projectRoot\src")
    if($Configuration -eq 'Debug') { $flags+=@('/Od','/Zi') } else { $flags+=@('/O2','/DNDEBUG') }
    $sources=@('main.cpp','appearance.cpp','renderer.cpp','settings.cpp','localization.cpp') | ForEach-Object {Join-Path $projectRoot "src\$_"}
    $libraries=@('d3d11.lib','d2d1.lib','dwrite.lib','dxgi.lib','dxguid.lib','dwmapi.lib','windowsapp.lib','CoreMessaging.lib','user32.lib','gdi32.lib','shell32.lib','ole32.lib','oleaut32.lib','propsys.lib','shlwapi.lib','comdlg32.lib','comctl32.lib','advapi32.lib','wtsapi32.lib','windowscodecs.lib','uuid.lib')
    Push-Location $buildRoot
    try {
        & $compiler @flags @sources app.res /Fe:CopilotReel.exe /link /SUBSYSTEM:WINDOWS /MANIFEST:NO @libraries
        if($LASTEXITCODE -ne 0) {throw 'Application compilation failed.'}
        & $compiler @flags "$projectRoot\tests\core_tests.cpp" /Fe:core_tests.exe
        if($LASTEXITCODE -ne 0) {throw 'Core tests compilation failed.'}
        & '.\core_tests.exe'
        if($LASTEXITCODE -ne 0) {throw 'Core tests failed.'}
        & $compiler @flags "$projectRoot\tests\settings_tests.cpp" "$projectRoot\src\localization.cpp" /Fe:settings_tests.exe /link user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib uuid.lib
        if($LASTEXITCODE -ne 0) {throw 'Settings tests compilation failed.'}
        & '.\settings_tests.exe'
        if($LASTEXITCODE -ne 0) {throw 'Settings tests failed.'}
        & $compiler @flags "$projectRoot\tests\launch_probe.cpp" /Fe:launch_probe.exe /link /SUBSYSTEM:WINDOWS user32.lib
        if($LASTEXITCODE -ne 0) {throw 'Launch probe compilation failed.'}
    } finally { Pop-Location }
    Write-Output "Built: $buildRoot\CopilotReel.exe"
} finally { $env:INCLUDE=$savedInclude; $env:LIB=$savedLib; $env:PATH=$savedPath }
