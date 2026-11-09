$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$record = Join-Path $projectRoot '.local/releases/latest.json'
if (-not (Test-Path -LiteralPath $record)) { throw 'Build the release first with scripts/dev.ps1 package -Preset windows-release.' }
$release = Get-Content -Raw -Encoding UTF8 -LiteralPath $record | ConvertFrom-Json
$relativeRoot = if ($release.relative_root) { $release.relative_root } else { '.local/releases/' + (Split-Path -Leaf $release.root) }
$releaseRoot = Join-Path $projectRoot $relativeRoot
$program = Join-Path $releaseRoot 'ModynToolbox.exe'
if (-not (Test-Path -LiteralPath $program)) { throw 'Release folder was moved. Open ModynToolbox.exe in its current folder.' }
Start-Process -FilePath $program -WorkingDirectory $releaseRoot
