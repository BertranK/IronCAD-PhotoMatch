$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$python=Join-Path $root '.venv\Scripts\python.exe'
& $python -m pip install --disable-pip-version-check -r "$root\packaging\requirements-build.txt"
if ($LASTEXITCODE -ne 0) { throw 'Package build dependencies failed.' }
& "$PSScriptRoot\build.ps1"
& "$PSScriptRoot\test-gui.ps1"
& "$root\tests\PackagingTests.ps1"
& $python -m PyInstaller --noconfirm --distpath "$root\build\frozen" --workpath "$root\build\pyinstaller" "$root\packaging\PhotoMatch.spec"
if ($LASTEXITCODE -ne 0) { throw 'GUI freezing failed.' }
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$package=Join-Path $root "build\packages\PhotoMatch-2027-$stamp"
New-Item -ItemType Directory -Path "$package\scripts","$package\build\v143","$package\evidence" -Force | Out-Null
Copy-Item -LiteralPath "$root\build\frozen\PhotoMatch" -Destination $package -Recurse
Copy-Item -LiteralPath "$root\build\v143\PhotoMatchProto.dll" -Destination "$package\build\v143"
foreach ($file in 'register.ps1','configure-host.ps1','configure-manifest.ps1') {
    Copy-Item -LiteralPath "$PSScriptRoot\$file" -Destination "$package\scripts"
}
foreach ($file in 'Install.cmd','Install.ps1','README.txt') {
    Copy-Item -LiteralPath "$root\packaging\$file" -Destination $package
}
& $python "$root\packaging\licenses.py" "$package\THIRD-PARTY-NOTICES"
if ($LASTEXITCODE -ne 0) { throw 'License collection failed.' }
$report=Join-Path $root "build\package-check-$stamp.json"
$process=Start-Process -FilePath "$package\PhotoMatch\PhotoMatch.exe" -ArgumentList ('--smoke-test "'+$report+'"') -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(60000)) { $process.Kill(); throw 'Frozen GUI check timed out.' }
if (!(Test-Path -LiteralPath $report) -or !(Get-Content -LiteralPath $report -Raw | ConvertFrom-Json).ok) { throw 'Frozen GUI check failed.' }
$zip="$package.zip"
Compress-Archive -LiteralPath $package -DestinationPath $zip
(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash | Set-Content -LiteralPath "$zip.sha256"
Write-Output "Verified package: $zip"
