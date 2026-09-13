$ErrorActionPreference='Stop'
$installPath='D:\BuildTools\VisualStudio2022'
$tempPath='D:\BuildTools\Temp'
New-Item -ItemType Directory -Path $tempPath -Force | Out-Null
$previousTemp=$env:TEMP
$previousTmp=$env:TMP
try {
    $env:TEMP=$tempPath
    $env:TMP=$tempPath
    $options='--quiet --wait --norestart --installPath "'+$installPath+'" --nocache --add Microsoft.Component.MSBuild --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.VC.ATLMFC'
    winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --silent --accept-source-agreements --accept-package-agreements --override $options
    if ($LASTEXITCODE -ne 0) { throw "Build Tools installation failed: $LASTEXITCODE" }
} finally {
    $env:TEMP=$previousTemp
    $env:TMP=$previousTmp
}
