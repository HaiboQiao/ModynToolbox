param([Parameter(Mandatory=$true)][string]$PackageRoot)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$package=(Resolve-Path -LiteralPath $PackageRoot).Path
$allowed=[IO.Path]::GetFullPath((Join-Path $projectRoot '.local/releases'))+[IO.Path]::DirectorySeparatorChar
if (-not ($package+[IO.Path]::DirectorySeparatorChar).StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Signing is restricted to project-generated release folders.' }
$files=@('ModynToolbox.exe','app/ModynToolbox.App.exe','app/ModynToolbox.WorkerHost.exe')
# Cross-version hosts can expose duplicate Security type data. Load PS5's own binary module.
if ($PSVersionTable.PSEdition -eq 'Desktop') {
 $nativeSecurity=[Reflection.Assembly]::Load('Microsoft.PowerShell.Security, Version=3.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35')
 Import-Module $nativeSecurity.Location -Force -ErrorAction Stop
} else { Import-Module Microsoft.PowerShell.Security -ErrorAction Stop }

$thumb=$env:MODYN_SIGN_CERT_SHA1
if ($thumb) {
 $thumb=($thumb -replace '\s','').ToUpperInvariant()
 if ($thumb -notmatch '^[A-F0-9]{40}$') {throw 'MODYN_SIGN_CERT_SHA1 must identify a certificate already in CurrentUser/My.'}
 $certificate=Get-Item -LiteralPath ('Cert:\CurrentUser\My\'+$thumb)
 $eku=@($certificate.Extensions | Where-Object { $_.Oid.Value -eq '2.5.29.37' } | ForEach-Object { $_.EnhancedKeyUsages } | ForEach-Object { $_.Value })
 if (-not $certificate.HasPrivateKey -or $certificate.NotAfter -lt (Get-Date) -or $certificate.NotBefore -gt (Get-Date) -or -not ($eku -contains '1.3.6.1.5.5.7.3.3')) {throw 'A current code-signing certificate with a private key is required.'}
 $sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
 $signTool=Get-ChildItem -Path (Join-Path $sdk '*/x64/signtool.exe') | Sort-Object FullName | Select-Object -Last 1
 if (-not $signTool) {throw 'Windows SDK SignTool is missing.'}
 foreach($relative in $files) {
  & $signTool.FullName sign /sha1 $thumb /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 (Join-Path $package $relative)
  if ($LASTEXITCODE -ne 0) {throw 'SignTool failed; do not publish this package.'}
 }
}
$entries=@()
foreach($relative in $files) {
 $signature=Get-AuthenticodeSignature -LiteralPath (Join-Path $package $relative)
 if ($thumb -and ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -ne $thumb)) {throw 'Signature verification failed; do not publish.'}
 $entries+=@{file=$relative;status=[string]$signature.Status;publisher=$(if($signature.SignerCertificate){$signature.SignerCertificate.Subject}else{$null})}
}
$report=@{files=$entries;certificate_configured=[bool]$thumb;smartscreen_guaranteed=$false;note='Trusted signing verifies publisher identity; SmartScreen reputation is separate. No system protections or Mark-of-the-Web are changed.'}
[IO.File]::WriteAllText((Join-Path $package 'signing-report.json'),($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
Write-Output $(if($thumb){'Host signatures verified.'}else{'No signing identity configured: host signature status recorded without changing system protection.'})
