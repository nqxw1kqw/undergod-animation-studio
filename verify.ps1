param(
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [string]$DxLibDir='C:\DxLib'
)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -DxLibDir $DxLibDir
$runtime=Join-Path $PSScriptRoot "bin\$Configuration"
Push-Location $runtime
try {
    & (Join-Path $runtime 'PortableChecks.exe')
    if ($LASTEXITCODE -ne 0) { throw 'PortableChecks failed.' }
    $p=Start-Process -FilePath (Join-Path $runtime 'UndergodAnimationStudio.exe') -ArgumentList '--smoke' -WorkingDirectory $runtime -WindowStyle Hidden -PassThru
    if (!$p.WaitForExit(30000)) { Stop-Process -Id $p.Id; throw 'Smoke test exceeded 30 seconds.' }
    Get-Content -LiteralPath (Join-Path $runtime 'artifacts\smoke.txt')
    if ($p.ExitCode -ne 0) { throw "Studio smoke failed: $($p.ExitCode)" }
} finally { Pop-Location }
