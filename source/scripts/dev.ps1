param(
    [ValidateSet('doctor','configure','build','run','test','tools','package')]
    [string]$Action = 'run',
    [ValidateSet('windows-dev','windows-check','windows-complete','windows-release')]
    [string]$Preset = 'windows-complete'
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
if ($env:MODYN_QT_ROOT) { $qtRoot = $env:MODYN_QT_ROOT }
if (-not (Test-Path -LiteralPath (Join-Path $qtRoot 'lib\cmake\Qt6\Qt6Config.cmake'))) { throw 'Qt SDK missing. Set MODYN_QT_ROOT or install the project Qt 6.8.3 SDK.' }
if (-not (Test-Path -LiteralPath $cmake)) { throw 'Visual Studio CMake component is required.' }
$env:MODYN_QT_ROOT = $qtRoot
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
    $configuration = if ($Preset -eq "windows-release") { "Release" } else { "Debug" }
    $binaryRoot = Join-Path $projectRoot (".local\build\" + $Preset + "\bin\" + $configuration)
    $deploy = Join-Path $qtRoot 'bin\windeployqt.exe'
    Invoke-Checked $deploy @($(if ($configuration -eq 'Release') { '--release' } else { '--debug' }),'--no-translations',(Join-Path $binaryRoot 'ModynToolbox.App.exe'))
    foreach ($module in @('Test','Concurrent','Sql')) {
        $runtimeName='Qt6'+$module+$(if ($configuration -eq 'Debug') {'d'} else {''})+'.dll'
        Copy-Item -LiteralPath (Join-Path $qtRoot ('bin\'+$runtimeName)) -Destination $binaryRoot -Force
    }
    Copy-Item -LiteralPath (Join-Path $qtRoot $(if ($configuration -eq 'Release') { 'plugins\platforms\qoffscreen.dll' } else { 'plugins\platforms\qoffscreend.dll' })) -Destination (Join-Path $binaryRoot 'platforms') -Force
    if ($Action -eq 'test') {
        Invoke-Checked $ctest @('--preset',$Preset)
    }
    if ($Action -eq 'tools') {
        Invoke-Checked 'python' @((Join-Path $projectRoot 'scripts\prepare-tools.py'),'--binary',(Join-Path $binaryRoot 'ModynToolbox.App.exe'))
    }
    if ($Action -eq 'run') {
        & (Join-Path $binaryRoot 'ModynToolbox.App.exe') '--data-root' (Join-Path $projectRoot 'data') '--packages-root' (Join-Path $projectRoot '.local\installed\tools')
        if ($LASTEXITCODE -ne 0) { throw "Application exited with $LASTEXITCODE" }
    }
    if ($Action -eq 'package') {
        if ($configuration -ne 'Release') { throw 'Use -Preset windows-release for packaging.' }
        Invoke-Checked 'python' @((Join-Path $projectRoot 'scripts\prepare-tools.py'),'--binary',(Join-Path $binaryRoot 'ModynToolbox.App.exe'))
        Invoke-Checked 'python' @((Join-Path $projectRoot 'scripts\package.py'),'--binary-root',$binaryRoot,'--qt-root',$qtRoot)
    }
} finally {
    Pop-Location
}
