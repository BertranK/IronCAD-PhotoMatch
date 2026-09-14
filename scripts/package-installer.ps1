param([Parameter(Mandatory=$true)][string]$PackageDir,[string]$Compiler='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$PackageDir=(Resolve-Path -LiteralPath $PackageDir).Path
if (!(Test-Path -LiteralPath "$PackageDir\PhotoMatch\PhotoMatch.exe")) { throw 'Pass the verified package directory created by package.ps1.' }
if (!$Compiler) { $Compiler=Join-Path $root 'build\tools\InnoSetup\ISCC.exe' }
if (!(Test-Path -LiteralPath $Compiler)) { throw 'Install Inno Setup 6 and pass -Compiler with its ISCC.exe path. See packaging/README.txt.' }
& $Compiler '/Qp' "/DPackageDir=$PackageDir" "/O$root\build\packages" "$root\packaging\PhotoMatch.iss"
if ($LASTEXITCODE -ne 0) { throw 'Setup EXE compilation failed.' }
$exe=Join-Path $root 'build\packages\PhotoMatch-2027-Setup.exe'
(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash | Set-Content -LiteralPath "$exe.sha256"
Write-Output "Setup EXE created: $exe"
