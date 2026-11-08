param(
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [string]$DxLibDir='C:\DxLib'
)
$ErrorActionPreference='Stop'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 with Desktop development with C++.' }
$msbuild=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild not found.' }
if (!(Test-Path -LiteralPath (Join-Path $DxLibDir 'DxLib.h'))) { throw "DxLib.h not found in $DxLibDir" }
& $msbuild (Join-Path $PSScriptRoot 'UndergodAnimationStudio.sln') "/p:Configuration=$Configuration" /p:Platform=x64 "/p:DxLibDir=$DxLibDir" /m /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
Write-Output (Join-Path $PSScriptRoot "bin\$Configuration\UndergodAnimationStudio.exe")
