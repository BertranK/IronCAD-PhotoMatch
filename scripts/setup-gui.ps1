$ErrorActionPreference='Stop'
$protoRoot=Split-Path -Parent $PSScriptRoot
$runtime=Join-Path $protoRoot 'gui\.runtime'
New-Item -ItemType Directory -Force $runtime | Out-Null
$env:TEMP=$runtime;$env:TMP=$runtime;$env:npm_config_cache=Join-Path $runtime 'npm-cache'
if(!(Test-Path -LiteralPath (Join-Path $protoRoot '.venv\Scripts\python.exe'))){
    & python -m venv (Join-Path $protoRoot '.venv')
    if($LASTEXITCODE -ne 0){throw 'Python virtual environment creation failed.'}
}
& (Join-Path $protoRoot '.venv\Scripts\python.exe') -m pip install --no-cache-dir -r (Join-Path $protoRoot 'gui\requirements.txt')
if($LASTEXITCODE -ne 0){throw 'GUI dependency installation failed.'}
Push-Location (Join-Path $protoRoot 'gui\web')
try {
    & npm.cmd ci --no-audit --no-fund
    if($LASTEXITCODE -ne 0){throw 'CSS dependency installation failed.'}
    & npm.cmd run build
    if($LASTEXITCODE -ne 0){throw 'CSS build failed.'}
} finally {Pop-Location}
Write-Output 'PhotoMatch GUI ready. Run scripts/start-gui.ps1 or use the IronCAD PhotoMatch command.'
