param(
    [ValidateSet('doctor','configure','build','run','test','package')]
    [string]$Action = 'run',
    [ValidateSet('windows-dev','windows-check')]
    [string]$Preset = 'windows-dev'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer / vswhere is required.' }
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'Install Visual Studio C++ desktop tools and Windows SDK.' }
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
$qtRoot = Join-Path $projectRoot '.local\toolchains\Qt\6.8.3\msvc2022_64'
if ($env:GRAPHITE_QT_ROOT) { $qtRoot = $env:GRAPHITE_QT_ROOT }
if (-not (Test-Path -LiteralPath (Join-Path $qtRoot 'lib\cmake\Qt6\Qt6Config.cmake'))) { throw 'Qt SDK missing. Set GRAPHITE_QT_ROOT or install the project Qt 6.8.3 SDK.' }
if (-not (Test-Path -LiteralPath $cmake)) { throw 'Visual Studio CMake component is required.' }
$env:GRAPHITE_QT_ROOT = $qtRoot
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
Push-Location $projectRoot
try {
    if ($Action -eq 'doctor') {
        Write-Output "Project: $projectRoot"
        Write-Output "Visual Studio: $vsRoot"
        Write-Output "Qt: $qtRoot"
        Invoke-Checked $cmake @('--version')
        exit 0
    }
    Invoke-Checked $cmake @('--preset',$Preset)
    if ($Action -eq 'configure') { exit 0 }
    Invoke-Checked $cmake @('--build','--preset',$Preset)
    $binaryRoot = Join-Path $projectRoot (".local\build\" + $Preset + "\bin\Debug")
    $deploy = Join-Path $qtRoot 'bin\windeployqt.exe'
    Invoke-Checked $deploy @('--debug','--no-translations',(Join-Path $binaryRoot 'GraphiteToolbox.App.exe'))
    foreach ($runtime in Get-ChildItem -LiteralPath (Join-Path $qtRoot 'bin') -Filter 'Qt6Test*.dll') {
        Copy-Item -LiteralPath $runtime.FullName -Destination $binaryRoot -Force
    }
    if ($Action -eq 'test') {
        Copy-Item -LiteralPath (Join-Path $qtRoot 'plugins\platforms\qoffscreend.dll') -Destination (Join-Path $binaryRoot 'platforms') -Force
        Invoke-Checked $ctest @('--preset',$Preset)
    }
    if ($Action -eq 'run') {
        & (Join-Path $binaryRoot 'GraphiteToolbox.App.exe') '--data-root' (Join-Path $projectRoot 'data') '--packages-root' (Join-Path $projectRoot '.local\installed\tools')
        if ($LASTEXITCODE -ne 0) { throw "Application exited with $LASTEXITCODE" }
    }
    if ($Action -eq 'package') {
        throw 'Release packaging is pending the M0 launcher and release validation. Debug build completed.'
    }
} finally {
    Pop-Location
}
