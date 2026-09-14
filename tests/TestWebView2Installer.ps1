param([Parameter(Mandatory=$true)][string]$Compiler)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$output=Join-Path $root 'build\installer-tests'
& $Compiler '/Qp' "/O$output" "$PSScriptRoot\WebView2InstallerTests.iss"
if ($LASTEXITCODE -ne 0) { throw 'WebView2 prerequisite test compilation failed.' }
$report=Join-Path $output ('result-'+[guid]::NewGuid().ToString('N')+'.txt')
$process=Start-Process -FilePath "$output\WebView2InstallerTests.exe" -ArgumentList ('/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /REPORT="'+$report+'"') -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'WebView2 prerequisite tests timed out.' }
if (!(Test-Path -LiteralPath $report) -or (Get-Content -LiteralPath $report -Raw) -ne 'passed') {
    throw 'WebView2 prerequisite tests failed.'
}
Write-Output 'WebView2 existing, missing, failed launch/install, false success, and restart tests passed.'
