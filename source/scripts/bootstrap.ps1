param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$venv = Join-Path $root '.local\bootstrap\venv'
& $Python -m venv $venv
if ($LASTEXITCODE -ne 0) { throw 'Could not create isolated bootstrap environment.' }
$pythonExe = Join-Path $venv 'Scripts\python.exe'
& $pythonExe -m pip install -r (Join-Path $PSScriptRoot 'bootstrap.lock.txt')
if ($LASTEXITCODE -ne 0) { throw 'Bootstrap dependencies failed.' }
Push-Location (Join-Path $root '.local\bootstrap')
try {
    & $pythonExe -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --archives qtbase qttools -O (Join-Path $root '.local\toolchains\Qt')
    if ($LASTEXITCODE -ne 0) { throw 'Qt SDK installation failed.' }
} finally { Pop-Location }
