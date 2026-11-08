$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
foreach ($id in @('json','image','rename')) {
    $source = Join-Path $projectRoot "tools\$id\integration\manifest.json"
    $manifest = Get-Content -LiteralPath $source -Raw -Encoding UTF8 | ConvertFrom-Json
    $target = Join-Path $projectRoot ".local\installed\tools\$id\$($manifest.package_version)"
    if (Test-Path -LiteralPath $target) {
        Write-Output "Keeping existing package $id"
        continue
    }
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination (Join-Path $target 'manifest.json')
    Write-Output "Prepared metadata preview $id"
}
